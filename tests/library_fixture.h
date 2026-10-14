#ifndef SAMOSA_TEST_LIBRARY_FIXTURE_H
#define SAMOSA_TEST_LIBRARY_FIXTURE_H
#include <stdint.h>
#include <string.h>
static inline void lib_w32(unsigned char *p,uint32_t v) {for(unsigned i=0;i<4;i++)p[i]=(unsigned char)(v>>(i*8));}
static inline void lib_w64(unsigned char *p,uint64_t v) {for(unsigned i=0;i<8;i++)p[i]=(unsigned char)(v>>(i*8));}
static inline size_t lib_command(unsigned char *b,size_t at,uint32_t cmd,const char *name,unsigned prefix) {
    size_t n=(prefix+strlen(name)+1+7)&~(size_t)7;
    lib_w32(b+at,cmd);lib_w32(b+at+4,(uint32_t)n);lib_w32(b+at+8,prefix);strcpy((char *)b+at+prefix,name);return at+n;
}
static inline void lib_mach(unsigned char b[4096],uint32_t cpu,const char *needed,int origin) {
    memset(b,0,4096);lib_w32(b,0xfeedfacf);lib_w32(b+4,cpu);lib_w32(b+8,cpu==0x100000c?0:3);
    lib_w32(b+12,6);lib_w32(b+24,0x80);
    size_t at=lib_command(b,32,0xd,"@rpath/fixture.dylib",24);lib_w32(b+48,0x10000);
    lib_w32(b+at,0x32);lib_w32(b+at+4,24);lib_w32(b+at+8,1);lib_w32(b+at+12,11u<<16);at+=24;
    unsigned count=2;
    if(needed){at=lib_command(b,at,0xc,needed,24);count++;}
    if(origin){at=lib_command(b,at,0x8000001c,"@loader_path",12);count++;}
    lib_w32(b+16,count);lib_w32(b+20,(uint32_t)(at-32));
}
static inline void lib_elf(unsigned char b[4096],uint32_t cpu,const char *needed,int origin) {
    memset(b,0,4096);memcpy(b,"\177ELF",4);b[4]=2;b[5]=1;b[6]=1;b[16]=3;b[18]=(unsigned char)cpu;
    lib_w32(b+20,1);lib_w64(b+32,64);b[52]=64;b[54]=56;b[56]=2;
    lib_w32(b+64,1);lib_w64(b+80,0x1000);lib_w64(b+96,4096);lib_w64(b+104,4096);
    lib_w32(b+120,2);lib_w64(b+128,512);lib_w64(b+152,128);lib_w64(b+160,128);
    lib_w64(b+512,5);lib_w64(b+520,0x1400);lib_w64(b+528,10);lib_w64(b+536,256);
    size_t at=544;
    if(needed){lib_w64(b+at,1);lib_w64(b+at+8,1);strcpy((char *)b+1025,needed);at+=16;}
    lib_w64(b+at,14);lib_w64(b+at+8,128);strcpy((char *)b+1152,"fixture.so");at+=16;
    if(origin){lib_w64(b+at,29);lib_w64(b+at+8,160);strcpy((char *)b+1184,"$ORIGIN");}
}
static inline void lib_elf_version(unsigned char b[4096],const char *name) {
    lib_w64(b+592,0x6ffffffe);lib_w64(b+600,0x1640);lib_w64(b+608,0x6fffffff);lib_w64(b+616,1);
    b[1600]=1;b[1602]=1;lib_w32(b+1604,1);lib_w32(b+1608,16);
    lib_w32(b+1624,192);strcpy((char *)b+1216,name);
}
static inline void lib_executable_mach(unsigned char *b,uint32_t cpu){
    memset(b,0,4096);lib_w32(b,0xfeedfacf);lib_w32(b+4,cpu);lib_w32(b+8,cpu==0x100000c?0:3);lib_w32(b+12,2);lib_w32(b+24,0x80);
    lib_w32(b+32,0x32);lib_w32(b+36,24);lib_w32(b+40,1);lib_w32(b+44,11u<<16);
    size_t at=lib_command(b,56,0xe,"/usr/lib/dyld",12);
    lib_w32(b+at,0x80000028);lib_w32(b+at+4,24);lib_w64(b+at+8,1024);at+=24;
    at=lib_command(b,at,0xc,"/usr/lib/libSystem.B.dylib",24);
    lib_w32(b+at,0x19);lib_w32(b+at+4,72);lib_w64(b+at+32,4096);lib_w64(b+at+48,4096);lib_w32(b+at+60,5);at+=72;
    lib_w32(b+16,5);lib_w32(b+20,(uint32_t)(at-32));
}
static inline void lib_executable_elf(unsigned char *b,uint32_t cpu){
    lib_elf(b,cpu,"libc.so.6",1);lib_w64(b+24,0x1800);lib_w32(b+68,5);b[56]=3;
    lib_w32(b+176,3);lib_w64(b+184,768);const char *path=cpu==62?"/lib64/ld-linux-x86-64.so.2":"/lib/ld-linux-aarch64.so.1";
    strcpy((char *)b+768,path);lib_w64(b+208,strlen(path)+1);lib_w64(b+216,strlen(path)+1);
    lib_w64(b+560,29);lib_w64(b+568,160);lib_w64(b+576,0);
}
#endif
