#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "package_native_runtime.h"
#include "library_runtime.h"
#include "engine_runtime.h"
#include "artifact.h"
#include "catalog.h"
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
int package_native_runtime_bound(int root,const char *path) {
    struct stat held,named;
    return path&&path[0]=='/'&&!fstat(root,&held)&&!lstat(path,&named)&&
        S_ISDIR(held.st_mode)&&S_ISDIR(named.st_mode)&&held.st_dev==named.st_dev&&held.st_ino==named.st_ino;
}
static int open_file(int root,const char *name){
    char path[256];strcpy(path,name);int parent=openat(root,".",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if(parent<0)return -1;char *leaf=path;
    for(char *slash=strchr(leaf,'/');slash;slash=strchr(leaf,'/')){
        *slash=0;int next=openat(parent,leaf,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);close(parent);
        if(next<0)return -1;parent=next;leaf=slash+1;
    }
    int fd=openat(parent,leaf,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);close(parent);return fd;
}
const char *package_native_runtime_verify(int root,const PackageIndex *index,const char *(*control)(void *),void *context){
    if(!index||!index->count||index->count>PACKAGE_INDEX_FILES||!memchr(index->platform,0,sizeof(index->platform))||!memchr(index->minimum_os,0,sizeof(index->minimum_os)))return "invalid_package_index";
    const char *e=control?control(context):NULL;if(e)return e;
    e=engine_runtime_environment();if(e)return e;
    PackageTreeResult observed;e=package_tree_verify(root,index,control,context,&observed);if(e)return e;
    char platform[65],os[65];e=provider_catalog_host(platform,os);if(!e)e=package_index_qualify(index,platform,os);if(e)return e;
    LibraryRuntimePolicy policy;e=library_runtime_host_policy(&policy);if(e)return e;
    LibraryRuntimeFile *files=calloc(index->count,sizeof(*files));LibraryRuntimeImage *images=calloc(index->count,sizeof(*images));
    if(!files||!images){free(files);free(images);return "resource_limit";}
    size_t engines=0;
    for(size_t i=0;!e&&i<index->count;i++){
        const PackageFile *file=&index->files[i];files[i].name=file->name;
        files[i].kind=file->role==PACKAGE_ENGINE?LIBRARY_RUNTIME_EXECUTABLE:file->role==PACKAGE_LIBRARY?LIBRARY_RUNTIME_LIBRARY:LIBRARY_RUNTIME_OTHER;
        if(files[i].kind==LIBRARY_RUNTIME_OTHER)continue;
        if(files[i].kind==LIBRARY_RUNTIME_EXECUTABLE)engines++;
        e=control?control(context):NULL;if(e)break;
        int fd=open_file(root,file->name);if(fd<0){e="artifact_changed";break;}
        /* Rehash the held image around inspection; the final closed-tree check
         * also observes the complete namespace after graph resolution. */
        e=artifact_verify_controlled(fd,file->bytes,PACKAGE_PAYLOAD_BYTES,file->sha256,control,context);
        if(!e)e=files[i].kind==LIBRARY_RUNTIME_EXECUTABLE?library_runtime_inspect_executable(fd,&policy,&images[i]):library_runtime_inspect(fd,&policy,&images[i]);
        if(!e)e=artifact_verify_controlled(fd,file->bytes,PACKAGE_PAYLOAD_BYTES,file->sha256,control,context);
        close(fd);
    }
    if(!e&&engines!=1)e="invalid_native_package_engine";
    if(!e)e=library_runtime_graph(&policy,files,images,index->count,control,context);
    if(!e)e=package_tree_verify(root,index,control,context,&observed);
    free(images);free(files);return e;
}
