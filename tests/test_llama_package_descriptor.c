#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "llama_package_descriptor.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"llama descriptor failure %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void failure(const char *body,PackageIndex *index,const char *expected){
    LlamaPackageDescriptor result;memset(&result,0xa5,sizeof(result));LlamaPackageDescriptor before=result;
    const char *e=llama_package_descriptor_parse(body,strlen(body),index,&result);CHECK(e&&!strcmp(e,expected));CHECK(!memcmp(&result,&before,sizeof(result)));
}
int main(int argc,char **argv){
    PackageIndex *index=calloc(1,sizeof(*index));CHECK(index);index->count=2;strcpy(index->files[0].name,"runtime/llama-server");index->files[0].role=PACKAGE_ENGINE;strcpy(index->files[1].name,"models/model.gguf");index->files[1].role=PACKAGE_MODEL;
    const char *body="{\"format\":\"samosa.inference.llama-package.v1\",\"kind\":\"embedding\",\"runtime_revision\":\"4fbc76dec51d0add466f0210855c0596589b60d4\",\"engine\":\"runtime/llama-server\",\"model\":\"models/model.gguf\",\"architecture\":\"gemma-embedding2\"}";
    LlamaPackageDescriptor result;CHECK(!llama_package_descriptor_parse(body,strlen(body),index,&result));CHECK(result.engine==0&&result.model==1&&!strcmp(result.kind,"embedding"));
    GgufMetadata model={.version=3,.tensors=413};strcpy(model.architecture,"gemma-embedding2");CHECK(!llama_package_descriptor_model(&result,&model));
    if(argc==2){int fd=open(argv[1],O_RDONLY|O_NOFOLLOW);CHECK(fd>=0);CHECK(!gguf_metadata_read(fd,NULL,NULL,&model));CHECK(!llama_package_descriptor_model(&result,&model));CHECK(!close(fd));free(index);puts("PASS actual GGUF architecture bound to llama package descriptor; runtime qualification remains separate");return 0;}
    CHECK(argc==1);
    const char *summary_body="{\"format\":\"samosa.inference.llama-package.v1\",\"kind\":\"summary\",\"runtime_revision\":\"9fcaed763ccda38ea81068ad9d7f991aaddca451\",\"engine\":\"runtime/llama-server\",\"model\":\"models/model.gguf\",\"architecture\":\"t5\"}";
    LlamaPackageDescriptor summary;GgufMetadata t5={.version=3,.tensors=131};strcpy(t5.architecture,"t5");
    CHECK(!llama_package_descriptor_parse(summary_body,strlen(summary_body),index,&summary));CHECK(!strcmp(summary.kind,"summary"));CHECK(!llama_package_descriptor_model(&summary,&t5));
    strcpy(summary.architecture,"gemma-embedding2");strcpy(t5.architecture,"gemma-embedding2");CHECK(!strcmp(llama_package_descriptor_model(&summary,&t5),"incompatible_llama_package_model"));
    memset(summary.kind,'x',sizeof(summary.kind));CHECK(!strcmp(llama_package_descriptor_model(&summary,&t5),"invalid_llama_package_descriptor"));
    strcpy(summary.kind,"unknown");CHECK(!strcmp(llama_package_descriptor_model(&summary,&t5),"invalid_llama_package_descriptor"));
    const char *unknown_kind="{\"format\":\"samosa.inference.llama-package.v1\",\"kind\":\"whisper\",\"runtime_revision\":\"9fcaed763ccda38ea81068ad9d7f991aaddca451\",\"engine\":\"runtime/llama-server\",\"model\":\"models/model.gguf\",\"architecture\":\"t5\"}";
    failure(unknown_kind,index,"invalid_llama_package_descriptor");
    strcpy(model.architecture,"qwen35");CHECK(!strcmp(llama_package_descriptor_model(&result,&model),"incompatible_llama_package_model"));strcpy(model.architecture,"gemma-embedding2");model.tensors=0;CHECK(!strcmp(llama_package_descriptor_model(&result,&model),"incompatible_llama_package_model"));model.tensors=413;model.version=2;CHECK(!strcmp(llama_package_descriptor_model(&result,&model),"incompatible_llama_package_model"));
    const char *invalid[]={"{}","{\"format\":\"samosa.inference.llama-package.v1\",\"kind\":\"embedding\",\"runtime_revision\":\"bad\",\"engine\":\"runtime/llama-server\",\"model\":\"models/model.gguf\",\"architecture\":\"gemma-embedding2\"}","{\"format\":\"samosa.inference.llama-package.v1\",\"kind\":\"embedding\",\"runtime_revision\":\"4fbc76dec51d0add466f0210855c0596589b60d4\",\"engine\":\"../bin/sh\",\"model\":\"models/model.gguf\",\"architecture\":\"gemma-embedding2\"}"};
    for(size_t i=0;i<sizeof(invalid)/sizeof(*invalid);i++)failure(invalid[i],index,"invalid_llama_package_descriptor");
    char extra[4096];size_t length=strlen(body);memcpy(extra,body,length-1);strcpy(extra+length-1,",\"command\":\"ignored\"}");failure(extra,index,"invalid_llama_package_descriptor");
    memcpy(extra,body,length-1);strcpy(extra+length-1,",\"kind\":\"text\"}");failure(extra,index,"invalid_llama_package_descriptor");
    index->files[1].role=PACKAGE_DATA;failure(body,index,"llama_package_identity_conflict");index->files[1].role=PACKAGE_MODEL;
    index->count=3;index->files[2]=index->files[1];strcpy(index->files[2].name,"models/other.gguf");failure(body,index,"llama_package_identity_conflict");index->count=2;
    strcpy(index->files[0].name,"runtime/other");failure(body,index,"llama_package_identity_conflict");strcpy(index->files[0].name,"runtime/llama-server");
    for(size_t n=0;n<length;n++){LlamaPackageDescriptor observed;memset(&observed,0xa5,sizeof(observed));LlamaPackageDescriptor before=observed;CHECK(llama_package_descriptor_parse(body,n,index,&observed));CHECK(!memcmp(&observed,&before,sizeof(observed)));}
    free(index);printf("PASS llama descriptor: %u checks; strict declarations, pinned index roles, GGUF identity and truncation\n",checks);return 0;
}
