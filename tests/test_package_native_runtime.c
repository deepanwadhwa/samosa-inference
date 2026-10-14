#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "package_native_runtime.h"
#include "llama_package.h"
#ifdef SAMOSA_LLAMA_INSTALL_TEST
#include "package_assemble.h"
#endif
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
    if(argc==4||argc==5||argc==6){int root=open(argv[1],O_RDONLY|O_DIRECTORY|O_NOFOLLOW),fd=open(argv[2],O_RDONLY|O_NOFOLLOW);struct stat s;
        CHECK(root>=0&&fd>=0&&!fstat(fd,&s)&&S_ISREG(s.st_mode)&&s.st_uid==geteuid()&&!(s.st_mode&077)&&s.st_nlink==1&&s.st_size>0&&s.st_size<=PACKAGE_INDEX_BYTES);
        char *body=malloc((size_t)s.st_size);PackageIndex *index=calloc(1,sizeof(*index));CHECK(body&&index);size_t at=0;
        while(at<(size_t)s.st_size){ssize_t n=read(fd,body+at,(size_t)s.st_size-at);CHECK(n>0);at+=(size_t)n;}
        CHECK(!package_index_parse(body,at,argv[3],index));const char *e;
        if(argc>=5){CHECK(!strcmp(argv[4],"llama"));LlamaPackagePolicy policy={.runtime_revision="4fbc76dec51d0add466f0210855c0596589b60d4",.kind="embedding",.architecture="gemma-embedding2",.engine_sha256="958601dbd22ab2f5857e09860247f30c74153cb0f3914b8d62f2bacbba9aab97"};LlamaPackageDescriptor descriptor;e=llama_package_verify(root,index,&policy,NULL,NULL,&descriptor);
#ifdef SAMOSA_LLAMA_INSTALL_TEST
            if(!e&&argc==6){int destination=open(argv[5],O_RDONLY|O_DIRECTORY|O_NOFOLLOW);CHECK(destination>=0);char payload[4096];CHECK(snprintf(payload,sizeof(payload),"%s/payload",argv[5])<(int)sizeof(payload));
                PackageAssembly assembly={.directory=destination,.source=root,.index=index,.payload_path=payload,.copy_ceiling=index->payload_bytes};PackageTreeResult installed;
                CHECK(!strcmp(package_assemble(&assembly,&installed),"unqualified_llama_package_runtime"));CHECK(faccessat(destination,"assemble.lock",F_OK,0)<0);
                assembly.llama_policy=&policy;CHECK(!package_assemble(&assembly,&installed));CHECK(installed.files==index->count);
                PackageAssemblyStatus status;assembly.source=-1;CHECK(!package_assembly_status(&assembly,&status));CHECK(!strcmp(status.state,"installed_verified"));
                CHECK(!fstatat(destination,"payload/llama-server",&s,AT_SYMLINK_NOFOLLOW));CHECK((s.st_mode&0777)==0500);CHECK(!close(destination));
                puts("PASS actual llama installation: missing-policy refusal, complete publication, offline status and executable engine mode");}
#else
            CHECK(argc!=6);
#endif
        }
        else e=package_native_runtime_verify(root,index,NULL,NULL);if(e)fprintf(stderr,"actual native runtime: %s\n",e);CHECK(!e);
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
    /* Combine trusted engine policy, pinned descriptor and model metadata.
     * Synthetic metadata tests this static gate, not tensor/model readiness. */
    memset(bytes,0,sizeof(bytes));memcpy(bytes,"GGUF",4);lib_w32(bytes+4,3);lib_w64(bytes+8,1);lib_w64(bytes+16,1);
    const char *architecture="gemma-embedding2",*key="general.architecture";size_t at=24;lib_w64(bytes+at,strlen(key));at+=8;memcpy(bytes+at,key,strlen(key));at+=strlen(key);lib_w32(bytes+at,8);at+=4;lib_w64(bytes+at,strlen(architecture));at+=8;memcpy(bytes+at,architecture,strlen(architecture));at+=strlen(architecture);
    put(root,&index->files[2],bytes,at);index->count=4;strcpy(index->files[3].name,"llama-package.json");index->files[3].role=PACKAGE_METADATA;
    const char *manifest="{\"format\":\"samosa.inference.llama-package.v1\",\"kind\":\"embedding\",\"runtime_revision\":\"4fbc76dec51d0add466f0210855c0596589b60d4\",\"engine\":\"engine\",\"model\":\"model\",\"architecture\":\"gemma-embedding2\"}";
    put(root,&index->files[3],manifest,strlen(manifest));index->payload_bytes=8192+at+strlen(manifest);
    LlamaPackagePolicy qualified={.runtime_revision="4fbc76dec51d0add466f0210855c0596589b60d4",.kind="embedding",.architecture=architecture,.engine_sha256=index->files[0].sha256};
    LlamaPackageDescriptor descriptor;memset(&descriptor,0xa5,sizeof(descriptor));LlamaPackageDescriptor unchanged_descriptor=descriptor;
    CHECK(!strcmp(llama_package_verify(root,index,&qualified,cancel,NULL,&descriptor),"cancelled"));CHECK(!memcmp(&descriptor,&unchanged_descriptor,sizeof(descriptor)));
    qualified.kind="text";CHECK(!strcmp(llama_package_verify(root,index,&qualified,NULL,NULL,&descriptor),"unqualified_llama_package_runtime"));CHECK(!memcmp(&descriptor,&unchanged_descriptor,sizeof(descriptor)));qualified.kind="embedding";
    const char *engine_pin=qualified.engine_sha256;qualified.engine_sha256="aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";CHECK(!strcmp(llama_package_verify(root,index,&qualified,NULL,NULL,&descriptor),"unqualified_llama_package_runtime"));CHECK(!memcmp(&descriptor,&unchanged_descriptor,sizeof(descriptor)));qualified.engine_sha256=engine_pin;
    CHECK(!llama_package_verify(root,index,&qualified,NULL,NULL,&descriptor));CHECK(descriptor.engine==0&&descriptor.model==2);
    unchanged_descriptor=descriptor;qualified.runtime_revision="0000000000000000000000000000000000000000";CHECK(!strcmp(llama_package_verify(root,index,&qualified,NULL,NULL,&descriptor),"unqualified_llama_package_runtime"));CHECK(!memcmp(&descriptor,&unchanged_descriptor,sizeof(descriptor)));qualified.runtime_revision="4fbc76dec51d0add466f0210855c0596589b60d4";
    bytes[at-strlen(architecture)]='q';put(root,&index->files[2],bytes,at);CHECK(!strcmp(llama_package_verify(root,index,&qualified,NULL,NULL,&descriptor),"incompatible_llama_package_model"));CHECK(!memcmp(&descriptor,&unchanged_descriptor,sizeof(descriptor)));bytes[at-strlen(architecture)]='g';put(root,&index->files[2],bytes,at);
    CHECK(!symlinkat("model",root,"foreign"));CHECK(package_native_runtime_verify(root,index,NULL,NULL));CHECK(!unlinkat(root,"foreign",0));
    for(size_t i=0;i<index->count;i++)CHECK(!unlinkat(root,index->files[i].name,0));CHECK(!close(root));CHECK(!rmdir(path));free(index);
    printf("PASS native runtime package: %u checks; complete pins, actual host, executable/library graph and opaque models\n",checks);return 0;
}
