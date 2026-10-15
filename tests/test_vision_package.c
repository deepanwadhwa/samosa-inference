#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "vision_package.h"
#include "library_runtime.h"
#include "catalog.h"
#include "library_fixture.h"
#include "sha256.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"vision package failure %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static const char *cancel(void *p){(void)p;return "cancelled";}
static void put(int root,PackageIndex *index,size_t i,const void *body,size_t bytes){
    PackageFile *f=&index->files[i];int fd=openat(root,f->name,O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW,0600);CHECK(fd>=0);CHECK(write(fd,body,bytes)==(ssize_t)bytes);CHECK(!close(fd));index->payload_bytes-=f->bytes;f->bytes=bytes;index->payload_bytes+=bytes;hash_bytes(body,bytes,f->sha256);
}
static void rejects(int root,PackageIndex *index,const VisionPackagePolicy *policy,const char *expected){
    VisionPackageDescriptor out;memset(&out,0xa5,sizeof(out));VisionPackageDescriptor before=out;const char *e=vision_package_verify(root,index,policy,NULL,NULL,&out);
    if(!e||strcmp(e,expected))fprintf(stderr,"expected %s got %s\n",expected,e?e:"success");CHECK(e&&!strcmp(e,expected));CHECK(!memcmp(&out,&before,sizeof(out)));
}
int main(void){
    char path[]="/tmp/samosa-vision-package-XXXXXX";CHECK(mkdtemp(path));int root=open(path,O_RDONLY|O_DIRECTORY);CHECK(root>=0);CHECK(!mkdirat(root,"model",0700));
    PackageIndex *index=calloc(1,sizeof(*index));CHECK(index);index->count=5;memset(index->manifest_sha256,'a',64);strcpy(index->minimum_os,"1.0");strcpy(index->recipe,"visionpsy-package-v1");char os[65];CHECK(!provider_catalog_host(index->platform,os));
    const char *names[]={"engine","model/config.json","model/tokenizer.json","model/model.safetensors","vision-package.json"};
    enum PackageFileRole roles[]={PACKAGE_ENGINE,PACKAGE_METADATA,PACKAGE_DATA,PACKAGE_MODEL,PACKAGE_METADATA};
    for(size_t i=0;i<5;i++){strcpy(index->files[i].name,names[i]);index->files[i].role=roles[i];}
    LibraryRuntimePolicy host;CHECK(!library_runtime_host_policy(&host));unsigned char image[4096];if(host.mach_o)lib_executable_mach(image,host.cpu);else lib_executable_elf(image,host.cpu);put(root,index,0,image,sizeof(image));
    for(size_t i=1;i<4;i++)put(root,index,i,"fixture",7);
    const char *body="{\"format\":\"samosa.inference.vision-package.v1\",\"profile\":\"visionpsy-v1\",\"engine\":\"engine\",\"model_directory\":\"model\"}";
    put(root,index,4,body,strlen(body));VisionPackagePolicy policy={index->files[0].sha256,index->files[1].sha256,index->files[2].sha256,index->files[3].sha256};
    VisionPackageDescriptor out;CHECK(vision_package_policy_valid(&policy));const char *initial=vision_package_verify(root,index,&policy,NULL,NULL,&out);if(initial)fprintf(stderr,"initial: %s\n",initial);CHECK(!initial);CHECK(out.engine==0&&out.config==1&&out.tokenizer==2&&out.weights==3);
    VisionPackageDescriptor before=out;CHECK(!strcmp(vision_package_verify(root,index,&policy,cancel,NULL,&out),"cancelled"));CHECK(!memcmp(&out,&before,sizeof(out)));
    rejects(-1,index,NULL,"invalid_vision_package_policy");
    const char **pins[]={&policy.engine_sha256,&policy.config_sha256,&policy.tokenizer_sha256,&policy.weights_sha256};
    for(size_t i=0;i<4;i++){const char *old=*pins[i];*pins[i]="aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";rejects(root,index,&policy,i?"unqualified_vision_package_model":"unqualified_vision_package_runtime");*pins[i]=old;}
    for(size_t n=1;n<strlen(body);n++){put(root,index,4,body,n);rejects(root,index,&policy,"invalid_vision_package_descriptor");}put(root,index,4,body,strlen(body));
    char extra[4096];snprintf(extra,sizeof(extra),"%.*s,\"command\":\"/bin/sh\"}",(int)strlen(body)-1,body);put(root,index,4,extra,strlen(extra));rejects(root,index,&policy,"invalid_vision_package_descriptor");put(root,index,4,body,strlen(body));
    for(size_t i=1;i<5;i++){enum PackageFileRole old=index->files[i].role;index->files[i].role=PACKAGE_ENGINE;rejects(root,index,&policy,"vision_package_identity_conflict");index->files[i].role=old;}
    CHECK(!symlinkat("model",root,"unknown"));rejects(root,index,&policy,"unsupported_package_artifact");CHECK(!unlinkat(root,"unknown",0));
    CHECK(!vision_package_verify(root,index,&policy,NULL,NULL,&out));
    for(size_t i=0;i<5;i++)CHECK(!unlinkat(root,index->files[i].name,0));CHECK(!unlinkat(root,"model",AT_REMOVEDIR));free(index);CHECK(!close(root));CHECK(!rmdir(path));printf("PASS Vision package: %u checks; independent pins, fixed layout, descriptor, native graph and controls\n",checks);return 0;
}
