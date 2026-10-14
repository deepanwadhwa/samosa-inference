#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "gguf_metadata.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"GGUF metadata failure %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void w32(unsigned char *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(unsigned char)(v>>(8*i));}
static void w64(unsigned char *p,uint64_t v){for(unsigned i=0;i<8;i++)p[i]=(unsigned char)(v>>(8*i));}
static size_t text(unsigned char *b,size_t at,const char *s){size_t n=strlen(s);w64(b+at,n);memcpy(b+at+8,s,n);return at+8+n;}
static size_t field(unsigned char *b,size_t at,const char *key,uint32_t type){at=text(b,at,key);w32(b+at,type);return at+4;}
static const char *cancel(void *unused){(void)unused;return "cancelled";}
static void put(int fd,const void *b,size_t n){CHECK(!ftruncate(fd,0));CHECK(pwrite(fd,b,n,0)==(ssize_t)n);CHECK(!fsync(fd));}
int main(int argc,char **argv){
    if(argc==2){int fd=open(argv[1],O_RDONLY|O_NOFOLLOW);CHECK(fd>=0);GgufMetadata metadata;const char *e=gguf_metadata_read(fd,NULL,NULL,&metadata);if(e)fprintf(stderr,"actual GGUF metadata: %s\n",e);CHECK(!e);CHECK(!close(fd));
        printf("PASS actual GGUF v%u: architecture %s; %llu tensors; %llu keys; %llu metadata bytes\n",metadata.version,metadata.architecture,(unsigned long long)metadata.tensors,(unsigned long long)metadata.pairs,(unsigned long long)metadata.metadata_bytes);return 0;}
    CHECK(argc==1);char path[]="/tmp/samosa-gguf-metadata-XXXXXX";int writer=mkstemp(path);CHECK(writer>=0);int reader=open(path,O_RDONLY|O_NOFOLLOW);CHECK(reader>=0);
    unsigned char b[4096]={0};memcpy(b,"GGUF",4);w32(b+4,3);w64(b+8,1);w64(b+16,3);
    size_t at=field(b,24,"tokenizer.tokens",9),array=at;w32(b+at,8);w64(b+at+4,2);at+=12;at=text(b,at,"hello");at=text(b,at,"");
    at=field(b,at,"some.flag",7);size_t flag=at;b[at++]=1;
    size_t architecture=at;at=field(b,at,"general.architecture",8);size_t value=at;at=text(b,at,"embeddinggemma");size_t valid_size=at;
    put(writer,b,at);CHECK(lseek(reader,1,SEEK_SET)==1);GgufMetadata observed;memset(&observed,0xa5,sizeof(observed));GgufMetadata before=observed;
    CHECK(!strcmp(gguf_metadata_read(reader,cancel,NULL,&observed),"cancelled"));CHECK(!memcmp(&observed,&before,sizeof(observed)));
    CHECK(!gguf_metadata_read(reader,NULL,NULL,&observed));CHECK(!strcmp(observed.architecture,"embeddinggemma")&&observed.version==3&&observed.tensors==1&&observed.pairs==3&&observed.metadata_bytes==valid_size);CHECK(lseek(reader,0,SEEK_CUR)==1);before=observed;
    CHECK(!strcmp(gguf_metadata_read(writer,NULL,NULL,&observed),"invalid_gguf_metadata"));CHECK(!memcmp(&observed,&before,sizeof(observed)));
    for(size_t n=0;n<valid_size;n++){put(writer,b,n);CHECK(gguf_metadata_read(reader,NULL,NULL,&observed));CHECK(!memcmp(&observed,&before,sizeof(observed)));}
    w32(b+4,2);put(writer,b,valid_size);CHECK(!strcmp(gguf_metadata_read(reader,NULL,NULL,&observed),"unsupported_gguf_version"));w32(b+4,3);
    b[flag]=2;put(writer,b,valid_size);CHECK(!strcmp(gguf_metadata_read(reader,NULL,NULL,&observed),"invalid_gguf_metadata"));b[flag]=1;
    w32(b+array,9);put(writer,b,valid_size);CHECK(!strcmp(gguf_metadata_read(reader,NULL,NULL,&observed),"invalid_gguf_metadata"));w32(b+array,8);
    w64(b+array+4,1048577);put(writer,b,valid_size);CHECK(!strcmp(gguf_metadata_read(reader,NULL,NULL,&observed),"gguf_metadata_limit"));w64(b+array+4,2);
    w64(b+16,8193);put(writer,b,valid_size);CHECK(!strcmp(gguf_metadata_read(reader,NULL,NULL,&observed),"gguf_metadata_limit"));w64(b+16,3);
    b[value+8]=0;put(writer,b,valid_size);CHECK(!strcmp(gguf_metadata_read(reader,NULL,NULL,&observed),"invalid_gguf_metadata"));b[value+8]='e';
    w32(b+architecture+8+strlen("general.architecture"),4);put(writer,b,valid_size);CHECK(!strcmp(gguf_metadata_read(reader,NULL,NULL,&observed),"invalid_gguf_metadata"));w32(b+architecture+8+strlen("general.architecture"),8);
    at=field(b,valid_size,"general.architecture",8);at=text(b,at,"llama");w64(b+16,4);put(writer,b,at);CHECK(!strcmp(gguf_metadata_read(reader,NULL,NULL,&observed),"invalid_gguf_metadata"));
    at=field(b,valid_size,"some.flag",7);b[at++]=0;put(writer,b,at);CHECK(!strcmp(gguf_metadata_read(reader,NULL,NULL,&observed),"invalid_gguf_metadata"));
    memset(b,0,sizeof(b));memcpy(b,"GGUF",4);w32(b+4,3);w64(b+8,1);w64(b+16,2);
    at=field(b,24,"padding",8);w64(b+at,67108864);put(writer,b,at+8);CHECK(!ftruncate(writer,67112960));
    CHECK(!strcmp(gguf_metadata_read(reader,NULL,NULL,&observed),"gguf_metadata_limit"));
    w64(b+at,UINT64_MAX);put(writer,b,at+8);CHECK(!strcmp(gguf_metadata_read(reader,NULL,NULL,&observed),"invalid_gguf_metadata"));
    CHECK(!memcmp(&observed,&before,sizeof(observed)));CHECK(!close(reader));CHECK(!close(writer));CHECK(!unlink(path));
    printf("PASS GGUF metadata: %u checks; bounded framing, architecture, duplicates, types, truncation and controls\n",checks);return 0;
}
