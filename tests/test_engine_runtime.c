#define _POSIX_C_SOURCE 200809L
#include "engine_runtime.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL runtime %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void w32(unsigned char *p,uint32_t x){for(unsigned i=0;i<4;i++)p[i]=(unsigned char)(x>>(8*i));}
static void w64(unsigned char *p,uint64_t x){for(unsigned i=0;i<8;i++)p[i]=(unsigned char)(x>>(8*i));}
static void expect(int fd,unsigned char *bytes,size_t length,const char *error){CHECK(!ftruncate(fd,0));CHECK(pwrite(fd,bytes,length,0)==(ssize_t)length);
    const char *e=engine_system_runtime(fd);CHECK(error?(e&&!strcmp(e,error)):!e);}
static void mach(unsigned char *b,const char *library){memset(b,0,4096);w32(b,0xfeedfacf);w32(b+4,0x100000c);w32(b+12,2);w32(b+16,1);
    size_t n=(24+strlen(library)+1+7)&~(size_t)7;w32(b+20,(uint32_t)n);w32(b+32,0xc);w32(b+36,(uint32_t)n);w32(b+40,24);strcpy((char*)b+56,library);}
static void elf(unsigned char *b,const char *library){memset(b,0,4096);memcpy(b,"\177ELF",4);b[4]=2;b[5]=1;b[6]=1;b[16]=3;b[18]=183;
    w64(b+24,0x1000);w64(b+32,64);b[54]=56;b[56]=3;
    w32(b+64,1);w64(b+64+16,0x1000);w64(b+64+32,4096);
    w32(b+120,2);w64(b+120+8,512);w64(b+120+32,80);
    w32(b+176,3);w64(b+176+8,300);w64(b+176+32,29);strcpy((char*)b+300,"/lib/ld-linux-aarch64.so.1");
    w64(b+512,5);w64(b+520,0x1000+1024);w64(b+528,10);w64(b+536,128);w64(b+544,1);w64(b+552,1);strcpy((char*)b+1025,library);}
int main(int argc,char **argv){
    if(argc==2){int fd=open(argv[1],O_RDONLY);CHECK(fd>=0);const char *e=engine_system_runtime(fd);CHECK(!close(fd));
        if(e){fprintf(stderr,"%s\n",e);return 1;}puts("PASS direct engine system-runtime inspection");return 0;}
    char path[]="/tmp/samosa-engine-runtime.XXXXXX";int fd=mkstemp(path);CHECK(fd>=0);CHECK(!unlink(path));unsigned char b[4096];
    mach(b,"/usr/lib/libc++.1.dylib");expect(fd,b,sizeof(b),NULL);
    mach(b,"/System/Library/Frameworks/Metal.framework/Versions/A/Metal");expect(fd,b,sizeof(b),NULL);
    mach(b,"@rpath/libllama.dylib");expect(fd,b,sizeof(b),"unqualified_engine_dependencies");
    mach(b,"/opt/homebrew/lib/libcrypto.dylib");expect(fd,b,sizeof(b),"unqualified_engine_dependencies");
    mach(b,"/usr/lib/../../tmp/evil.dylib");expect(fd,b,sizeof(b),"unqualified_engine_dependencies");
    mach(b,"/usr/lib/libc++.1.dylib");w32(b+32,0x8000001c);expect(fd,b,sizeof(b),"unqualified_engine_dependencies");
    w32(b+32,0x27);expect(fd,b,sizeof(b),"unqualified_engine_dependencies");
    mach(b,"/usr/lib/libc++.1.dylib");w32(b+40,8);expect(fd,b,sizeof(b),"invalid_engine_loader");
    mach(b,"/usr/lib/libc++.1.dylib");w32(b+36,9);expect(fd,b,sizeof(b),"invalid_engine_loader");
    mach(b,"/usr/lib/libc++.1.dylib");w32(b+20,65537);expect(fd,b,sizeof(b),"engine_loader_limit");
    mach(b,"/usr/lib/libc++.1.dylib");expect(fd,b,64,"invalid_engine_loader");
    elf(b,"libc.so.6");expect(fd,b,sizeof(b),NULL);
    elf(b,"libstdc++.so.6");expect(fd,b,sizeof(b),NULL);
    elf(b,"libllama.so");expect(fd,b,sizeof(b),"unqualified_engine_dependencies");
    elf(b,"/tmp/libc.so.6");expect(fd,b,sizeof(b),"unqualified_engine_dependencies");
    elf(b,"libc.so.6");w64(b+544,29);expect(fd,b,sizeof(b),"unqualified_engine_dependencies");
    elf(b,"libc.so.6");w64(b+544,15);expect(fd,b,sizeof(b),"unqualified_engine_dependencies");
    elf(b,"libc.so.6");w64(b+552,128);expect(fd,b,sizeof(b),"invalid_engine_loader");
    elf(b,"libc.so.6");memset(b+1025,'x',127);expect(fd,b,sizeof(b),"invalid_engine_loader");
    elf(b,"libc.so.6");w64(b+520,0xffffffffffffffffULL);expect(fd,b,sizeof(b),"invalid_engine_loader");
    elf(b,"libc.so.6");w64(b+120+32,16385);expect(fd,b,sizeof(b),"invalid_engine_loader");
    elf(b,"libc.so.6");b[56]=129;expect(fd,b,sizeof(b),"engine_loader_limit");
    elf(b,"libc.so.6");b[5]=2;expect(fd,b,sizeof(b),"unsupported_engine_binary");
    memset(b,0,sizeof(b));expect(fd,b,sizeof(b),"unsupported_engine_binary");
    CHECK(!close(fd));CHECK(!setenv("LD_PRELOAD","",1));CHECK(engine_runtime_environment()!=NULL);CHECK(!unsetenv("LD_PRELOAD"));
    CHECK(!setenv("DYLD_LIBRARY_PATH","/tmp",1));CHECK(engine_runtime_environment()!=NULL);CHECK(!unsetenv("DYLD_LIBRARY_PATH"));
    CHECK(!setenv("GGML_BACKEND_PATH","/tmp",1));CHECK(engine_runtime_environment()!=NULL);CHECK(!unsetenv("GGML_BACKEND_PATH"));
    CHECK(!setenv("LLAMA_ARG_MMPROJ","/tmp/unpinned",1));CHECK(engine_runtime_environment()!=NULL);CHECK(!unsetenv("LLAMA_ARG_MMPROJ"));
    printf("PASS engine runtime: %u checks; bounded ARM64 Mach-O/ELF loader policy and environment refusal\n",checks);return 0;
}
