#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "decision_package.h"
#include "library_runtime.h"
#include "library_fixture.h"
#include "catalog.h"
#include "sha256.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"decision package failure %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void put(int root,PackageIndex *index,size_t i,const void *body,size_t bytes) {
    PackageFile *file=&index->files[i];int mode=file->role==PACKAGE_ENGINE?0700:0600;
    int fd=openat(root,file->name,O_WRONLY|O_CREAT|O_TRUNC,mode);CHECK(fd>=0);
    CHECK(write(fd,body,bytes)==(ssize_t)bytes);CHECK(!fchmod(fd,mode));CHECK(!close(fd));
    index->payload_bytes-=file->bytes;file->bytes=bytes;index->payload_bytes+=bytes;
    hash_bytes(body,bytes,file->sha256);
}
static void rejects(int root,const char *path,const PackageIndex *index,const DecisionPackagePolicy *policy,const char *expected) {
    DecisionPackageDescriptor output;memset(&output,0xa5,sizeof(output));DecisionPackageDescriptor before=output;
    const char *error=decision_package_verify(root,path,index,policy,NULL,NULL,&output);
    if(!error|| (expected&&strcmp(error,expected)))fprintf(stderr,"observed error: %s, expected: %s\n",error?error:"none",expected?expected:"any refusal");
    CHECK(error&&(!expected||!strcmp(error,expected)));CHECK(!memcmp(&before,&output,sizeof(output)));
}
static const char *cancel(void *context){(void)context;return "cancelled";}
int main(int argc,char **argv) {
    int exporting=argc==2&&(!strcmp(argv[1],"--export-image")||!strcmp(argv[1],"--export-text"));
    CHECK(argc==1||exporting);
    char path[]="/tmp/samosa-decision-package-XXXXXX";CHECK(mkdtemp(path));
    int root=open(path,O_RDONLY|O_DIRECTORY|O_NOFOLLOW);CHECK(root>=0);CHECK(!mkdirat(root,"assets",0700));
    PackageIndex *index=calloc(1,sizeof(*index));CHECK(index);index->count=5;
    strcpy(index->recipe,"opendecision-package-v1");strcpy(index->minimum_os,"1.0");
    char os[65];CHECK(!provider_catalog_host(index->platform,os));memset(index->manifest_sha256,'a',64);
    const enum PackageFileRole roles[]={PACKAGE_ENGINE,PACKAGE_LIBRARY,PACKAGE_MODEL,PACKAGE_DATA};
    for(unsigned i=0;i<4;i++){snprintf(index->files[i].name,sizeof(index->files[i].name),"assets/%s",decision_asset_name(i));index->files[i].role=roles[i];}
    strcpy(index->files[4].name,"decision-package.json");index->files[4].role=PACKAGE_METADATA;
    LibraryRuntimePolicy host;CHECK(!library_runtime_host_policy(&host));unsigned char engine[4096],library[4096];
    if(host.mach_o) {
        lib_executable_mach(engine,host.cpu);char needed[128];snprintf(needed,sizeof(needed),"@rpath/%s",decision_asset_name(DECISION_RUNTIME));
        uint32_t command_bytes=engine[20]|(uint32_t)engine[21]<<8|(uint32_t)engine[22]<<16|(uint32_t)engine[23]<<24;
        size_t at=lib_command(engine,32+command_bytes,0xc,needed,24);at=lib_command(engine,at,0x8000001c,"@loader_path",12);
        lib_w32(engine+16,7);lib_w32(engine+20,(uint32_t)(at-32));lib_mach(library,host.cpu,NULL,0);
    } else {
        lib_executable_elf(engine,host.cpu);lib_w64(engine+576,1);lib_w64(engine+584,192);lib_w64(engine+592,0);
        strcpy((char*)engine+1216,decision_asset_name(DECISION_RUNTIME));lib_elf(library,host.cpu,NULL,0);
    }
    put(root,index,0,engine,sizeof(engine));put(root,index,1,library,sizeof(library));put(root,index,2,"model",5);put(root,index,3,"tokenizer",9);
    const char *body="{\"format\":\"samosa.inference.decision-package.v1\",\"profile\":\"image\",\"protocol\":\"opendecision-framed-v1\",\"assets\":\"assets\",\"runtime_revision\":\"327d6911fc7f31e779d2fa673e83ba1f3410b468\"}";
    put(root,index,4,body,strlen(body));
    char qualified[4][65];DecisionPackagePolicy policy={.profile=DECISION_ASSETS_IMAGE,.runtime_revision="327d6911fc7f31e779d2fa673e83ba1f3410b468"};
    for(unsigned i=0;i<4;i++){strcpy(qualified[i],index->files[i].sha256);policy.pins[i]=qualified[i];}
    CHECK(decision_package_policy_valid(&policy));
    DecisionPackagePolicy invalid=policy;invalid.profile=2;CHECK(!decision_package_policy_valid(&invalid));
    char unterminated[65];memset(unterminated,'a',sizeof(unterminated));invalid=policy;invalid.pins[0]=unterminated;
    CHECK(!decision_package_policy_valid(&invalid));invalid=policy;invalid.runtime_revision=unterminated;
    CHECK(!decision_package_policy_valid(&invalid));
    DecisionPackageDescriptor output;const char *error=decision_package_verify(root,path,index,&policy,NULL,NULL,&output);
    if(error)fprintf(stderr,"initial admission: %s\n",error);CHECK(!error&&output.profile==DECISION_ASSETS_IMAGE);
    rejects(-1,path,index,NULL,"invalid_decision_package_policy");rejects(-1,path,index,&policy,"unsafe_decision_package_root");
    memset(&output,0xa5,sizeof(output));DecisionPackageDescriptor before=output;
    CHECK(!strcmp(decision_package_verify(root,path,index,&policy,cancel,NULL,&output),"cancelled"));CHECK(!memcmp(&before,&output,sizeof(output)));
    policy.profile=DECISION_ASSETS_TEXT;rejects(root,path,index,&policy,"unqualified_decision_package_profile");policy.profile=DECISION_ASSETS_IMAGE;
    policy.runtime_revision="aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";rejects(root,path,index,&policy,"unqualified_decision_package_profile");policy.runtime_revision="327d6911fc7f31e779d2fa673e83ba1f3410b468";
    for(unsigned i=0;i<4;i++){policy.pins[i]="aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";rejects(root,path,index,&policy,i<2?"unqualified_decision_package_runtime":"unqualified_decision_package_model");policy.pins[i]=qualified[i];}
    CHECK(!symlinkat("assets",root,"unlisted"));rejects(root,path,index,&policy,NULL);CHECK(!unlinkat(root,"unlisted",0));
    int assets=openat(root,"assets",O_RDONLY|O_DIRECTORY|O_NOFOLLOW);CHECK(assets>=0);
    CHECK(!linkat(assets,decision_asset_name(DECISION_MODEL),root,"alias",0));rejects(root,path,index,&policy,NULL);CHECK(!unlinkat(root,"alias",0));
    int changed=openat(assets,decision_asset_name(DECISION_TOKENIZER),O_WRONLY);CHECK(changed>=0);CHECK(write(changed,"changed!!",9)==9);CHECK(!close(changed));
    rejects(root,path,index,&policy,NULL);put(root,index,3,"tokenizer",9);
    CHECK(!decision_package_verify(root,path,index,&policy,NULL,NULL,&output));
    const char *text_body="{\"format\":\"samosa.inference.decision-package.v1\",\"profile\":\"text\",\"protocol\":\"opendecision-framed-v1\",\"assets\":\"assets\",\"runtime_revision\":\"327d6911fc7f31e779d2fa673e83ba1f3410b468\"}";
    put(root,index,4,text_body,strlen(text_body));policy.profile=DECISION_ASSETS_TEXT;
    CHECK(!decision_package_verify(root,path,index,&policy,NULL,NULL,&output)&&output.profile==DECISION_ASSETS_TEXT);
    put(root,index,4,body,strlen(body));policy.profile=DECISION_ASSETS_IMAGE;
    char moved[256];snprintf(moved,sizeof(moved),"%s-moved",path);CHECK(!rename(path,moved));CHECK(!mkdir(path,0700));
    rejects(root,path,index,&policy,"unsafe_decision_package_root");CHECK(!rmdir(path));CHECK(!rename(moved,path));
    index->count=6;strcpy(index->files[5].name,"provenance.json");index->files[5].role=PACKAGE_METADATA;put(root,index,5,"{}",2);
    CHECK(!decision_package_verify(root,path,index,&policy,NULL,NULL,&output));
    CHECK(!unlinkat(root,"provenance.json",0));index->count=5;index->payload_bytes-=index->files[5].bytes;
    if(exporting) {
        if(!strcmp(argv[1],"--export-text")){put(root,index,4,text_body,strlen(text_body));policy.profile=DECISION_ASSETS_TEXT;}
        CHECK(!decision_package_verify(root,path,index,&policy,NULL,NULL,&output));
        char *json=NULL;size_t bytes=0;FILE *stream=open_memstream(&json,&bytes);CHECK(stream);
        fprintf(stream,"{\"format\":\"samosa.inference.package.v1\",\"platform\":\"%s\",\"minimum_os\":\"1.0\",\"recipe\":\"opendecision-package-v1\",\"files\":[",index->platform);
        const char *role_names[]={"engine","library","model","data","metadata"};
        for(size_t i=0;i<index->count;i++){const PackageFile *file=&index->files[i];if(i)fputc(',',stream);fprintf(stream,"{\"name\":\"%s\",\"role\":\"%s\",\"bytes\":%llu,\"sha256\":\"%s\"}",file->name,role_names[file->role-1],(unsigned long long)file->bytes,file->sha256);}
        fputs("]}",stream);CHECK(!fclose(stream));char digest[65];hash_bytes(json,bytes,digest);
        char index_path[256];snprintf(index_path,sizeof(index_path),"%s.index.json",path);FILE *file=fopen(index_path,"w");CHECK(file);CHECK(!fchmod(fileno(file),0600));CHECK(fwrite(json,1,bytes,file)==bytes);CHECK(!fclose(file));free(json);
        printf("{\"root\":\"%s\",\"index\":\"%s\",\"pin\":\"%s\",\"engine_pin\":\"%s\",\"runtime_pin\":\"%s\",\"model_pin\":\"%s\",\"tokenizer_pin\":\"%s\"}\n",path,index_path,digest,qualified[0],qualified[1],qualified[2],qualified[3]);
        CHECK(!close(assets));CHECK(!close(root));free(index);return 0;
    }
    for(unsigned i=0;i<4;i++)CHECK(!unlinkat(root,index->files[i].name,0));CHECK(!close(assets));
    CHECK(!unlinkat(root,"decision-package.json",0));CHECK(!unlinkat(root,"assets",AT_REMOVEDIR));CHECK(!close(root));CHECK(!rmdir(path));free(index);
    printf("PASS decision package admission: %u checks; independent host pins, held/named roots, closed native graph, metadata separation and atomic refusal; synthetic assets only\n",checks);
    return 0;
}
