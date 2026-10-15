#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "whisper_package.h"
#include "library_runtime.h"
#include "catalog.h"
#include "library_fixture.h"
#include "sha256.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"whisper package failure %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static const char *cancel(void *p){(void)p;return "cancelled";}
static void put(int root,PackageIndex *index,size_t i,const void *body,size_t bytes){
    PackageFile *f=&index->files[i];int fd=openat(root,f->name,O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW,0600);CHECK(fd>=0);CHECK(write(fd,body,bytes)==(ssize_t)bytes);CHECK(!close(fd));index->payload_bytes-=f->bytes;f->bytes=bytes;index->payload_bytes+=bytes;hash_bytes(body,bytes,f->sha256);
}
static void rejects(int root,PackageIndex *index,const WhisperPackagePolicy *policy,const char *expected){
    WhisperPackageDescriptor out;memset(&out,0xa5,sizeof(out));WhisperPackageDescriptor before=out;const char *e=whisper_package_verify(root,index,policy,NULL,NULL,&out);
    if(!e||strcmp(e,expected))fprintf(stderr,"expected %s got %s\n",expected,e?e:"success");CHECK(e&&!strcmp(e,expected));CHECK(!memcmp(&out,&before,sizeof(out)));
}
int main(int argc,char **argv){
    int exporting=argc==2&&!strcmp(argv[1],"--export");CHECK(argc==1||exporting);
    char path[]="/tmp/samosa-whisper-package-XXXXXX";CHECK(mkdtemp(path));int root=open(path,O_RDONLY|O_DIRECTORY);CHECK(root>=0);
    PackageIndex *index=calloc(1,sizeof(*index));CHECK(index);index->count=3;memset(index->manifest_sha256,'a',64);strcpy(index->recipe,"whisper-ggml-package-v1");strcpy(index->minimum_os,"1.0");char os[65];CHECK(!provider_catalog_host(index->platform,os));
    strcpy(index->files[0].name,"engine");index->files[0].role=PACKAGE_ENGINE;strcpy(index->files[1].name,"model.bin");index->files[1].role=PACKAGE_MODEL;strcpy(index->files[2].name,"whisper-package.json");index->files[2].role=PACKAGE_METADATA;
    LibraryRuntimePolicy host;CHECK(!library_runtime_host_policy(&host));unsigned char image[4096];if(host.mach_o)lib_executable_mach(image,host.cpu);else lib_executable_elf(image,host.cpu);put(root,index,0,image,sizeof(image));
    uint32_t header[]={0x67676d6c,51864,1500,512,8,6,448,512,8,6,80,1};unsigned char model[48];for(size_t i=0;i<12;i++)lib_w32(model+i*4,header[i]);put(root,index,1,model,sizeof(model));
    const char *body="{\"format\":\"samosa.inference.whisper-package.v1\",\"runtime_revision\":\"f049fff95a089aa9969deb009cdd4892b3e74916\",\"profile\":\"base.en-f16\",\"engine\":\"engine\",\"model\":\"model.bin\"}";
    put(root,index,2,body,strlen(body));WhisperPackagePolicy policy={.runtime_revision="f049fff95a089aa9969deb009cdd4892b3e74916",.profile="base.en-f16",.engine_sha256=index->files[0].sha256,.model_sha256=index->files[1].sha256};CHECK(whisper_package_policy_valid(&policy));
    WhisperPackageDescriptor out;CHECK(!whisper_package_verify(root,index,&policy,NULL,NULL,&out));CHECK(out.engine==0&&out.model==1&&!strcmp(out.profile,"base.en-f16"));
    WhisperPackageDescriptor before=out;CHECK(!strcmp(whisper_package_verify(root,index,&policy,cancel,NULL,&out),"cancelled"));CHECK(!memcmp(&out,&before,sizeof(out)));
    rejects(-1,index,NULL,"invalid_whisper_package_policy");
    const char *old=policy.engine_sha256;policy.engine_sha256="aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";rejects(root,index,&policy,"unqualified_whisper_package_runtime");policy.engine_sha256=old;
    old=policy.model_sha256;policy.model_sha256="aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";rejects(root,index,&policy,"unqualified_whisper_package_model");policy.model_sha256=old;
    old=policy.runtime_revision;policy.runtime_revision="0000000000000000000000000000000000000000";rejects(root,index,&policy,"unqualified_whisper_package_runtime");policy.runtime_revision=old;
    policy.profile="base";CHECK(!whisper_package_policy_valid(&policy));rejects(root,index,&policy,"invalid_whisper_package_policy");policy.profile="base.en-f16";
    /* Even a separately trusted malformed-model pin cannot bypass profile checks. */
    for(size_t i=0;i<12;i++){model[i*4]^=1;put(root,index,1,model,sizeof(model));rejects(root,index,&policy,"incompatible_whisper_package_model");model[i*4]^=1;}
    for(size_t n=1;n<sizeof(model);n++){put(root,index,1,model,n);rejects(root,index,&policy,"incompatible_whisper_package_model");}put(root,index,1,model,sizeof(model));
    for(size_t n=0;n<strlen(body);n++){memset(&out,0xa5,sizeof(out));before=out;CHECK(whisper_package_descriptor_parse(body,n,index,&out));CHECK(!memcmp(&out,&before,sizeof(out)));}
    char extra[4096];snprintf(extra,sizeof(extra),"%.*s,\"command\":\"/bin/sh\"}",(int)strlen(body)-1,body);put(root,index,2,extra,strlen(extra));rejects(root,index,&policy,"invalid_whisper_package_descriptor");put(root,index,2,body,strlen(body));
    index->files[1].role=PACKAGE_DATA;rejects(root,index,&policy,"whisper_package_identity_conflict");index->files[1].role=PACKAGE_MODEL;
    CHECK(!symlinkat("model.bin",root,"unknown"));rejects(root,index,&policy,"unsupported_package_artifact");CHECK(!unlinkat(root,"unknown",0));
    CHECK(!whisper_package_verify(root,index,&policy,NULL,NULL,&out));
    if(exporting){char *json=NULL;size_t bytes=0;FILE *stream=open_memstream(&json,&bytes);CHECK(stream);fprintf(stream,"{\"format\":\"samosa.inference.package.v1\",\"platform\":\"%s\",\"minimum_os\":\"1.0\",\"recipe\":\"whisper-ggml-package-v1\",\"files\":[",index->platform);
        const char *roles[]={"engine","model","metadata"};for(size_t i=0;i<3;i++){if(i)fputc(',',stream);fprintf(stream,"{\"name\":\"%s\",\"role\":\"%s\",\"bytes\":%llu,\"sha256\":\"%s\"}",index->files[i].name,roles[i],(unsigned long long)index->files[i].bytes,index->files[i].sha256);}fputs("]}",stream);CHECK(!fclose(stream));char pin[65],target[256];hash_bytes(json,bytes,pin);CHECK(snprintf(target,sizeof(target),"%s.index.json",path)<(int)sizeof(target));int fd=open(target,O_WRONLY|O_CREAT|O_EXCL,0600);CHECK(fd>=0);CHECK(write(fd,json,bytes)==(ssize_t)bytes);CHECK(!close(fd));
        printf("{\"root\":\"%s\",\"index\":\"%s\",\"pin\":\"%s\",\"engine_pin\":\"%s\",\"model_pin\":\"%s\"}\n",path,target,pin,index->files[0].sha256,index->files[1].sha256);free(json);free(index);CHECK(!close(root));return 0;}
    for(size_t i=0;i<3;i++)CHECK(!unlinkat(root,index->files[i].name,0));free(index);CHECK(!close(root));CHECK(!rmdir(path));printf("PASS Whisper package: %u checks; host-qualified engine/model pins, profile/header, native graph, strict descriptor and controls\n",checks);return 0;
}
