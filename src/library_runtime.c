#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "library_runtime.h"
#include "engine_runtime.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef __APPLE__
#include <sys/sysctl.h>
#endif
#if defined(__linux__) && defined(__GLIBC__)
#include <gnu/libc-version.h>
#endif
static uint32_t u32(const unsigned char *p) { return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static uint64_t u64(const unsigned char *p) { return u32(p)|(uint64_t)u32(p+4)<<32; }
static int read_at(int fd,void *out,size_t n,uint64_t offset,uint64_t size) {
    if(offset>size||n>size-offset)return 0;
    unsigned char *p=out;
    while(n) {
        ssize_t got=pread(fd,p,n,(off_t)offset);
        if(got<0&&errno==EINTR)continue;
        if(got<=0)return 0;
        p+=got;n-=(size_t)got;offset+=(uint64_t)got;
    }
    return 1;
}
static int version(const char *text,uint32_t *out) {
    unsigned parts[3]={0};unsigned count=0;
    if(!text||!*text)return 0;
    for(;;) {
        if(count==3||*text<'0'||*text>'9')return 0;
        unsigned v=0;
        do {v=v*10+(unsigned)(*text++-'0');if(v>(count?255u:65535u))return 0;} while(*text>='0'&&*text<='9');
        parts[count++]=v;
        if(!*text)break;
        if(*text++!='.')return 0;
    }
    *out=parts[0]<<16|parts[1]<<8|parts[2];return 1;
}
const char *library_runtime_host_policy(LibraryRuntimePolicy *p) {
    if(!p)return "invalid_configuration";
    memset(p,0,sizeof(*p));
#ifdef __APPLE__
    p->mach_o=1;
    char os[64];size_t n=sizeof(os);
    if(sysctlbyname("kern.osproductversion",os,&n,NULL,0)||!n||n>sizeof(os)||
        !memchr(os,0,n)||!version(os,&p->os_version))return "unsupported_library_platform";
#if defined(__aarch64__)
    p->cpu=0x100000c;
#elif defined(__x86_64__)
    p->cpu=0x1000007;
#else
    return "unsupported_library_platform";
#endif
#elif defined(__linux__)
#if defined(__aarch64__)
    p->cpu=183;
#elif defined(__x86_64__)
    p->cpu=62;
#else
    return "unsupported_library_platform";
#endif
#if defined(__GLIBC__)
    if(!version(gnu_get_libc_version(),&p->glibc_version))return "unsupported_library_platform";
#else
    return "unsupported_library_platform";
#endif
#else
    return "unsupported_library_platform";
#endif
    return NULL;
}
static const char *cstring(const unsigned char *b,size_t n,size_t start,size_t minimum) {
    if(start<minimum||start>=n)return NULL;
    const unsigned char *end=memchr(b+start,0,n-start);
    if(!end||end==b+start||(size_t)(end-(b+start))>255)return NULL;
    return (const char *)b+start;
}
static int bare_name(const char *name) {
    if(!name||!*name||*name=='.')return 0;
    for(const unsigned char *p=(const unsigned char *)name;*p;p++)
        if(!((*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||(*p>='0'&&*p<='9')||strchr("-_.",*p)))return 0;
    return 1;
}
static const char *dependency(LibraryRuntimeImage *image,const char *name,uint32_t compatibility) {
    if(image->count==LIBRARY_DEPENDENCIES)return "library_loader_limit";
    for(size_t i=0;i<image->count;i++)if(!strcmp(image->dependencies[i].name,name))return "invalid_library_loader";
    strcpy(image->dependencies[image->count].name,name);
    image->dependencies[image->count++].compatibility_version=compatibility;return NULL;
}
static const char *mach(int fd,const unsigned char *h,uint64_t size,const LibraryRuntimePolicy *policy,LibraryRuntimeImage *image,int executable) {
    uint32_t subtype=u32(h+8)&0xffffff;
    if(u32(h+4)!=policy->cpu||u32(h+12)!=(executable?2u:6u)||
        (policy->cpu==0x100000c?subtype!=0:subtype!=3)||!(u32(h+24)&0x80)||(u32(h+24)&0x100))return "incompatible_library_binary";
    uint32_t count=u32(h+16),length=u32(h+20);
    if(!count||count>1024||length>65536)return "library_loader_limit";
    unsigned char *commands=malloc(length?length:1);
    if(!commands)return "resource_limit";
    const char *e=NULL;if(!read_at(fd,commands,length,32,size))e="invalid_library_loader";
    size_t offset=0;unsigned min_count=0,id_count=0,interpreter_count=0,entry_count=0;
    for(uint32_t i=0;!e&&i<count;i++) {
        if(offset>length||length-offset<8){e="invalid_library_loader";break;}
        unsigned char *c=commands+offset;uint32_t cmd=u32(c),n=u32(c+4);
        if(n<8||n>length-offset||(n&7)){e="invalid_library_loader";break;}
        if(cmd==0xe&&executable){const char *path=n>=12?cstring(c,n,u32(c+8),12):NULL;
            if(!path||interpreter_count++)e="invalid_library_loader";
            else if(strcmp(path,"/usr/lib/dyld"))e="unqualified_library_dependencies";
        }else if(cmd==0x80000028&&executable){
            if(n!=24||entry_count++||!u64(c+8)||u64(c+8)>=size)e="invalid_library_loader";
        }else if(cmd==0x27||cmd==0xe||cmd==0x20)e="unqualified_library_dependencies";
        else if(cmd==0x8000001c) {
            const char *path=n>=12?cstring(c,n,u32(c+8),12):NULL;
            if(!path)e="invalid_library_loader";
            else if(image->origin_search||strcmp(path,"@loader_path"))e="unqualified_library_dependencies";
            else image->origin_search=1;
        } else if(cmd==0xd||cmd==0xc||cmd==0x80000018||cmd==0x8000001f||cmd==0x80000023) {
            const char *name=n>=24?cstring(c,n,u32(c+8),24):NULL;
            if(!name)e="invalid_library_loader";
            else if(cmd==0xd) {
                const char *base=NULL;
                if(!strncmp(name,"@rpath/",7))base=name+7;
                else if(!strncmp(name,"@loader_path/",13))base=name+13;
                if(executable||id_count++)e="invalid_library_loader";
                else if(!bare_name(base))e="unqualified_library_dependencies";
                else image->current_version=u32(c+16);
            } else e=dependency(image,name,u32(c+20));
        } else if(cmd==0x32) {
            if(n<24||u32(c+8)!=1||u32(c+20)>8||n!=24+u32(c+20)*8||min_count++)e="invalid_library_loader";
            else image->minimum_os=u32(c+12);
        } else if(cmd==0x24) {
            if(n!=16||min_count++)e="invalid_library_loader";
            else image->minimum_os=u32(c+8);
        } else if(cmd==0x25||cmd==0x2f||cmd==0x30)e="incompatible_library_binary";
        else if(cmd==0x19) {
            if(n<72||u32(c+64)>(n-72)/80||n!=72+u32(c+64)*80||
                u64(c+40)>size||u64(c+48)>size-u64(c+40)||u64(c+48)>u64(c+32))e="invalid_library_loader";
        } else if(cmd==0x2c) {
            if(n!=24)e="invalid_library_loader";
            else if(u32(c+16))e="unqualified_library_dependencies";
        } else if((cmd&0x80000000)&&cmd!=0x80000022&&cmd!=0x80000033&&cmd!=0x80000034)
            e="unqualified_library_dependencies";
        offset+=n;
    }
    if(!e&&(offset!=length||min_count!=1||id_count!=(executable?0u:1u)||!image->minimum_os||
        (executable&&(interpreter_count!=1||entry_count!=1))))e="invalid_library_loader";
    if(!e&&image->minimum_os>policy->os_version)e="incompatible_library_os";
    free(commands);return e;
}
typedef struct {const unsigned char *headers;uint32_t count;uint64_t size;} ElfMap;
static int mapped(const ElfMap *m,uint64_t address,size_t bytes,uint64_t *offset) {
    unsigned matches=0;
    for(uint32_t i=0;i<m->count;i++) {
        const unsigned char *p=m->headers+i*56;uint64_t va=u64(p+16),length=u64(p+32);
        if(u32(p)==1&&address>=va&&address-va<=length&&bytes<=length-(address-va)) {
            *offset=u64(p+8)+(address-va);matches++;
        }
    }
    return matches==1;
}
static int virtual_read(int fd,const ElfMap *m,void *out,size_t bytes,uint64_t address) {
    uint64_t offset=0;return mapped(m,address,bytes,&offset)&&read_at(fd,out,bytes,offset,m->size);
}
static const char *elf_versions(int fd,const ElfMap *map,const char *strings,size_t string_size,
    uint64_t address,uint64_t count,const LibraryRuntimePolicy *policy,LibraryRuntimeImage *image) {
    if(!address&&!count)return NULL;
    if(!address||!count||count>64)return "library_loader_limit";
    uint64_t at=address;
    for(uint64_t i=0;i<count;i++) {
        unsigned char need[16];if(!virtual_read(fd,map,need,16,at))return "invalid_library_loader";
        unsigned aux_count=(unsigned)need[2]|(unsigned)need[3]<<8;
        uint32_t file=u32(need+4),aux=u32(need+8),next=u32(need+12);
        if(need[0]!=1||need[1]||!aux_count||aux_count>128||file>=string_size||
            !cstring((const unsigned char *)strings,string_size,file,0)||aux<16||at>UINT64_MAX-aux)
            return "invalid_library_loader";
        uint64_t va=at+aux;
        for(unsigned k=0;k<aux_count;k++) {
            unsigned char item[16];if(!virtual_read(fd,map,item,16,va))return "invalid_library_loader";
            uint32_t name=u32(item+8),step=u32(item+12);
            const char *v=cstring((const unsigned char *)strings,string_size,name,0);
            if(!v)return "invalid_library_loader";
            if(!strncmp(v,"GLIBC_",6)) {
                uint32_t minimum=0;
                if(!strcmp(v,"GLIBC_ABI_DT_RELR"))minimum=2u<<16|36u<<8;
                else if(!version(v+6,&minimum))return "unqualified_library_dependencies";
                if(minimum>image->minimum_glibc)image->minimum_glibc=minimum;
            }
            if(k+1==aux_count) {if(step)return "invalid_library_loader";}
            else {if(step<16||va>UINT64_MAX-step)return "invalid_library_loader";va+=step;}
        }
        if(i+1==count) {if(next)return "invalid_library_loader";}
        else {if(next<16||at>UINT64_MAX-next)return "invalid_library_loader";at+=next;}
    }
    return image->minimum_glibc>policy->glibc_version?"incompatible_library_os":NULL;
}
static const char *elf(int fd,const unsigned char *h,uint64_t size,const LibraryRuntimePolicy *policy,LibraryRuntimeImage *image,int executable) {
    unsigned machine=(unsigned)h[18]|(unsigned)h[19]<<8;
    if(h[4]!=2||h[5]!=1||h[6]!=1||(h[16]!=3&&!(executable&&h[16]==2))||h[17]||machine!=policy->cpu||u32(h+20)!=1||(executable&&!u64(h+24))||
        u32(h+48)||h[52]!=64||h[53])return "incompatible_library_binary";
    uint32_t count=(unsigned)h[56]|(unsigned)h[57]<<8;
    if(h[54]!=56||h[55]||!count||count>128)return "library_loader_limit";
    unsigned char headers[128*56];if(!read_at(fd,headers,count*56,u64(h+32),size))return "invalid_library_loader";
    uint64_t dyn_offset=0,dyn_size=0;unsigned dyn_count=0,interpreter_count=0;
    for(uint32_t i=0;i<count;i++) {
        const unsigned char *p=headers+i*56;uint64_t offset=u64(p+8),length=u64(p+32);
        if(offset>size||length>size-offset||length>u64(p+40))return "invalid_library_loader";
        if(u32(p)==2){dyn_offset=offset;dyn_size=length;dyn_count++;}
        if(u32(p)==3){char path[128];
            if(!executable)return "unqualified_library_dependencies";
            if(interpreter_count++||length<2||length>sizeof(path)||!read_at(fd,path,(size_t)length,offset,size)||path[length-1]||strlen(path)!=length-1)return "invalid_library_loader";
            const char *primary=policy->cpu==62?"/lib64/ld-linux-x86-64.so.2":"/lib/ld-linux-aarch64.so.1";
            const char *alternate=policy->cpu==62?"/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2":"/lib/aarch64-linux-gnu/ld-linux-aarch64.so.1";
            if(strcmp(path,primary)&&strcmp(path,alternate))return "unqualified_library_dependencies";
        }
    }
    if(executable&&interpreter_count!=1)return "invalid_library_loader";
    if(dyn_count!=1||!dyn_size||dyn_size>16384||dyn_size%16)return "library_loader_limit";
    unsigned char dynamic[16384];if(!read_at(fd,dynamic,(size_t)dyn_size,dyn_offset,size))return "invalid_library_loader";
    uint64_t strings=0,string_size=0,version_address=0,version_count=0;unsigned terminated=0;
    unsigned ver_address_seen=0,ver_count_seen=0;
    for(size_t i=0;i<dyn_size;i+=16) {
        uint64_t tag=u64(dynamic+i),value=u64(dynamic+i+8);if(!tag){terminated=1;break;}
        if(tag==5){if(strings||!value)return "invalid_library_loader";strings=value;}
        if(tag==10){if(string_size||!value)return "invalid_library_loader";string_size=value;}
        if(tag==0x6ffffffe){if(ver_address_seen++)return "invalid_library_loader";version_address=value;}
        if(tag==0x6fffffff){if(ver_count_seen++)return "invalid_library_loader";version_count=value;}
        if(tag==22||tag==0x7ffffffd||tag==0x7fffffff||tag==0x6ffffefb||tag==0x6ffffefc||
            (tag==30&&(value&4)))return "unqualified_library_dependencies";
    }
    if(ver_address_seen!=ver_count_seen||(ver_address_seen&&(!version_address||!version_count)))return "invalid_library_loader";
    if(!terminated||!strings||!string_size||string_size>1048576)return "invalid_library_loader";
    ElfMap map={headers,count,size};uint64_t table_offset=0;
    if(!mapped(&map,strings,(size_t)string_size,&table_offset))return "invalid_library_loader";
    unsigned char *table=malloc((size_t)string_size);if(!table)return "resource_limit";
    const char *e=NULL;if(!read_at(fd,table,(size_t)string_size,table_offset,size))e="invalid_library_loader";
    unsigned soname=0;
    for(size_t i=0;!e&&i<dyn_size;i+=16) {
        uint64_t tag=u64(dynamic+i),value=u64(dynamic+i+8);if(!tag)break;
        if(tag==1||tag==14||tag==15||tag==29) {
            const char *name=value<=SIZE_MAX?cstring(table,(size_t)string_size,(size_t)value,0):NULL;
            if(!name)e="invalid_library_loader";
            else if(tag==1)e=dependency(image,name,0);
            else if(tag==14){if(executable||soname++||!bare_name(name)||engine_trusted_system_library(0,name))e="unqualified_library_dependencies";}
            else if(image->origin_search||strcmp(name,"$ORIGIN"))e="unqualified_library_dependencies";
            else image->origin_search=1;
        }
    }
    if(!e)e=elf_versions(fd,&map,(const char *)table,(size_t)string_size,version_address,version_count,policy,image);
    free(table);return e;
}
static const char *inspect(int fd,const LibraryRuntimePolicy *policy,LibraryRuntimeImage *image,int executable) {
    if(!policy||!image||(policy->mach_o!=0&&policy->mach_o!=1)||
        (policy->mach_o?(policy->cpu!=0x100000c&&policy->cpu!=0x1000007):(policy->cpu!=183&&policy->cpu!=62)))return "invalid_configuration";
    memset(image,0,sizeof(*image));image->mach_o=policy->mach_o;
    struct stat before,after;unsigned char h[64];
    if(fstat(fd,&before)||!S_ISREG(before.st_mode)||before.st_size<64||
        !read_at(fd,h,64,0,(uint64_t)before.st_size))return "incompatible_library_binary";
    const char *e=NULL;
    if(policy->mach_o&&u32(h)==0xfeedfacf)e=mach(fd,h,(uint64_t)before.st_size,policy,image,executable);
    else if(!policy->mach_o&&!memcmp(h,"\177ELF",4))e=elf(fd,h,(uint64_t)before.st_size,policy,image,executable);
    else e="incompatible_library_binary";
    if(!e&&(fstat(fd,&after)||before.st_dev!=after.st_dev||before.st_ino!=after.st_ino||before.st_size!=after.st_size))e="artifact_changed";
    return e;
}
const char *library_runtime_inspect(int fd,const LibraryRuntimePolicy *policy,LibraryRuntimeImage *image){return inspect(fd,policy,image,0);}
const char *library_runtime_inspect_executable(int fd,const LibraryRuntimePolicy *policy,LibraryRuntimeImage *image){return inspect(fd,policy,image,1);}
