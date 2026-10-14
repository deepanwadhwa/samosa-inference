#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "package_tree.h"
#include "sha256.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"package tree failure %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void write_file(int root,const char *name,const char *body){
    int fd=openat(root,name,O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW,0600);CHECK(fd>=0);
    CHECK(write(fd,body,strlen(body))==(ssize_t)strlen(body));CHECK(!close(fd));
}
static PackageIndex *index_make(void){
    char *body=NULL;size_t length=0;FILE *f=open_memstream(&body,&length);CHECK(f);
    fputs("{\"format\":\"samosa.inference.package.v1\",\"platform\":\"macos-arm64\",\"minimum_os\":\"14.0\",\"recipe\":\"fixture-v1\",\"files\":[",f);
    const char *names[]={"models/model.gguf","native.so","data/voices/!v/Mr serious"};
    const char *roles[]={"model","library","data"},*payloads[]={"abc","def","ghi"};
    for(unsigned i=0;i<3;i++){char sha[65];hash_bytes(payloads[i],3,sha);
        fprintf(f,"%s{\"name\":\"%s\",\"role\":\"%s\",\"bytes\":3,\"sha256\":\"%s\"}",i?",":"",names[i],roles[i],sha);}
    fputs("]}",f);CHECK(!fclose(f));char pin[65];hash_bytes(body,length,pin);
    PackageIndex *index=malloc(sizeof(*index));CHECK(index);CHECK(!package_index_parse(body,length,pin,index));free(body);return index;
}
static void fails(int root,const PackageIndex *index,const char *expected){
    PackageTreeResult result,before;memset(&result,42,sizeof(result));memcpy(&before,&result,sizeof(result));
    const char *e=package_tree_verify(root,index,NULL,NULL,&result);CHECK(e);if(strcmp(e,expected))fprintf(stderr,"got %s, expected %s\n",e,expected);
    CHECK(!strcmp(e,expected));CHECK(!memcmp(&result,&before,sizeof(result)));
}
static const char *cancel(void *unused){(void)unused;return "model_cancelled";}
typedef struct {int root,changed;} Mutation;
static const char *mutate(void *context){Mutation *m=context;if(!m->changed){m->changed=1;write_file(m->root,"models/model.gguf","abd");}return NULL;}
int main(int argc,char **argv){
    if(argc==5&&(!strcmp(argv[1],"--verify")||!strcmp(argv[1],"--snapshot"))){
        int input=open(argv[3],O_RDONLY|O_NOFOLLOW);struct stat s;CHECK(input>=0&&!fstat(input,&s));
        CHECK(S_ISREG(s.st_mode)&&s.st_uid==geteuid()&&!(s.st_mode&077)&&s.st_nlink==1&&s.st_size>0&&s.st_size<=PACKAGE_INDEX_BYTES);
        char *body=malloc((size_t)s.st_size);PackageIndex *index=malloc(sizeof(*index));CHECK(body&&index);
        size_t at=0;while(at<(size_t)s.st_size){ssize_t n=read(input,body+at,(size_t)s.st_size-at);CHECK(n>0);at+=(size_t)n;}
        const char *e=package_index_parse(body,at,argv[4],index);CHECK(!close(input));free(body);
        int root=open(argv[2],O_RDONLY|O_DIRECTORY|O_NOFOLLOW);PackageTreeResult result;
        if(!e&&!strcmp(argv[1],"--snapshot")){PackageTreeSnapshot *snapshot=malloc(sizeof(*snapshot));CHECK(snapshot);e=package_tree_snapshot(root,index,NULL,NULL,snapshot);
            if(!e){result=(PackageTreeResult){.files=snapshot->file_count,.directories=snapshot->directory_count,.payload_bytes=index->payload_bytes};strcpy(result.manifest_sha256,snapshot->manifest_sha256);}
            free(snapshot);
        }else if(!e)e=package_tree_verify(root,index,NULL,NULL,&result);
        if(root>=0)close(root);free(index);
        if(e){fprintf(stderr,"actual package tree: %s\n",e);return 1;}
        printf("PASS pinned actual package tree: %zu files, %zu directories, %llu payload bytes, index %s; integrity observation only\n",
            result.files,result.directories,(unsigned long long)result.payload_bytes,result.manifest_sha256);return 0;
    }
    CHECK(argc==1);
    char base[]="/tmp/samosa-package-tree.XXXXXX";CHECK(mkdtemp(base));
    int root=open(base,O_RDONLY|O_DIRECTORY|O_NOFOLLOW);CHECK(root>=0);
    const char *directories[]={"models","data","data/voices","data/voices/!v"};
    for(unsigned i=0;i<4;i++)CHECK(!mkdirat(root,directories[i],0700));
    write_file(root,"models/model.gguf","abc");write_file(root,"native.so","def");write_file(root,"data/voices/!v/Mr serious","ghi");
    PackageIndex *index=index_make();PackageTreeResult result;
    CHECK(!package_tree_verify(root,index,NULL,NULL,&result));CHECK(result.files==3&&result.directories==4&&result.payload_bytes==9);
    CHECK(!strcmp(result.manifest_sha256,index->manifest_sha256));CHECK(!package_tree_verify(root,index,NULL,NULL,&result));
    PackageTreeSnapshot *snapshot=malloc(sizeof(*snapshot)),*before_snapshot=malloc(sizeof(*before_snapshot));CHECK(snapshot&&before_snapshot);
    memset(snapshot,0xa5,sizeof(*snapshot));memcpy(before_snapshot,snapshot,sizeof(*snapshot));
    CHECK(!strcmp(package_tree_snapshot(root,index,cancel,NULL,snapshot),"model_cancelled"));CHECK(!memcmp(snapshot,before_snapshot,sizeof(*snapshot)));
    CHECK(!package_tree_snapshot(root,index,NULL,NULL,snapshot));CHECK(snapshot->file_count==3&&snapshot->directory_count==4&&!strcmp(snapshot->manifest_sha256,index->manifest_sha256));
    struct stat observed;CHECK(!fstat(root,&observed));CHECK(snapshot->root.st_dev==observed.st_dev&&snapshot->root.st_ino==observed.st_ino);
    for(size_t i=0;i<index->count;i++){CHECK(!fstatat(root,index->files[i].name,&observed,AT_SYMLINK_NOFOLLOW));CHECK(snapshot->files[i].st_dev==observed.st_dev&&snapshot->files[i].st_ino==observed.st_ino&&snapshot->files[i].st_size==3&&snapshot->files[i].st_nlink==1);}
    for(size_t i=0;i<snapshot->directory_count;i++){CHECK(!fstatat(root,snapshot->directories[i].name,&observed,AT_SYMLINK_NOFOLLOW));CHECK(snapshot->directories[i].identity.st_dev==observed.st_dev&&snapshot->directories[i].identity.st_ino==observed.st_ino&&S_ISDIR(snapshot->directories[i].identity.st_mode));}
    memcpy(before_snapshot,snapshot,sizeof(*snapshot));write_file(root,"models/model.gguf","abd");CHECK(!strcmp(package_tree_snapshot(root,index,NULL,NULL,snapshot),"artifact_checksum_mismatch"));CHECK(!memcmp(snapshot,before_snapshot,sizeof(*snapshot)));write_file(root,"models/model.gguf","abc");free(snapshot);free(before_snapshot);
    write_file(root,"extra","unknown");fails(root,index,"unlisted_package_artifact");CHECK(!unlinkat(root,"extra",0));
    CHECK(!mkdirat(root,"empty",0700));fails(root,index,"unlisted_package_artifact");CHECK(!unlinkat(root,"empty",AT_REMOVEDIR));
    CHECK(!symlinkat("models",root,"alias"));fails(root,index,"unsupported_package_artifact");CHECK(!unlinkat(root,"alias",0));
    CHECK(!mkfifoat(root,"pipe",0600));fails(root,index,"unsupported_package_artifact");CHECK(!unlinkat(root,"pipe",0));
    char alias[512];snprintf(alias,sizeof(alias),"%s.link",base);
    CHECK(!linkat(root,"models/model.gguf",AT_FDCWD,alias,0));fails(root,index,"unsupported_package_artifact");CHECK(!unlink(alias));
    CHECK(!fchmodat(root,"native.so",0644,0));fails(root,index,"unsupported_package_artifact");CHECK(!fchmodat(root,"native.so",0600,0));
    CHECK(!fchmod(root,0755));fails(root,index,"unsupported_package_root");CHECK(!fchmod(root,0700));
    CHECK(!unlinkat(root,"native.so",0));fails(root,index,"package_file_missing");write_file(root,"native.so","def");
    CHECK(!unlinkat(root,"native.so",0));CHECK(!symlinkat("models/model.gguf",root,"native.so"));fails(root,index,"unsupported_package_artifact");
    CHECK(!unlinkat(root,"native.so",0));write_file(root,"native.so","def");
    write_file(root,"models/model.gguf","ab");fails(root,index,"artifact_size_mismatch");
    write_file(root,"models/model.gguf","abd");fails(root,index,"artifact_checksum_mismatch");write_file(root,"models/model.gguf","abc");
    PackageTreeResult before;memset(&result,77,sizeof(result));memcpy(&before,&result,sizeof(result));
    CHECK(!strcmp(package_tree_verify(root,index,cancel,NULL,&result),"model_cancelled"));CHECK(!memcmp(&result,&before,sizeof(result)));
    Mutation mutation={.root=root};CHECK(!strcmp(package_tree_verify(root,index,mutate,&mutation,&result),"artifact_checksum_mismatch"));CHECK(!memcmp(&result,&before,sizeof(result)));
    write_file(root,"models/model.gguf","abc");CHECK(!package_tree_verify(root,index,NULL,NULL,&result));
    /* Exact namespace spelling matters even on case-insensitive filesystems. */
    CHECK(!renameat(root,"native.so",root,"NATIVE.so"));fails(root,index,"unlisted_package_artifact");CHECK(!renameat(root,"NATIVE.so",root,"native.so"));
    CHECK(!unlinkat(root,"models/model.gguf",0));CHECK(!unlinkat(root,"native.so",0));CHECK(!unlinkat(root,"data/voices/!v/Mr serious",0));
    for(int i=3;i>=0;i--)CHECK(!unlinkat(root,directories[i],AT_REMOVEDIR));CHECK(!close(root));CHECK(!rmdir(base));free(index);
    printf("PASS closed package tree: %u checks; complete hashes, missing/unlisted nodes, modes, symlink/hardlink/case aliases, controls and mutation; no installation/readiness claim\n",checks);
    return 0;
}
