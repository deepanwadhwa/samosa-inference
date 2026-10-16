#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "decision_assets.h"
#include "artifact.h"
#include "engine_runtime.h"
#include "library_runtime.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
const char *decision_asset_name(unsigned i) {
    static const char *names[]={"decision-engine",
#ifdef __APPLE__
        "libonnxruntime.1.27.0.dylib",
#else
        "libonnxruntime.so.1.27.0",
#endif
        "model.onnx","tokenizer.json"};
    return i<DECISION_ASSET_COUNT?names[i]:NULL;
}
static const uint64_t ceilings[]={67108864u,67108864u,1073741824u,16777216u};
static int same(const struct stat *a,const struct stat *b) {
    if(a->st_dev!=b->st_dev||a->st_ino!=b->st_ino||a->st_size!=b->st_size||
       a->st_mode!=b->st_mode||a->st_uid!=b->st_uid||a->st_nlink!=b->st_nlink)return 0;
#ifdef __APPLE__
    return a->st_mtimespec.tv_sec==b->st_mtimespec.tv_sec&&a->st_mtimespec.tv_nsec==b->st_mtimespec.tv_nsec&&
        a->st_ctimespec.tv_sec==b->st_ctimespec.tv_sec&&a->st_ctimespec.tv_nsec==b->st_ctimespec.tv_nsec;
#else
    return a->st_mtim.tv_sec==b->st_mtim.tv_sec&&a->st_mtim.tv_nsec==b->st_mtim.tv_nsec&&
        a->st_ctim.tv_sec==b->st_ctim.tv_sec&&a->st_ctim.tv_nsec==b->st_ctim.tv_nsec;
#endif
}
static const char *tree(int root,const char *(*control)(void *),void *context) {
    // Use an independent directory description: dup would share/reuse readdir's
    // offset and could make a later inspection skip namespace entries.
    int fd=openat(root,".",O_RDONLY|O_DIRECTORY|O_CLOEXEC);
    if(fd<0)return "artifact_unavailable";
    DIR *directory=fdopendir(fd);if(!directory){close(fd);return "artifact_unavailable";}
    const char *error=NULL;unsigned found=0;struct dirent *entry;
    for(;;) {
        error=control?control(context):NULL;if(error)break;
        errno=0;entry=readdir(directory);
        if(!entry){if(errno)error="artifact_unavailable";break;}
        if(!strcmp(entry->d_name,".")||!strcmp(entry->d_name,".."))continue;
        unsigned i;for(i=0;i<DECISION_ASSET_COUNT;i++)if(!strcmp(entry->d_name,decision_asset_name(i)))break;
        if(i==DECISION_ASSET_COUNT||(found&(1u<<i))){error="unqualified_decision_assets";break;}
        found|=1u<<i;
    }
    closedir(directory);
    return error?error:found==((1u<<DECISION_ASSET_COUNT)-1)?NULL:"unqualified_decision_assets";
}
static const char *file_check(const DecisionAssets *assets,unsigned i,int compare,
    const char *(*control)(void *),void *context,struct stat *receipt) {
    const char *error=control?control(context):NULL;struct stat before,named,after;
    if(!error&&(fstat(assets->files[i],&before)||fstatat(assets->root,decision_asset_name(i),&named,AT_SYMLINK_NOFOLLOW)))
        error="artifact_unavailable";
    if(!error&&(!S_ISREG(before.st_mode)||before.st_uid!=geteuid()||before.st_nlink!=1||
       (before.st_mode&022)||before.st_size<=0||(i==DECISION_ENGINE&&!(before.st_mode&0111))))error="unsupported_artifact";
    if(!error&&(!same(&before,&named)||(compare&&!same(&before,&assets->identities[i]))))error="artifact_changed";
    uint64_t ceiling=i==DECISION_MODEL&&assets->profile==DECISION_ASSETS_IMAGE?2147483648ULL:ceilings[i];
    if(!error)error=artifact_verify_controlled(assets->files[i],(uint64_t)before.st_size,ceiling,assets->pins[i],control,context);
    if(!error&&(fstat(assets->files[i],&after)||!same(&before,&after)))error="artifact_changed";
    if(!error&&receipt)*receipt=after;
    return error;
}
void decision_assets_close(DecisionAssets *assets) {
    if(!assets||!assets->active)return;
    for(unsigned i=0;i<DECISION_ASSET_COUNT;i++)if(assets->files[i]>=0)close(assets->files[i]);
    if(assets->root>=0)close(assets->root);
    memset(assets,0,sizeof(*assets));
}
const char *decision_assets_recheck(const DecisionAssets *assets,const char *(*control)(void *),void *context) {
    if(!assets||!assets->active||assets->profile>DECISION_ASSETS_IMAGE)return "invalid_decision_configuration";
    const char *error=control?control(context):NULL;struct stat held,named;
    if(!error&&(fstat(assets->root,&held)||lstat(assets->path,&named)))error="artifact_unavailable";
    if(!error&&(!same(&held,&assets->directory)||!same(&held,&named)))error="artifact_changed";
    if(!error)error=tree(assets->root,control,context);
    for(unsigned i=0;!error&&i<DECISION_ASSET_COUNT;i++)error=file_check(assets,i,1,control,context,NULL);
    if(!error&&(fstat(assets->root,&held)||lstat(assets->path,&named)||
       !same(&held,&assets->directory)||!same(&held,&named)))error="artifact_changed";
    return error;
}
const char *decision_assets_open(const char *path,const char *const pins[DECISION_ASSET_COUNT],
    const char *(*control)(void *),void *context,DecisionAssets *out) {
    return decision_assets_open_profile(path,pins,DECISION_ASSETS_TEXT,control,context,out);
}
const char *decision_assets_open_profile(const char *path,const char *const pins[DECISION_ASSET_COUNT],
    unsigned profile,const char *(*control)(void *),void *context,DecisionAssets *out) {
    if(!out||out->active||!path||path[0]!='/'||strlen(path)>=sizeof(out->path)||!pins||profile>DECISION_ASSETS_IMAGE)
        return "invalid_decision_configuration";
    DecisionAssets candidate={.active=1,.root=-1,.files={-1,-1,-1,-1}};
    candidate.profile=profile;
    for(unsigned i=0;i<DECISION_ASSET_COUNT;i++) {
        if(!pins[i]||strlen(pins[i])!=64||strspn(pins[i],"0123456789abcdef")!=64)return "invalid_decision_configuration";
        memcpy(candidate.pins[i],pins[i],65);
    }
    strcpy(candidate.path,path);
    const char *error=control?control(context):NULL;
    if(!error)error=engine_runtime_environment();
    if(!error){candidate.root=open(path,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
        if(candidate.root<0||fstat(candidate.root,&candidate.directory)||!S_ISDIR(candidate.directory.st_mode)||
           candidate.directory.st_uid!=geteuid()||(candidate.directory.st_mode&077))error="unsupported_artifact";}
    if(!error)error=tree(candidate.root,control,context);
    for(unsigned i=0;!error&&i<DECISION_ASSET_COUNT;i++) {
        candidate.files[i]=openat(candidate.root,decision_asset_name(i),O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
        error=candidate.files[i]<0?"artifact_unavailable":file_check(&candidate,i,0,control,context,&candidate.identities[i]);
    }
    LibraryRuntimePolicy policy;LibraryRuntimeImage images[2];
    if(!error)error=library_runtime_host_policy(&policy);
    if(!error)error=library_runtime_inspect_executable(candidate.files[DECISION_ENGINE],&policy,&images[0]);
    if(!error)error=library_runtime_inspect(candidate.files[DECISION_RUNTIME],&policy,&images[1]);
    if(!error){int linked=0;
        for(size_t i=0;i<images[0].count;i++) {
            const char *name=images[0].dependencies[i].name;
            if(!strncmp(name,"@rpath/",7))name+=7;
            else if(!strncmp(name,"@loader_path/",13))name+=13;
            if(!strcmp(name,decision_asset_name(DECISION_RUNTIME)))linked=1;
        }
        if(!linked)error="unqualified_decision_assets";
    }
    if(!error){const LibraryRuntimeFile files[]={
        {decision_asset_name(DECISION_ENGINE),LIBRARY_RUNTIME_EXECUTABLE},
        {decision_asset_name(DECISION_RUNTIME),LIBRARY_RUNTIME_LIBRARY}};
        error=library_runtime_graph(&policy,files,images,2,control,context);}
    if(!error)error=decision_assets_recheck(&candidate,control,context);
    if(error)decision_assets_close(&candidate);else *out=candidate;
    return error;
}
