#define _POSIX_C_SOURCE 200809L
#include "package_index.h"
#include "sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"package index failure %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static const char digest[]="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
static char *manifest(const char *name,const char *role,const char *size,size_t count){
    char *body=NULL;size_t length;FILE *f=open_memstream(&body,&length);CHECK(f);
    fputs("{\"format\":\"samosa.inference.package.v1\",\"platform\":\"macos-arm64\",\"minimum_os\":\"14.0\",\"recipe\":\"fixture-v1\",\"files\":[",f);
    for(size_t i=0;i<count;i++){
        char numbered[64];snprintf(numbered,sizeof(numbered),"models/part-%zu.bin",i);
        fprintf(f,"%s{\"name\":\"%s\",\"role\":\"%s\",\"bytes\":%s,\"sha256\":\"%s\"}",i?",":"",name?name:numbered,role,size,digest);
    }
    fputs("]}",f);CHECK(!fclose(f));return body;
}
static const char *parse(const char *body,PackageIndex *out){char sha[65];hash_bytes(body,strlen(body),sha);return package_index_parse(body,strlen(body),sha,out);}
static void bad(const char *body,PackageIndex *out,const PackageIndex *before){CHECK(parse(body,out));CHECK(!memcmp(out,before,sizeof(*out)));}
static char *replace(const char *body,const char *needle,const char *value){
    const char *at=strstr(body,needle);CHECK(at);size_t n=strlen(body)+strlen(value)+1;char *out=malloc(n);CHECK(out);
    snprintf(out,n,"%.*s%s%s",(int)(at-body),body,value,at+strlen(needle));return out;
}
int main(void){
    PackageIndex *index=malloc(sizeof(*index)),*before=malloc(sizeof(*before));CHECK(index&&before);
    memset(index,42,sizeof(*index));*before=*index;
    char *body=manifest("models/model.gguf","model","3",1);CHECK(!parse(body,index));
    CHECK(index->count==1&&index->payload_bytes==3&&index->files[0].role==PACKAGE_MODEL);
    CHECK(!strcmp(index->files[0].sha256,digest)&&!strcmp(index->files[0].name,"models/model.gguf"));
    char expected[65];hash_bytes(body,strlen(body),expected);CHECK(!strcmp(index->manifest_sha256,expected));
    CHECK(!package_index_qualify(index,"macos-arm64","14.0"));CHECK(!package_index_qualify(index,"macos-arm64","26.5.1"));
    CHECK(!strcmp(package_index_qualify(index,"linux-arm64","26.5.1"),"unsupported_provider_platform"));
    CHECK(!strcmp(package_index_qualify(index,"macos-arm64","13.9.9"),"unsupported_provider_os"));
    *before=*index;
    CHECK(!strcmp(package_index_parse(body,strlen(body),digest,index),"package_index_checksum"));CHECK(!memcmp(index,before,sizeof(*index)));
    CHECK(package_index_parse(body,PACKAGE_INDEX_BYTES+1,expected,index));CHECK(!memcmp(index,before,sizeof(*index)));
    for(size_t i=0;i<strlen(body);i++){
        char sha[65];hash_bytes(body,i,sha);CHECK(package_index_parse(body,i,sha,index));CHECK(!memcmp(index,before,sizeof(*index)));
    }
    const char *invalid_names[]={"","/absolute","../escape","x/../escape","x/./file","x//file","x/",".hidden","x/.hidden","a/b/c/d/e/f/g/h/i","file\\\\name","file?query","data/!v/other space","data/!variant/file"};
    for(size_t i=0;i<sizeof(invalid_names)/sizeof(*invalid_names);i++){char *b=manifest(invalid_names[i],"data","3",1);bad(b,index,before);free(b);}
    const char *invalid_sizes[]={"0","-1","1.5","null","68719476737","18446744073709551616"};
    for(size_t i=0;i<sizeof(invalid_sizes)/sizeof(*invalid_sizes);i++){char *b=manifest("model","model",invalid_sizes[i],1);bad(b,index,before);free(b);}
    char *b=manifest("data/voices/!v/Mr serious","data","3",1);CHECK(!parse(b,index));free(b);
    *before=*index;b=manifest("data/voices/!v/Mr serious","engine","3",1);bad(b,index,before);free(b);
    const char *needles[]={"fixture-v1","macos-arm64","14.0","samosa.inference.package.v1",digest};
    const char *values[]={"invalid recipe","unknown-platform","14.0.0.0","samosa.inference.package.v2","invalid-sha"};
    *before=*index;
    for(size_t i=0;i<sizeof(needles)/sizeof(*needles);i++){b=replace(body,needles[i],values[i]);bad(b,index,before);free(b);}
    b=replace(body,"\"role\":\"model\"","\"role\":\"unknown\"");bad(b,index,before);free(b);
    b=replace(body,"\"role\":\"model\"","\"role\":\"model\",\"url\":\"https://untrusted\"");bad(b,index,before);free(b);
    b=manifest("duplicate","model","3",2);bad(b,index,before);free(b);
    b=manifest(NULL,"model","3",0);bad(b,index,before);free(b);
    b=manifest(NULL,"model","3",513);bad(b,index,before);free(b);
    b=manifest(NULL,"model","3",512);CHECK(!parse(b,index));CHECK(index->count==512&&index->payload_bytes==1536);free(b);
    b=manifest(NULL,"model","34359738368",2);CHECK(!parse(b,index));CHECK(index->payload_bytes==PACKAGE_PAYLOAD_BYTES);free(b);
    *before=*index;b=manifest(NULL,"model","34359738369",2);bad(b,index,before);free(b);
    b=manifest(NULL,"model","3",2);char *inconsistent=replace(b,"\"bytes\":3","\"bytes\":4");bad(inconsistent,index,before);free(inconsistent);free(b);
    /* A file cannot also be a parent directory, in either manifest order. */
    const char *names[]={"models","models/model.gguf"};
    for(unsigned order=0;order<2;order++){
        b=manifest(names[order],"model","3",1);char extra[512];
        snprintf(extra,sizeof(extra),"},{\"name\":\"%s\",\"role\":\"model\",\"bytes\":3,\"sha256\":\"%s\"}]}",names[1-order],digest);
        char *both=replace(b,"}]}",extra);bad(both,index,before);free(both);free(b);
    }
    const char *aliases[]={"MODELS/model.gguf","models/MODEL.GGUF","Models","MODELS/model.gguf/subfile"};
    for(size_t i=0;i<sizeof(aliases)/sizeof(*aliases);i++){
        char extra[512];snprintf(extra,sizeof(extra),"},{\"name\":\"%s\",\"role\":\"model\",\"bytes\":3,\"sha256\":\"%s\"}]}",aliases[i],digest);
        b=replace(body,"}]}",extra);bad(b,index,before);free(b);
    }
    free(body);free(before);free(index);
    printf("PASS pinned package index: %u checks; full pin, bounded file sets, paths/collisions, deployment and OS policy; no installation/readiness claim\n",checks);
    return 0;
}
