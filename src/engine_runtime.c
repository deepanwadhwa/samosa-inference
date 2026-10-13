#define _POSIX_C_SOURCE 200809L
#include "engine_runtime.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
extern char **environ;
static uint32_t u32(const unsigned char *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static uint64_t u64(const unsigned char *p){return u32(p)|(uint64_t)u32(p+4)<<32;}
static int read_at(int fd,void *out,size_t n,uint64_t offset,uint64_t size){
    if(offset>size||n>size-offset)return 0;
    unsigned char *p=out;while(n){ssize_t got=pread(fd,p,n,(off_t)offset);if(got<0&&errno==EINTR)continue;
        if(got<=0)return 0;
        p+=got;n-=(size_t)got;offset+=(uint64_t)got;}return 1;
}
const char *engine_runtime_environment(void){
    for(char **p=environ;*p;p++)if(!strncmp(*p,"DYLD_",5)||!strncmp(*p,"LD_",3)||!strncmp(*p,"GGML_",5)||!strncmp(*p,"LLAMA_",6))
        return "unsafe_engine_environment";
    return NULL;
}
static int os_mach_path(const char *p){return (!strncmp(p,"/usr/lib/",9)||!strncmp(p,"/System/Library/",16))&&
    !strstr(p,"/../")&&!strstr(p,"/./")&&!strstr(p,"//")&&strcmp(p+strlen(p)-1,".");}
static const char *mach(int fd,const unsigned char *h,uint64_t size){
    if(u32(h+4)!=0x100000c||u32(h+12)!=2)return "unsupported_engine_binary";
    uint32_t count=u32(h+16),length=u32(h+20);if(count>1024||length>65536)return "engine_loader_limit";
    unsigned char *commands=malloc(length?length:1);if(!commands)return "engine_resource_limit";
    const char *e=NULL;if(!read_at(fd,commands,length,32,size))e="invalid_engine_loader";
    size_t offset=0;
    for(uint32_t i=0;!e&&i<count;i++){
        if(offset>length||length-offset<8){e="invalid_engine_loader";break;}
        unsigned char *c=commands+offset;uint32_t cmd=u32(c),n=u32(c+4);
        if(n<8||n>length-offset||(n&7)){e="invalid_engine_loader";break;}
        if(cmd==0x8000001c||cmd==0x27)e="unqualified_engine_dependencies";
        if(cmd==0xe){uint32_t name=n>=12?u32(c+8):0;
            if(name<12||name>=n||!memchr(c+name,0,n-name)||strcmp((const char*)c+name,"/usr/lib/dyld"))e="unqualified_engine_dependencies";}
        if(cmd==0xc||cmd==0x80000018||cmd==0x8000001f||cmd==0x80000023||cmd==0x20){
            if(n<24)e="invalid_engine_loader";
            else{uint32_t start=u32(c+8);
                if(start<24||start>=n||!memchr(c+start,0,n-start))e="invalid_engine_loader";
                else if(!os_mach_path((const char*)c+start))e="unqualified_engine_dependencies";}
        }
        offset+=n;
    }
    if(!e&&offset!=length)e="invalid_engine_loader";
    free(commands);return e;
}
static int os_elf_library(const char *name){
    static const char *const allowed[]={"libc.so.6","libm.so.6","libstdc++.so.6","libgcc_s.so.1","libgomp.so.1","libpthread.so.0","libdl.so.2","librt.so.1",NULL};
    for(size_t i=0;allowed[i];i++)if(!strcmp(name,allowed[i]))return 1;
    return 0;
}
static const char *elf(int fd,const unsigned char *h,uint64_t size){
    if(h[4]!=2||h[5]!=1||h[6]!=1||(h[16]!=2&&h[16]!=3)||h[17]||h[18]!=183||h[19]||!u64(h+24))return "unsupported_engine_binary";
    uint64_t start=u64(h+32);uint32_t count=(uint32_t)h[56]|(uint32_t)h[57]<<8;
    if(h[54]!=56||h[55]||!count||count>128)return "engine_loader_limit";
    unsigned char headers[128*56];if(!read_at(fd,headers,count*56,start,size))return "invalid_engine_loader";
    uint64_t dyn_offset=0,dyn_size=0;unsigned dynamic_count=0;
    for(uint32_t i=0;i<count;i++){unsigned char *p=headers+i*56;uint32_t type=u32(p);uint64_t offset=u64(p+8),length=u64(p+32);
        if(offset>size||length>size-offset)return "invalid_engine_loader";
        if(type==2){dyn_offset=offset;dyn_size=length;dynamic_count++;}
        if(type==3){char interpreter[128];if(length<2||length>sizeof(interpreter)||!read_at(fd,interpreter,(size_t)length,offset,size)||
                !memchr(interpreter,0,(size_t)length)||strcmp(interpreter,"/lib/ld-linux-aarch64.so.1"))return "unqualified_engine_dependencies";}
    }
    if(!dynamic_count)return NULL;
    if(dynamic_count!=1||!dyn_size||dyn_size>16384||dyn_size%16)return "engine_loader_limit";
    unsigned char dynamic[16384];if(!read_at(fd,dynamic,(size_t)dyn_size,dyn_offset,size))return "invalid_engine_loader";
    uint64_t strings=0,string_size=0;unsigned terminated=0;
    for(size_t i=0;i<dyn_size;i+=16){uint64_t tag=u64(dynamic+i),value=u64(dynamic+i+8);if(!tag){terminated=1;break;}
        if(tag==5){if(strings)return "invalid_engine_loader";strings=value;}
        if(tag==10){if(string_size)return "invalid_engine_loader";string_size=value;}
        if(tag==15||tag==29||tag==0x7ffffffd||tag==0x7fffffff||tag==0x6ffffefb||tag==0x6ffffefc)return "unqualified_engine_dependencies";
    }
    if(!terminated||!strings||!string_size||string_size>65536)return "invalid_engine_loader";
    uint64_t table_offset=0;unsigned matches=0;
    for(uint32_t i=0;i<count;i++){unsigned char *p=headers+i*56;uint64_t address=u64(p+16),length=u64(p+32);
        if(u32(p)==1&&strings>=address&&strings-address<=length&&string_size<=length-(strings-address)){
            table_offset=u64(p+8)+(strings-address);matches++;}}
    if(matches!=1)return "invalid_engine_loader";
    char *table=malloc((size_t)string_size);if(!table)return "engine_resource_limit";
    const char *e=NULL;if(!read_at(fd,table,(size_t)string_size,table_offset,size))e="invalid_engine_loader";
    for(size_t i=0;!e&&i<dyn_size;i+=16){uint64_t tag=u64(dynamic+i),value=u64(dynamic+i+8);if(!tag)break;
        if(tag==1){if(value>=string_size||!memchr(table+value,0,(size_t)(string_size-value)))e="invalid_engine_loader";
            else if(!os_elf_library(table+value))e="unqualified_engine_dependencies";}}
    free(table);return e;
}
const char *engine_system_runtime(int fd){
    struct stat s;unsigned char header[64];if(fstat(fd,&s)||!S_ISREG(s.st_mode)||s.st_size<64||
        !read_at(fd,header,sizeof(header),0,(uint64_t)s.st_size))return "unsupported_engine_binary";
    if(u32(header)==0xfeedfacf)return mach(fd,header,(uint64_t)s.st_size);
    if(!memcmp(header,"\177ELF",4))return elf(fd,header,(uint64_t)s.st_size);
    return "unsupported_engine_binary";
}
