#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "gguf_metadata.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#define METADATA_BYTES UINT64_C(67108864)
#define METADATA_PAIRS 8192u
#define ARRAY_ELEMENTS UINT64_C(1048576)
typedef struct {int fd;uint64_t size,position,start,last_control;size_t available;
    unsigned char buffer[65536];const char *(*control)(void *);void *context;} Reader;
static uint32_t u32(const unsigned char *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static uint64_t u64(const unsigned char *p){return u32(p)|(uint64_t)u32(p+4)<<32;}
static const char *advance(Reader *r,uint64_t n){
    if(r->position>r->size||n>r->size-r->position)return "invalid_gguf_metadata";
    if(r->position>METADATA_BYTES||n>METADATA_BYTES-r->position)return "gguf_metadata_limit";
    r->position+=n;
    if(r->position-r->last_control>=sizeof(r->buffer)){r->last_control=r->position;return r->control?r->control(r->context):NULL;}return NULL;
}
static const char *read_bytes(Reader *r,void *out,size_t n){
    unsigned char *p=out;
    while(n){
        if(r->position>=r->size)return "invalid_gguf_metadata";
        if(r->position>=METADATA_BYTES)return "gguf_metadata_limit";
        if(r->position<r->start||r->position-r->start>=r->available){
            const char *e=r->control?r->control(r->context):NULL;if(e)return e;
            size_t want=sizeof(r->buffer);if(r->size-r->position<want)want=(size_t)(r->size-r->position);
            if(METADATA_BYTES-r->position<want)want=(size_t)(METADATA_BYTES-r->position);
            ssize_t got=pread(r->fd,r->buffer,want,(off_t)r->position);if(got<0&&errno==EINTR)continue;if(got<=0)return "gguf_metadata_read_failed";
            r->start=r->position;r->available=(size_t)got;
        }
        size_t at=(size_t)(r->position-r->start),take=r->available-at;if(take>n)take=n;
        memcpy(p,r->buffer+at,take);const char *e=advance(r,take);if(e)return e;p+=take;n-=take;
    }return NULL;
}
static const char *string(Reader *r,char *out,size_t capacity){
    unsigned char length[8];const char *e=read_bytes(r,length,8);if(e)return e;uint64_t n=u64(length);
    if(!out)return advance(r,n);
    if(!n||n>=capacity)return "gguf_metadata_limit";
    e=read_bytes(r,out,(size_t)n);if(e)return e;
    if(memchr(out,0,(size_t)n))return "invalid_gguf_metadata";out[n]=0;return NULL;
}
static const char *value(Reader *r,uint32_t type){
    static const unsigned widths[13]={1,1,2,2,4,4,4,1,0,0,8,8,8};
    if(type>=13||type==9)return "invalid_gguf_metadata";
    if(type==8)return string(r,NULL,0);
    if(type==7){unsigned char b;const char *e=read_bytes(r,&b,1);return e?e:b>1?"invalid_gguf_metadata":NULL;}
    return advance(r,widths[type]);
}
static int compare(const void *a,const void *b){return strcmp(a,b);}
static int unchanged(const struct stat *a,const struct stat *b){
    if(a->st_dev!=b->st_dev||a->st_ino!=b->st_ino||a->st_size!=b->st_size||a->st_mode!=b->st_mode)return 0;
#ifdef __APPLE__
    return a->st_mtimespec.tv_sec==b->st_mtimespec.tv_sec&&a->st_mtimespec.tv_nsec==b->st_mtimespec.tv_nsec&&a->st_ctimespec.tv_sec==b->st_ctimespec.tv_sec&&a->st_ctimespec.tv_nsec==b->st_ctimespec.tv_nsec;
#else
    return a->st_mtim.tv_sec==b->st_mtim.tv_sec&&a->st_mtim.tv_nsec==b->st_mtim.tv_nsec&&a->st_ctim.tv_sec==b->st_ctim.tv_sec&&a->st_ctim.tv_nsec==b->st_ctim.tv_nsec;
#endif
}
const char *gguf_metadata_read(int fd,const char *(*control)(void *),void *context,GgufMetadata *out){
    if(!out)return "invalid_configuration";const char *e=control?control(context):NULL;if(e)return e;
    struct stat before,after;if(fstat(fd,&before)||!S_ISREG(before.st_mode)||before.st_size<24||(fcntl(fd,F_GETFL)&O_ACCMODE)!=O_RDONLY)return "invalid_gguf_metadata";
    Reader reader={.fd=fd,.size=(uint64_t)before.st_size,.control=control,.context=context};unsigned char header[24];e=read_bytes(&reader,header,sizeof(header));if(e)return e;
    if(memcmp(header,"GGUF",4))return "invalid_gguf_metadata";
    GgufMetadata result={.version=u32(header+4),.tensors=u64(header+8),.pairs=u64(header+16)};
    if(result.version!=3)return "unsupported_gguf_version";
    if(!result.pairs||result.pairs>METADATA_PAIRS||result.tensors>ARRAY_ELEMENTS)return "gguf_metadata_limit";
    char (*keys)[256]=calloc((size_t)result.pairs,sizeof(*keys));if(!keys)return "resource_limit";
    unsigned architectures=0;
    for(size_t i=0;!e&&i<result.pairs;i++){
        e=string(&reader,keys[i],sizeof(keys[i]));unsigned char bytes[12];if(!e)e=read_bytes(&reader,bytes,4);if(e)break;uint32_t type=u32(bytes);
        if(!strcmp(keys[i],"general.architecture")){
            if(type!=8||architectures++){e="invalid_gguf_metadata";break;}
            e=string(&reader,result.architecture,sizeof(result.architecture));
            for(const unsigned char *p=(const unsigned char *)result.architecture;!e&&*p;p++)if(!((*p>='a'&&*p<='z')||(*p>='0'&&*p<='9')||strchr("._-",*p)))e="invalid_gguf_metadata";
        }else if(type==9){
            e=read_bytes(&reader,bytes,12);if(e)break;uint32_t element=u32(bytes);uint64_t count=u64(bytes+4);
            if(element>=13||element==9){e="invalid_gguf_metadata";break;}
            if(count>ARRAY_ELEMENTS){e="gguf_metadata_limit";break;}
            for(uint64_t at=0;!e&&at<count;at++)e=value(&reader,element);
        }else e=value(&reader,type);
    }
    if(!e&&architectures!=1)e="invalid_gguf_metadata";
    if(!e){qsort(keys,(size_t)result.pairs,sizeof(*keys),compare);for(size_t i=1;i<result.pairs;i++)if(!strcmp(keys[i-1],keys[i])){e="invalid_gguf_metadata";break;}}
    if(!e)e=control?control(context):NULL;
    if(!e&&(fstat(fd,&after)||!unchanged(&before,&after)))e="artifact_changed";
    if(!e){result.metadata_bytes=reader.position;*out=result;}
    free(keys);return e;
}
