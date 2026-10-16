#define _POSIX_C_SOURCE 200809L
#include "library_runtime.h"
#include "engine_runtime.h"
#include "library_fixture.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL library runtime %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void expect(int fd,unsigned char *bytes,size_t n,const LibraryRuntimePolicy *p,const char *error) {
    CHECK(!ftruncate(fd,0));CHECK(pwrite(fd,bytes,n,0)==(ssize_t)n);
    LibraryRuntimeImage image;const char *e=library_runtime_inspect(fd,p,&image);
    if(error&&(!e||strcmp(e,error)))fprintf(stderr,"expected %s got %s\n",error,e?e:"success");
    CHECK(error?(e&&!strcmp(e,error)):!e);
}
static void expect_engine(int fd,unsigned char *bytes,const LibraryRuntimePolicy *p,const char *error){
    CHECK(!ftruncate(fd,0));CHECK(pwrite(fd,bytes,4096,0)==4096);LibraryRuntimeImage image;
    const char *e=library_runtime_inspect_executable(fd,p,&image);
    if(error&&(!e||strcmp(e,error)))fprintf(stderr,"expected engine %s got %s\n",error,e?e:"success");
    CHECK(error?(e&&!strcmp(e,error)):!e);
}
static const char *graph_cancel(void *unused){(void)unused;return "cancelled";}
static void test_graph(int fd){
    unsigned char b[4096];LibraryRuntimePolicy policy={1,0x100000c,14u<<16,0};
    LibraryRuntimeImage images[3];LibraryRuntimeFile files[3]={{"runtime/engine",LIBRARY_RUNTIME_EXECUTABLE},{"runtime/ort",LIBRARY_RUNTIME_LIBRARY},{"runtime/helper",LIBRARY_RUNTIME_LIBRARY}};
    lib_executable_mach(b,policy.cpu);size_t dependency_at=32;for(unsigned i=0;i<3;i++)dependency_at+=b[dependency_at+4];
    strcpy((char *)b+dependency_at+24,"@loader_path/ort");lib_w32(b+dependency_at+20,0x10000);
    CHECK(!ftruncate(fd,0));CHECK(pwrite(fd,b,4096,0)==4096);CHECK(!library_runtime_inspect_executable(fd,&policy,&images[0]));
    lib_mach(b,policy.cpu,"@rpath/helper",1);CHECK(pwrite(fd,b,4096,0)==4096);CHECK(!library_runtime_inspect(fd,&policy,&images[1]));
    lib_mach(b,policy.cpu,NULL,0);CHECK(pwrite(fd,b,4096,0)==4096);CHECK(!library_runtime_inspect(fd,&policy,&images[2]));
    CHECK(!library_runtime_graph(&policy,files,images,3,NULL,NULL));
    CHECK(!strcmp(library_runtime_graph(&policy,files,images,3,graph_cancel,NULL),"cancelled"));
    images[1].current_version=0;CHECK(!strcmp(library_runtime_graph(&policy,files,images,3,NULL,NULL),"incompatible_library_version"));images[1].current_version=0x10000;
    files[1].kind=LIBRARY_RUNTIME_OTHER;CHECK(!strcmp(library_runtime_graph(&policy,files,images,3,NULL,NULL),"unqualified_library_dependencies"));
    files[1].kind=LIBRARY_RUNTIME_EXECUTABLE;CHECK(!strcmp(library_runtime_graph(&policy,files,images,3,NULL,NULL),"unqualified_library_dependencies"));files[1].kind=LIBRARY_RUNTIME_LIBRARY;
    files[2].name="elsewhere/helper";CHECK(!strcmp(library_runtime_graph(&policy,files,images,3,NULL,NULL),"unqualified_library_dependencies"));files[2].name="runtime/helper";
    images[1].origin_search=0;CHECK(!strcmp(library_runtime_graph(&policy,files,images,3,NULL,NULL),"unqualified_library_dependencies"));images[1].origin_search=1;
    strcpy(images[0].dependencies[0].name,"@loader_path/../ort");CHECK(!strcmp(library_runtime_graph(&policy,files,images,3,NULL,NULL),"unqualified_library_dependencies"));
    memset(images[0].dependencies[0].name,'x',256);CHECK(!strcmp(library_runtime_graph(&policy,files,images,3,NULL,NULL),"invalid_library_loader"));
    policy=(LibraryRuntimePolicy){0,62,0,2u<<16|40u<<8};lib_executable_elf(b,62);CHECK(pwrite(fd,b,4096,0)==4096);CHECK(!library_runtime_inspect_executable(fd,&policy,&images[0]));
    files[1]=(LibraryRuntimeFile){"runtime/libc.so.6",LIBRARY_RUNTIME_OTHER};files[2].kind=LIBRARY_RUNTIME_OTHER;
    CHECK(!strcmp(library_runtime_graph(&policy,files,images,3,NULL,NULL),"unqualified_library_dependencies"));
    files[1].name="runtime/unrelated-data";CHECK(!library_runtime_graph(&policy,files,images,3,NULL,NULL));
}
int main(void) {
    LibraryRuntimePolicy host;CHECK(!library_runtime_host_policy(&host));CHECK(host.cpu!=0);
    char path[]="/tmp/samosa-library-runtime.XXXXXX";int fd=mkstemp(path);CHECK(fd>=0);CHECK(!unlink(path));
    test_graph(fd);
    unsigned char b[4096];LibraryRuntimePolicy mach={1,0x100000c,14u<<16,0};
    lib_executable_mach(b,mach.cpu);expect_engine(fd,b,&mach,NULL);expect(fd,b,4096,&mach,"incompatible_library_binary");
    lib_w32(b+4,0x1000007);expect_engine(fd,b,&mach,"incompatible_library_binary");
    lib_executable_mach(b,mach.cpu);lib_w32(b+44,15u<<16);expect_engine(fd,b,&mach,"incompatible_library_os");
    lib_executable_mach(b,mach.cpu);strcpy((char *)b+68,"/tmp/dyld");expect_engine(fd,b,&mach,"unqualified_library_dependencies");
    lib_executable_mach(b,mach.cpu);lib_w32(b+16,2);lib_w32(b+20,56);expect_engine(fd,b,&mach,"invalid_library_loader");
    lib_executable_mach(b,mach.cpu);size_t segment=32;for(unsigned i=0;i<4;i++)segment+=b[segment+4];lib_w32(b+segment+60,1);expect_engine(fd,b,&mach,"invalid_library_loader");
    lib_executable_mach(b,mach.cpu);lib_w64(b+segment+40,2048);lib_w64(b+segment+48,2048);expect_engine(fd,b,&mach,"invalid_library_loader");
    LibraryRuntimePolicy engine_elf={0,62,0,2u<<16|40u<<8};lib_executable_elf(b,62);expect_engine(fd,b,&engine_elf,NULL);
    b[16]=2;expect_engine(fd,b,&engine_elf,NULL);lib_w64(b+24,0);expect_engine(fd,b,&engine_elf,"incompatible_library_binary");
    lib_executable_elf(b,62);strcpy((char *)b+768,"/tmp/untrusted-loader");lib_w64(b+208,22);lib_w64(b+216,22);expect_engine(fd,b,&engine_elf,"unqualified_library_dependencies");
    lib_executable_elf(b,183);engine_elf.cpu=183;expect_engine(fd,b,&engine_elf,NULL);engine_elf.cpu=62;expect_engine(fd,b,&engine_elf,"incompatible_library_binary");
    lib_executable_elf(b,62);b[56]=2;expect_engine(fd,b,&engine_elf,"invalid_library_loader");
    lib_executable_elf(b,62);lib_w32(b+68,4);expect_engine(fd,b,&engine_elf,"invalid_library_loader");
    lib_executable_elf(b,62);lib_w64(b+24,0x3000);expect_engine(fd,b,&engine_elf,"invalid_library_loader");
    lib_mach(b,mach.cpu,"@rpath/libort.dylib",1);expect(fd,b,4096,&mach,NULL);
    LibraryRuntimeImage image;CHECK(!library_runtime_inspect(fd,&mach,&image));CHECK(image.count==1&&image.origin_search&&image.minimum_os==(11u<<16)&&image.current_version==0x10000);
    mach.cpu=0x1000007;lib_mach(b,mach.cpu,"/usr/lib/libSystem.B.dylib",0);expect(fd,b,4096,&mach,NULL);
    mach.cpu=0x100000c;expect(fd,b,4096,&mach,"incompatible_library_binary");
    lib_mach(b,mach.cpu,NULL,0);lib_w32(b+12,2);expect(fd,b,4096,&mach,"incompatible_library_binary");
    lib_mach(b,mach.cpu,NULL,0);lib_w32(b+8,2);expect(fd,b,4096,&mach,"incompatible_library_binary");
    lib_mach(b,mach.cpu,NULL,0);lib_w32(b+24,0x180);expect(fd,b,4096,&mach,"incompatible_library_binary");
    lib_mach(b,mach.cpu,NULL,0);mach.os_version=10u<<16;expect(fd,b,4096,&mach,"incompatible_library_os");mach.os_version=14u<<16;
    lib_mach(b,mach.cpu,NULL,0);expect(fd,b,64,&mach,"invalid_library_loader");
    lib_mach(b,mach.cpu,NULL,0);lib_w32(b+20,65537);expect(fd,b,4096,&mach,"library_loader_limit");
    lib_mach(b,mach.cpu,NULL,0);lib_w32(b+36,9);expect(fd,b,4096,&mach,"invalid_library_loader");
    lib_mach(b,mach.cpu,NULL,0);lib_w32(b+40,1);expect(fd,b,4096,&mach,"invalid_library_loader");
    lib_mach(b,mach.cpu,NULL,0);lib_w32(b+32,0x27);expect(fd,b,4096,&mach,"unqualified_library_dependencies");
    lib_mach(b,mach.cpu,NULL,0);lib_w32(b+32,0xe);expect(fd,b,4096,&mach,"unqualified_library_dependencies");
    lib_mach(b,mach.cpu,NULL,0);lib_w32(b+32,0x20);expect(fd,b,4096,&mach,"unqualified_library_dependencies");
    lib_mach(b,mach.cpu,NULL,0);lib_w32(b+32,0xc);expect(fd,b,4096,&mach,"invalid_library_loader");
    lib_mach(b,mach.cpu,"@rpath/libort.dylib",1);size_t last=32;for(unsigned i=1;i<4;i++)last+=(size_t)b[last+4];strcpy((char *)b+last+12,"/tmp");expect(fd,b,4096,&mach,"unqualified_library_dependencies");
    lib_mach(b,mach.cpu,NULL,0);lib_w32(b+32,0x80000099);expect(fd,b,4096,&mach,"unqualified_library_dependencies");
    lib_mach(b,mach.cpu,NULL,0);strcpy((char *)b+56,"/usr/lib/libSystem.B");expect(fd,b,4096,&mach,"unqualified_library_dependencies");
    LibraryRuntimePolicy elf={0,62,0,2u<<16|40u<<8};
    lib_elf(b,62,"libc.so.6",1);expect(fd,b,4096,&elf,NULL);
    CHECK(!library_runtime_inspect(fd,&elf,&image));CHECK(image.count==1&&image.origin_search);
    elf.cpu=183;lib_elf(b,183,"libc.so.6",1);expect(fd,b,4096,&elf,NULL);
    lib_w32(b+18,62);expect(fd,b,4096,&elf,"incompatible_library_binary");
    lib_elf(b,183,"libc.so.6",1);b[5]=2;expect(fd,b,4096,&elf,"incompatible_library_binary");
    lib_elf(b,183,"libc.so.6",1);b[16]=2;expect(fd,b,4096,&elf,"incompatible_library_binary");
    lib_elf(b,183,"libc.so.6",1);lib_w32(b+64,3);expect(fd,b,4096,&elf,"unqualified_library_dependencies");
    lib_elf(b,183,"libc.so.6",1);b[56]=129;expect(fd,b,4096,&elf,"library_loader_limit");
    lib_elf(b,183,"libc.so.6",1);lib_w64(b+520,UINT64_MAX);expect(fd,b,4096,&elf,"invalid_library_loader");
    lib_elf(b,183,"libc.so.6",1);lib_w64(b+536,1048577);expect(fd,b,4096,&elf,"invalid_library_loader");
    lib_elf(b,183,"libc.so.6",1);strcpy((char *)b+1184,"/tmp");expect(fd,b,4096,&elf,"unqualified_library_dependencies");
    lib_elf(b,183,"libc.so.6",1);strcpy((char *)b+1184,"$ORIGIN/..");expect(fd,b,4096,&elf,"unqualified_library_dependencies");
    const uint64_t bad[]={22,0x7ffffffd,0x7fffffff,0x6ffffefb,0x6ffffefc};
    for(size_t i=0;i<sizeof(bad)/sizeof(*bad);i++){lib_elf(b,183,"libc.so.6",1);lib_w64(b+544,bad[i]);expect(fd,b,4096,&elf,"unqualified_library_dependencies");}
    lib_elf(b,183,"libc.so.6",1);lib_elf_version(b,"GLIBC_2.17");expect(fd,b,4096,&elf,NULL);
    CHECK(!library_runtime_inspect(fd,&elf,&image));CHECK(image.minimum_glibc==(2u<<16|17u<<8));
    lib_elf(b,183,"libc.so.6",1);lib_elf_version(b,"GLIBC_2.99");expect(fd,b,4096,&elf,"incompatible_library_os");
    lib_elf(b,183,"libc.so.6",1);lib_elf_version(b,"GLIBC_PRIVATE");expect(fd,b,4096,&elf,"unqualified_library_dependencies");
    lib_elf(b,183,"libc.so.6",1);lib_elf_version(b,"GLIBC_ABI_DT_RELR");expect(fd,b,4096,&elf,NULL);
    lib_elf(b,183,"libc.so.6",1);lib_elf_version(b,"GLIBC_2.17");lib_w32(b+1608,UINT32_MAX);expect(fd,b,4096,&elf,"invalid_library_loader");
    lib_elf(b,183,"libc.so.6",1);lib_elf_version(b,"GLIBC_2.17");lib_w32(b+1628,16);expect(fd,b,4096,&elf,"invalid_library_loader");
    CHECK(engine_trusted_system_library(1,"/usr/lib/libc++.1.dylib"));CHECK(!engine_trusted_system_library(1,"/usr/lib/.."));CHECK(!engine_trusted_system_library(1,"/System/Library/../evil"));
    CHECK(engine_trusted_system_library(0,"libc.so.6"));CHECK(!engine_trusted_system_library(0,"/tmp/libc.so.6"));
    CHECK(engine_trusted_system_library(0,"ld-linux-aarch64.so.1"));CHECK(engine_trusted_system_library(0,"ld-linux-x86-64.so.2"));
    CHECK(!engine_trusted_system_library(0,"/tmp/ld-linux-aarch64.so.1"));CHECK(!engine_trusted_system_library(0,"ld-linux-custom.so.1"));
    /* Bounded mutation/truncation corpus covers parser tables on both formats.
     * Any typed refusal or structurally valid image is permitted; sanitizers
     * must catch memory/overflow errors in these untrusted byte paths. */
    for(unsigned i=0;i<512;i++) {
        lib_mach(b,mach.cpu,"@rpath/libort.dylib",1);b[i]^=(unsigned char)(1u<<(i%8));
        CHECK(!ftruncate(fd,0));CHECK(pwrite(fd,b,sizeof(b),0)==sizeof(b));
        const char *e=library_runtime_inspect(fd,&mach,&image);CHECK(!e||*e);
        lib_elf(b,elf.cpu,"libc.so.6",1);b[i]^=(unsigned char)(1u<<(i%8));
        CHECK(!ftruncate(fd,0));CHECK(pwrite(fd,b,sizeof(b),0)==sizeof(b));
        e=library_runtime_inspect(fd,&elf,&image);CHECK(!e||*e);
    }
    for(unsigned i=0;i<192;i++) {
        lib_mach(b,mach.cpu,NULL,0);CHECK(!ftruncate(fd,0));CHECK(pwrite(fd,b,i,0)==i);
        const char *e=library_runtime_inspect(fd,&mach,&image);CHECK(!e||*e);
        lib_elf(b,elf.cpu,"libc.so.6",1);CHECK(!ftruncate(fd,0));CHECK(pwrite(fd,b,i,0)==i);
        e=library_runtime_inspect(fd,&elf,&image);CHECK(e!=NULL);
    }
    CHECK(!close(fd));printf("PASS library runtime: %u checks; Mach-O/ELF architecture, min OS, bounded tables and loader refusal\n",checks);return 0;
}
