#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "package_tree.h"
#include "artifact.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#define TREE_DIRECTORIES (PACKAGE_INDEX_FILES * 7u)
typedef struct {char name[256];struct stat identity;} Directory;
typedef struct {
    const PackageIndex *index;const char *(*control)(void *);void *context;
    struct stat root,files[PACKAGE_INDEX_FILES];Directory directories[TREE_DIRECTORIES];
    size_t directory_count,visited;unsigned char found[PACKAGE_INDEX_FILES];int recheck,sync;
} Tree;
static int same(const struct stat *a,const struct stat *b){
    if(a->st_dev!=b->st_dev||a->st_ino!=b->st_ino||a->st_uid!=b->st_uid||a->st_mode!=b->st_mode||
       a->st_nlink!=b->st_nlink||a->st_size!=b->st_size)return 0;
#ifdef __APPLE__
    return a->st_mtimespec.tv_sec==b->st_mtimespec.tv_sec&&a->st_mtimespec.tv_nsec==b->st_mtimespec.tv_nsec&&
        a->st_ctimespec.tv_sec==b->st_ctimespec.tv_sec&&a->st_ctimespec.tv_nsec==b->st_ctimespec.tv_nsec;
#else
    return a->st_mtim.tv_sec==b->st_mtim.tv_sec&&a->st_mtim.tv_nsec==b->st_mtim.tv_nsec&&
        a->st_ctim.tv_sec==b->st_ctim.tv_sec&&a->st_ctim.tv_nsec==b->st_ctim.tv_nsec;
#endif
}
static int private(const Tree *tree,const struct stat *s){return s->st_uid==geteuid()&&!(s->st_mode&077)&&s->st_dev==tree->root.st_dev;}
static const char *regular(int parent,const char *leaf,const char *path,const struct stat *named,Tree *tree){
    size_t i=0;while(i<tree->index->count&&strcmp(path,tree->index->files[i].name))i++;
    if(i==tree->index->count)return "unlisted_package_artifact";
    if(tree->found[i]||!private(tree,named)||named->st_nlink!=1)return "unsupported_package_artifact";
    for(size_t other=0;other<tree->index->count;other++)if(tree->found[other]&&
        tree->files[other].st_dev==named->st_dev&&tree->files[other].st_ino==named->st_ino)return "unsupported_package_artifact";
    int fd=openat(parent,leaf,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);struct stat before,after,rebound;
    const char *e=fd<0||fstat(fd,&before)?"package_tree_changed":NULL;
    if(!e&&(!same(named,&before)||(tree->recheck&&!same(&before,&tree->files[i]))))e="package_tree_changed";
    const PackageFile *file=&tree->index->files[i];
    if(!e&&!tree->recheck)e=artifact_verify_controlled(fd,file->bytes,PACKAGE_PAYLOAD_BYTES,file->sha256,tree->control,tree->context);
    if(!e&&tree->sync&&!tree->recheck&&fsync(fd))e="package_sync_failed";
    if(!e&&(fstat(fd,&after)||fstatat(parent,leaf,&rebound,AT_SYMLINK_NOFOLLOW)||!same(&before,&after)||!same(&after,&rebound)))e="package_tree_changed";
    if(!e){tree->files[i]=after;tree->found[i]=1;}
    if(fd>=0)close(fd);return e;
}
static const char *walk(int fd,const char *prefix,Tree *tree){
    int copy=openat(fd,".",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    DIR *directory=copy>=0?fdopendir(copy):NULL;
    if(!directory){if(copy>=0)close(copy);return "package_tree_unavailable";}
    const char *e=NULL;
    for(;;){
        e=tree->control?tree->control(tree->context):NULL;if(e)break;
        errno=0;struct dirent *entry=readdir(directory);
        if(!entry){if(errno)e="package_tree_unavailable";break;}
        if(!strcmp(entry->d_name,".")||!strcmp(entry->d_name,".."))continue;
        if(++tree->visited>PACKAGE_INDEX_FILES+TREE_DIRECTORIES){e="package_tree_limit";break;}
        char path[256];int n=snprintf(path,sizeof(path),"%s%s%s",prefix,*prefix?"/":"",entry->d_name);
        if(n<0||(size_t)n>=sizeof(path)){e="unlisted_package_artifact";break;}
        struct stat named;if(fstatat(fd,entry->d_name,&named,AT_SYMLINK_NOFOLLOW)){e="package_tree_changed";break;}
        if(S_ISREG(named.st_mode))e=regular(fd,entry->d_name,path,&named,tree);
        else if(S_ISDIR(named.st_mode)){
            int needed=0;for(size_t i=0;i<tree->index->count;i++){
                size_t length=strlen(tree->index->files[i].name);
                if(length>(size_t)n&&!memcmp(path,tree->index->files[i].name,(size_t)n)&&tree->index->files[i].name[n]=='/')needed=1;
            }
            if(!needed)e="unlisted_package_artifact";
            else if(!private(tree,&named))e="unsupported_package_artifact";
            else{
                size_t record=0;
                if(tree->recheck){while(record<tree->directory_count&&strcmp(path,tree->directories[record].name))record++;
                    if(record==tree->directory_count||!same(&named,&tree->directories[record].identity))e="package_tree_changed";}
                else if(tree->directory_count==TREE_DIRECTORIES)e="package_tree_limit";
                else{
                    if(named.st_ino==tree->root.st_ino)e="unsupported_package_artifact";
                    for(size_t i=0;!e&&i<tree->directory_count;i++)if(tree->directories[i].identity.st_ino==named.st_ino)e="unsupported_package_artifact";
                    if(!e){record=tree->directory_count++;strcpy(tree->directories[record].name,path);tree->directories[record].identity=named;}
                }
                int child=-1;struct stat held,after,rebound;
                if(!e){child=openat(fd,entry->d_name,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
                    if(child<0||fstat(child,&held)||!same(&named,&held))e="package_tree_changed";}
                if(!e)e=walk(child,path,tree);
                if(!e&&tree->sync&&!tree->recheck&&fsync(child))e="package_sync_failed";
                if(!e&&(fstat(child,&after)||fstatat(fd,entry->d_name,&rebound,AT_SYMLINK_NOFOLLOW)||
                    !same(&named,&after)||!same(&after,&rebound)))e="package_tree_changed";
                if(child>=0)close(child);
            }
        }else e="unsupported_package_artifact";
        if(e)break;
    }
    closedir(directory);return e;
}
static const char *verify(int root,const PackageIndex *index,const char *(*control)(void *),void *context,PackageTreeResult *out,int sync){
    if(!index||!out||!index->count||index->count>PACKAGE_INDEX_FILES||
       !memchr(index->manifest_sha256,0,sizeof(index->manifest_sha256))||strlen(index->manifest_sha256)!=64||
       strspn(index->manifest_sha256,"0123456789abcdef")!=64)return "invalid_package_index";
    uint64_t expected=0;
    for(size_t i=0;i<index->count;i++){
        const PackageFile *f=&index->files[i];
        if(!memchr(f->name,0,sizeof(f->name))||!f->name[0]||!memchr(f->sha256,0,sizeof(f->sha256))||
           strlen(f->sha256)!=64||strspn(f->sha256,"0123456789abcdef")!=64||!f->bytes||f->bytes>PACKAGE_PAYLOAD_BYTES-expected||
           f->role<PACKAGE_ENGINE||f->role>PACKAGE_METADATA)return "invalid_package_index";
        expected+=f->bytes;
    }
    if(expected!=index->payload_bytes)return "invalid_package_index";
    const char *e=control?control(context):NULL;if(e)return e;
    Tree *tree=calloc(1,sizeof(*tree));if(!tree)return "allocation_failed";
    tree->index=index;tree->control=control;tree->context=context;tree->sync=sync;
    if(fstat(root,&tree->root)||!S_ISDIR(tree->root.st_mode)||!private(tree,&tree->root)||
       (fcntl(root,F_GETFL)&O_ACCMODE)!=O_RDONLY)e="unsupported_package_root";
    if(!e)e=walk(root,"",tree);
    if(!e)for(size_t i=0;i<index->count;i++)if(!tree->found[i]){e="package_file_missing";break;}
    /* Hash every file, then observe the entire namespace/inode set again. This
     * catches late changes to earlier files while later payloads were hashed. */
    if(!e){tree->recheck=1;tree->visited=0;memset(tree->found,0,sizeof(tree->found));e=walk(root,"",tree);}
    if(!e)for(size_t i=0;i<index->count;i++)if(!tree->found[i]){e="package_file_missing";break;}
    struct stat after;
    if(!e&&sync&&fsync(root))e="package_sync_failed";
    if(!e&&(fstat(root,&after)||!same(&tree->root,&after)))e="package_tree_changed";
    if(!e)e=control?control(context):NULL;
    if(!e){PackageTreeResult result={.files=index->count,.directories=tree->directory_count,.payload_bytes=expected};
        strcpy(result.manifest_sha256,index->manifest_sha256);*out=result;}
    free(tree);return e;
}
const char *package_tree_verify(int root,const PackageIndex *index,const char *(*control)(void *),void *context,PackageTreeResult *out){return verify(root,index,control,context,out,0);}
const char *package_tree_sync(int root,const PackageIndex *index,const char *(*control)(void *),void *context,PackageTreeResult *out){return verify(root,index,control,context,out,1);}
