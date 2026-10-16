#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "decision_assets.h"
#include "library_runtime.h"
#include "library_fixture.h"
#include "sha256.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void put(int root,const char *name,const void *bytes,size_t length,int mode,char pin[65]) {
    int fd=openat(root,name,O_WRONLY|O_CREAT|O_TRUNC,mode);CHECK(fd>=0);
    CHECK(write(fd,bytes,length)==(ssize_t)length);CHECK(!fchmod(fd,mode));CHECK(!close(fd));hash_bytes(bytes,length,pin);
}
static const char *cancel(void *unused){(void)unused;return "cancelled";}
static uint32_t r32(const unsigned char *p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
int main(void) {
    char path[]="/tmp/samosa-decision-assets.XXXXXX";CHECK(mkdtemp(path));
    int root=open(path,O_RDONLY|O_DIRECTORY);CHECK(root>=0);
    LibraryRuntimePolicy policy;CHECK(!library_runtime_host_policy(&policy));
    unsigned char engine[4096],library[4096];
    if(policy.mach_o){
        lib_executable_mach(engine,policy.cpu);char dependency[128];
        snprintf(dependency,sizeof(dependency),"@rpath/%s",decision_asset_name(DECISION_RUNTIME));
        size_t at=32+r32(engine+20);at=lib_command(engine,at,0xc,dependency,24);
        at=lib_command(engine,at,0x8000001c,"@loader_path",12);
        lib_w32(engine+16,7);lib_w32(engine+20,(uint32_t)(at-32));
        lib_mach(library,policy.cpu,NULL,0);
    }else{
        lib_executable_elf(engine,policy.cpu);lib_w64(engine+576,1);lib_w64(engine+584,192);lib_w64(engine+592,0);
        strcpy((char*)engine+1216,decision_asset_name(DECISION_RUNTIME));lib_elf(library,policy.cpu,NULL,0);
    }
    char pins[4][65];const char *values[]={pins[0],pins[1],pins[2],pins[3]};
    put(root,decision_asset_name(0),engine,sizeof(engine),0700,pins[0]);
    put(root,decision_asset_name(1),library,sizeof(library),0600,pins[1]);
    put(root,decision_asset_name(2),"model",5,0600,pins[2]);
    put(root,decision_asset_name(3),"tokenizer",9,0600,pins[3]);
    DecisionAssets assets={0};
    const char *error=decision_assets_open(path,values,NULL,NULL,&assets);
    if(error)fprintf(stderr,"asset open: %s\n",error);
    CHECK(!error&&assets.active);CHECK(!decision_assets_recheck(&assets,NULL,NULL));
    CHECK(!decision_assets_recheck(&assets,NULL,NULL)); /* Repeated enumeration must not reuse EOF. */
    CHECK(!strcmp(decision_assets_recheck(&assets,cancel,NULL),"cancelled"));
    DecisionAssets empty={0};CHECK(!strcmp(decision_assets_open(path,values,cancel,NULL,&empty),"cancelled"));CHECK(!empty.active);
    char unused[65];put(root,"unexpected","x",1,0600,unused);
    CHECK(decision_assets_recheck(&assets,NULL,NULL)!=NULL);CHECK(!unlinkat(root,"unexpected",0));
    decision_assets_close(&assets);decision_assets_close(&assets);
    CHECK(!decision_assets_open_profile(path,values,DECISION_ASSETS_IMAGE,NULL,NULL,&assets));
    CHECK(assets.profile==DECISION_ASSETS_IMAGE&&!decision_assets_recheck(&assets,NULL,NULL));
    decision_assets_close(&assets);
    CHECK(!strcmp(decision_assets_open_profile(path,values,2,NULL,NULL,&empty),"invalid_decision_configuration")&&!empty.active);
    int large=openat(root,decision_asset_name(DECISION_MODEL),O_WRONLY);CHECK(large>=0);
    CHECK(!ftruncate(large,1073741825LL));
    CHECK(!strcmp(decision_assets_open(path,values,NULL,NULL,&empty),"invalid_artifact_configuration")&&!empty.active);
    CHECK(!ftruncate(large,2147483649LL));
    CHECK(!strcmp(decision_assets_open_profile(path,values,DECISION_ASSETS_IMAGE,NULL,NULL,&empty),"invalid_artifact_configuration")&&!empty.active);
    CHECK(!ftruncate(large,5));CHECK(!close(large));
    put(root,decision_asset_name(2),"wrong",5,0600,unused);
    CHECK(!strcmp(decision_assets_open(path,values,NULL,NULL,&empty),"artifact_checksum_mismatch")&&!empty.active);
    put(root,decision_asset_name(2),"model",5,0600,pins[2]);
    CHECK(!decision_assets_open(path,values,NULL,NULL,&assets));
    put(root,decision_asset_name(3),"changed",7,0600,unused);
    CHECK(decision_assets_recheck(&assets,NULL,NULL)!=NULL);decision_assets_close(&assets);
    put(root,decision_asset_name(3),"tokenizer",9,0600,pins[3]);
    unsigned char bad[4096];
    if(policy.mach_o)lib_executable_mach(bad,policy.cpu);else lib_executable_elf(bad,policy.cpu);
    put(root,decision_asset_name(0),bad,sizeof(bad),0700,pins[0]);
    CHECK(!strcmp(decision_assets_open(path,values,NULL,NULL,&empty),"unqualified_decision_assets")&&!empty.active);
    put(root,decision_asset_name(0),engine,sizeof(engine),0700,pins[0]);
    if(policy.mach_o)lib_mach(bad,policy.cpu,"@loader_path/unlisted.dylib",1);
    else lib_elf(bad,policy.cpu,"unlisted.so",1);
    put(root,decision_asset_name(1),bad,sizeof(bad),0600,pins[1]);
    CHECK(!strcmp(decision_assets_open(path,values,NULL,NULL,&empty),"unqualified_library_dependencies")&&!empty.active);
    put(root,decision_asset_name(1),library,sizeof(library),0600,pins[1]);
    CHECK(!decision_assets_open(path,values,NULL,NULL,&assets));
    char moved[128];snprintf(moved,sizeof(moved),"%s-moved",path);
    CHECK(!rename(path,moved));CHECK(!mkdir(path,0700));
    CHECK(!strcmp(decision_assets_recheck(&assets,NULL,NULL),"artifact_changed"));
    decision_assets_close(&assets);CHECK(!rmdir(path));CHECK(!rename(moved,path));
    char alias[128];snprintf(alias,sizeof(alias),"%s.alias",path);
    CHECK(!linkat(root,decision_asset_name(2),AT_FDCWD,alias,0));
    CHECK(!strcmp(decision_assets_open(path,values,NULL,NULL,&empty),"unsupported_artifact")&&!empty.active);
    CHECK(!unlink(alias));
    CHECK(!unlinkat(root,decision_asset_name(3),0));CHECK(!symlinkat(decision_asset_name(2),root,decision_asset_name(3)));
    CHECK(decision_assets_open(path,values,NULL,NULL,&empty)!=NULL&&!empty.active);
    CHECK(!unlinkat(root,decision_asset_name(3),0));put(root,decision_asset_name(3),"tokenizer",9,0600,pins[3]);
    CHECK(!fchmod(root,0755));CHECK(decision_assets_open(path,values,NULL,NULL,&empty)!=NULL&&!empty.active);CHECK(!fchmod(root,0700));
    for(unsigned i=0;i<4;i++)CHECK(!unlinkat(root,decision_asset_name(i),0));
    CHECK(!close(root));CHECK(!rmdir(path));
    printf("PASS decision assets: %u checks; pinned held files, closed loader graph, mutation and cancellation; synthetic binaries only\n",checks);
}
