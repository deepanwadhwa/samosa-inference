#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "package_native_runtime.h"
#include "library_runtime.h"
#include "catalog.h"
#include "library_fixture.h"
#include "sha256.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"native package failure %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static const char *cancel(void *unused){(void)unused;return "cancelled";}
static void put(int root,PackageFile *file,const void *data,size_t bytes){
    int fd=openat(root,file->name,O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW,0600);CHECK(fd>=0);CHECK(write(fd,data,bytes)==(ssize_t)bytes);CHECK(!close(fd));
    file->bytes=bytes;hash_bytes(data,bytes,file->sha256);
}
int main(int argc,char **argv){
    if(argc==4){int root=open(argv[1],O_RDONLY|O_DIRECTORY|O_NOFOLLOW),fd=open(argv[2],O_RDONLY|O_NOFOLLOW);struct stat s;
        CHECK(root>=0&&fd>=0&&!fstat(fd,&s)&&S_ISREG(s.st_mode)&&s.st_uid==geteuid()&&!(s.st_mode&077)&&s.st_nlink==1&&s.st_size>0&&s.st_size<=PACKAGE_INDEX_BYTES);
        char *body=malloc((size_t)s.st_size);PackageIndex *index=calloc(1,sizeof(*index));CHECK(body&&index);size_t at=0;
        while(at<(size_t)s.st_size){ssize_t n=read(fd,body+at,(size_t)s.st_size-at);CHECK(n>0);at+=(size_t)n;}
        CHECK(!package_index_parse(body,at,argv[3],index));const char *e=package_native_runtime_verify(root,index,NULL,NULL);if(e)fprintf(stderr,"actual native runtime: %s\n",e);CHECK(!e);
        printf("PASS actual native runtime package: %zu files; %llu pinned bytes; index %s; loader compatibility only\n",index->count,(unsigned long long)index->payload_bytes,index->manifest_sha256);
        free(index);free(body);CHECK(!close(fd));CHECK(!close(root));return 0;
    }
    CHECK(argc==1);
    char path[]="/tmp/samosa-native-package-XXXXXX";CHECK(mkdtemp(path));int root=open(path,O_RDONLY|O_DIRECTORY);CHECK(root>=0);
    PackageIndex *index=calloc(1,sizeof(*index));CHECK(index);memset(index->manifest_sha256,'a',64);index->count=3;strcpy(index->minimum_os,"1.0");strcpy(index->recipe,"native-runtime-test");char os[65];CHECK(!provider_catalog_host(index->platform,os));
    strcpy(index->files[0].name,"engine");index->files[0].role=PACKAGE_ENGINE;strcpy(index->files[1].name,"library");index->files[1].role=PACKAGE_LIBRARY;strcpy(index->files[2].name,"model");index->files[2].role=PACKAGE_MODEL;
    LibraryRuntimePolicy policy;CHECK(!library_runtime_host_policy(&policy));unsigned char bytes[4096];
    if(policy.mach_o)lib_executable_mach(bytes,policy.cpu);else lib_executable_elf(bytes,policy.cpu);put(root,&index->files[0],bytes,sizeof(bytes));
    if(policy.mach_o)lib_mach(bytes,policy.cpu,NULL,0);else lib_elf(bytes,policy.cpu,"libc.so.6",1);put(root,&index->files[1],bytes,sizeof(bytes));
    put(root,&index->files[2],"abc",3);index->payload_bytes=8195;
    CHECK(!strcmp(package_native_runtime_verify(root,index,cancel,NULL),"cancelled"));CHECK(!package_native_runtime_verify(root,index,NULL,NULL));
    index->files[0].role=PACKAGE_DATA;CHECK(!strcmp(package_native_runtime_verify(root,index,NULL,NULL),"invalid_native_package_engine"));index->files[0].role=PACKAGE_ENGINE;
    /* A pinned but incompatible unreferenced library still fails admission. */
    if(policy.mach_o)lib_mach(bytes,policy.cpu==0x100000c?0x1000007:0x100000c,NULL,0);else lib_elf(bytes,policy.cpu==62?183:62,"libc.so.6",1);put(root,&index->files[1],bytes,sizeof(bytes));
    CHECK(!strcmp(package_native_runtime_verify(root,index,NULL,NULL),"incompatible_library_binary"));
    if(policy.mach_o)lib_mach(bytes,policy.cpu,NULL,0);else lib_elf(bytes,policy.cpu,"libc.so.6",1);put(root,&index->files[1],bytes,sizeof(bytes));
    int model=openat(root,"model",O_WRONLY);CHECK(model>=0);CHECK(pwrite(model,"x",1,0)==1);CHECK(!close(model));CHECK(!strcmp(package_native_runtime_verify(root,index,NULL,NULL),"artifact_checksum_mismatch"));
    put(root,&index->files[2],"abc",3);CHECK(!package_native_runtime_verify(root,index,NULL,NULL));
    CHECK(!symlinkat("model",root,"foreign"));CHECK(package_native_runtime_verify(root,index,NULL,NULL));CHECK(!unlinkat(root,"foreign",0));
    for(size_t i=0;i<index->count;i++)CHECK(!unlinkat(root,index->files[i].name,0));CHECK(!close(root));CHECK(!rmdir(path));free(index);
    printf("PASS native runtime package: %u checks; complete pins, actual host, executable/library graph and opaque models\n",checks);return 0;
}
