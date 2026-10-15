#define _POSIX_C_SOURCE 200809L
#include "molmo_package_descriptor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"Molmo package descriptor failure %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void rejects(const char *body,size_t bytes,PackageIndex *index){
    MolmoPackageDescriptor out;memset(&out,0xa5,sizeof(out));MolmoPackageDescriptor before=out;
    CHECK(molmo_package_descriptor_parse(body,bytes,index,&out));CHECK(!memcmp(&out,&before,sizeof(out)));
}
int main(void){
    PackageIndex *index=calloc(1,sizeof(*index));CHECK(index);index->count=8;
    const char *names[]={"engine","mlx.metallib","molmo-package.json","model/manifest.json","model/config.json","model/tokenizer.json","model/processor.json","model/weights.safetensors"};
    const enum PackageFileRole roles[]={PACKAGE_ENGINE,PACKAGE_DATA,PACKAGE_METADATA,PACKAGE_METADATA,PACKAGE_METADATA,PACKAGE_DATA,PACKAGE_METADATA,PACKAGE_MODEL};
    for(size_t i=0;i<8;i++){strcpy(index->files[i].name,names[i]);index->files[i].role=roles[i];index->files[i].bytes=i+1;memset(index->files[i].sha256,'a',64);}
    const char *body="{\"format\":\"samosa.inference.molmo-package.v1\",\"profile\":\"molmo2-4b-mlx-q4-v1\",\"engine\":\"engine\",\"model_directory\":\"model\"}";
    MolmoPackageDescriptor out;CHECK(!molmo_package_descriptor_parse(body,strlen(body),index,&out));CHECK(out.engine==0&&out.kernels==1&&out.manifest==3);
    for(size_t n=0;n<strlen(body);n++)rejects(body,n,index);
    char extra[4096];snprintf(extra,sizeof(extra),"%.*s,\"command\":\"/bin/sh\"}",(int)strlen(body)-1,body);rejects(extra,strlen(extra),index);
    for(size_t i=0;i<8;i++){enum PackageFileRole old=index->files[i].role;index->files[i].role=PACKAGE_ENGINE;if(i)rejects(body,strlen(body),index);index->files[i].role=old;}
    for(size_t i=0;i<8;i++){uint64_t old=index->files[i].bytes;index->files[i].bytes=0;rejects(body,strlen(body),index);index->files[i].bytes=old;}
    strcpy(index->files[0].name,"bin/engine");rejects(body,strlen(body),index);strcpy(index->files[0].name,names[0]);
    index->files[1].bytes=UINT64_C(268435457);rejects(body,strlen(body),index);index->files[1].bytes=2;
    TemporalPackageReceipt *receipt=calloc(1,sizeof(*receipt));CHECK(receipt);receipt->count=4;
    for(size_t i=0;i<4;i++){strcpy(receipt->files[i].name,names[i+4]+6);strcpy(receipt->files[i].sha256,index->files[i+4].sha256);receipt->files[i].bytes=index->files[i+4].bytes;}
    CHECK(!molmo_package_bind_model(index,receipt));
    /* Independently valid manifest metadata cannot bind different generic files. */
    for(size_t i=4;i<8;i++){
        index->files[i].bytes++;CHECK(molmo_package_bind_model(index,receipt));index->files[i].bytes--;
        index->files[i].sha256[0]='b';CHECK(molmo_package_bind_model(index,receipt));index->files[i].sha256[0]='a';
        enum PackageFileRole old=index->files[i].role;index->files[i].role=PACKAGE_LIBRARY;CHECK(molmo_package_bind_model(index,receipt));index->files[i].role=old;
    }
    index->count=7;CHECK(molmo_package_bind_model(index,receipt));index->count=8;
    index->files[8]=index->files[4];strcpy(index->files[8].name,"model/foreign.json");index->count=9;CHECK(molmo_package_bind_model(index,receipt));
    index->files[8]=index->files[4];CHECK(molmo_package_bind_model(index,receipt));index->count=8;
    receipt->count=65;CHECK(molmo_package_bind_model(index,receipt));receipt->count=4;
    CHECK(!molmo_package_bind_model(index,receipt));free(receipt);free(index);
    printf("PASS Molmo package descriptor: %u checks; strict profile/layout, unchanged failure output and exact native manifest/index binding; no model load\n",checks);return 0;
}
