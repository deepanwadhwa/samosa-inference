#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#define _DARWIN_C_SOURCE
#include "speech_package_loader.h"
#include "library_fixture.h"
#include "sha256.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL speech loader %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static const char *names[]={"runtime/api","model/flow","model/main","model/encoder","model/decoder","model/text","model/vocab","model/scores","voices/caro","voices/stuart","runtime/ort"};
static const char *roles[]={"library","pocket_lm_flow","pocket_lm_main","pocket_encoder","pocket_decoder","pocket_text_conditioner","pocket_vocab","pocket_token_scores","reference_caro","reference_stuart","dependency"};
static void write_bytes(int root,const char *name,const void *data,size_t size) {
    int fd=openat(root,name,O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW,0600);CHECK(fd>=0);CHECK(write(fd,data,size)==(ssize_t)size);CHECK(!close(fd));
}
static void manifest(int root,char pin[65],const char *ort_role) {
    char *text=NULL;size_t bytes=0;FILE *f=open_memstream(&text,&bytes);CHECK(f!=NULL);
    fprintf(f,"{\"format\":\"samosa.sherpa-speech.v1\",\"abi_version\":\"1.13.4\",\"platform\":\"%s\",\"kind\":\"pocket\",\"threads\":2,\"files\":[",speech_package_platform());
    for(unsigned i=0;i<11;i++) {
        int fd=openat(root,names[i],O_RDONLY|O_NOFOLLOW);CHECK(fd>=0);unsigned char data[4096];ssize_t n=read(fd,data,sizeof(data));CHECK(n>0);CHECK(!close(fd));
        char sha[65];hash_bytes(data,(size_t)n,sha);
        fprintf(f,"%s{\"name\":\"%s\",\"role\":\"%s\",\"bytes\":%zu,\"sha256\":\"%s\"}",i?",":"",names[i],i==10?ort_role:roles[i],(size_t)n,sha);
    }
    fputs("]}",f);CHECK(!fclose(f));hash_bytes(text,bytes,pin);write_bytes(root,"manifest.json",text,bytes);free(text);
}
static void image(int root,const char *name,const LibraryRuntimePolicy *policy,const char *needed,int origin) {
    unsigned char b[4096];if(policy->mach_o)lib_mach(b,policy->cpu,needed,origin);else lib_elf(b,policy->cpu,needed,origin);
    write_bytes(root,name,b,sizeof(b));
}
static void check_package(int root,const char *path,const LibraryRuntimePolicy *policy,SpeechPackageReceipt *receipt,const char *role,const char *expected) {
    char pin[65];manifest(root,pin,role);CHECK(!speech_package_verify(root,path,pin,NULL,NULL,receipt,0));
    const char *e=speech_package_loader_verify(root,path,pin,policy,NULL,NULL,receipt);
    if(expected&&(!e||strcmp(e,expected)))fprintf(stderr,"expected %s got %s\n",expected,e?e:"success");
    CHECK(expected?(e&&!strcmp(e,expected)):!e);
}
static const char *cancel(void *context) {unsigned *n=context;return ++*n==20?"operation_deadline":NULL;}
int main(void) {
#ifdef __APPLE__
    char path[]="/private/tmp/samosa-speech-loader.XXXXXX";
#else
    char path[]="/tmp/samosa-speech-loader.XXXXXX";
#endif
    CHECK(mkdtemp(path)!=NULL);int root=open(path,O_RDONLY|O_DIRECTORY|O_NOFOLLOW);CHECK(root>=0);
    CHECK(!mkdirat(root,"runtime",0700));CHECK(!mkdirat(root,"model",0700));CHECK(!mkdirat(root,"voices",0700));
    for(unsigned i=1;i<10;i++)write_bytes(root,names[i],"artifact",8);
    LibraryRuntimePolicy native;CHECK(!library_runtime_host_policy(&native));
    SpeechPackageReceipt *receipt=calloc(1,sizeof(*receipt));CHECK(receipt!=NULL);
    /* Both binary formats and architectures use explicit private test policy;
     * this is format/graph coverage, not a Linux actual-runtime qualification. */
    for(unsigned format=0;format<4;format++) {
    LibraryRuntimePolicy host=format<2?(LibraryRuntimePolicy){1,format?0x1000007:0x100000c,26u<<16,0}:
        (LibraryRuntimePolicy){0,format==2?183:62,0,2u<<16|40u<<8};
    const char *local=host.mach_o?"@rpath/ort":"ort";
    const char *os=host.mach_o?"/usr/lib/libSystem.B.dylib":"libc.so.6";
    image(root,names[0],&host,local,1);image(root,names[10],&host,os,0);
    check_package(root,path,&host,receipt,"dependency",NULL);
    check_package(root,path,&host,receipt,"metadata","unqualified_library_dependencies");
    image(root,names[0],&host,local,0);check_package(root,path,&host,receipt,"dependency","unqualified_library_dependencies");
    image(root,names[0],&host,host.mach_o?"@loader_path/ort":"ort",1);check_package(root,path,&host,receipt,"dependency",NULL);
    const char *bad_mach[]={"@rpath/missing","@rpath/../ort","@executable_path/ort","/tmp/ort","ort"};
    const char *bad_elf[]={"missing","../ort","$ORIGIN/ort","/tmp/ort","runtime/ort"};
    for(unsigned i=0;i<5;i++) {
        image(root,names[0],&host,host.mach_o?bad_mach[i]:bad_elf[i],1);
        check_package(root,path,&host,receipt,"dependency","unqualified_library_dependencies");
    }
    image(root,names[0],&host,local,1);image(root,names[10],&host,host.mach_o?"@rpath/missing":"missing",1);
    check_package(root,path,&host,receipt,"dependency","unqualified_library_dependencies");
    image(root,names[10],&host,os,0);
    LibraryRuntimePolicy wrong=host;wrong.cpu=host.mach_o?(host.cpu==0x100000c?0x1000007:0x100000c):(host.cpu==62?183:62);
    image(root,names[10],&wrong,os,0);check_package(root,path,&host,receipt,"dependency","incompatible_library_binary");
    image(root,names[10],&host,os,0);
    if(host.mach_o) {
        unsigned char b[4096];lib_mach(b,host.cpu,local,1);
        size_t at=32+b[36]+24;lib_w32(b+at+20,2u<<16);write_bytes(root,names[0],b,sizeof(b));
        check_package(root,path,&host,receipt,"dependency","incompatible_library_version");
        image(root,names[0],&host,local,1);wrong=host;wrong.os_version=10u<<16;
        check_package(root,path,&wrong,receipt,"dependency","incompatible_library_os");
    } else {
        unsigned char b[4096];lib_elf(b,host.cpu,local,1);lib_elf_version(b,"GLIBC_2.99");write_bytes(root,names[0],b,sizeof(b));
        check_package(root,path,&host,receipt,"dependency","incompatible_library_os");image(root,names[0],&host,local,1);
        CHECK(!renameat(root,"runtime/ort",root,"runtime/libc.so.6"));names[10]="runtime/libc.so.6";
        image(root,names[0],&host,os,1);
        check_package(root,path,&host,receipt,"dependency","unqualified_library_dependencies");
        CHECK(!renameat(root,"runtime/libc.so.6",root,"runtime/ort"));names[10]="runtime/ort";
    }
    /* A declared binary which no current edge uses still receives inspection. */
    image(root,names[0],&host,os,0);write_bytes(root,names[10],"not-native",10);
    check_package(root,path,&host,receipt,"dependency","incompatible_library_binary");
    image(root,names[0],&host,local,1);image(root,names[10],&host,os,0);char pin[65];manifest(root,pin,"dependency");
    CHECK(!speech_package_verify(root,path,pin,NULL,NULL,receipt,0));unsigned calls=0;
    CHECK(!strcmp(speech_package_loader_verify(root,path,pin,&host,cancel,&calls,receipt),"operation_deadline"));
    CHECK(!setenv("LD_LIBRARY_PATH","/tmp",1));CHECK(!strcmp(speech_package_loader_verify(root,path,pin,&host,NULL,NULL,receipt),"unsafe_engine_environment"));CHECK(!unsetenv("LD_LIBRARY_PATH"));
    }
    for(unsigned i=0;i<11;i++)CHECK(!unlinkat(root,names[i],0));CHECK(!unlinkat(root,"manifest.json",0));
    CHECK(!unlinkat(root,"runtime",AT_REMOVEDIR));CHECK(!unlinkat(root,"model",AT_REMOVEDIR));CHECK(!unlinkat(root,"voices",AT_REMOVEDIR));CHECK(!close(root));CHECK(!rmdir(path));free(receipt);
    printf("PASS speech loader: %u checks; pinned transitive graph, versions, controls and foreign-path refusal\n",checks);return 0;
}
