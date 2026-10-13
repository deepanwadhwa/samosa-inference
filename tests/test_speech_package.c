#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#define _DARWIN_C_SOURCE
#include "speech_package.h"
#include "sha256.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr,"%s:%d %s\n",__FILE__,__LINE__,#x); exit(1); } } while(0)
static const char *names[] = {"runtime/api", "model/flow", "model/main", "model/encoder", "model/decoder",
    "model/text", "model/vocab", "model/scores", "voices/caro.wav", "voices/stuart.wav", "runtime/ort"};
static const char *roles[] = {"library", "pocket_lm_flow", "pocket_lm_main", "pocket_encoder", "pocket_decoder",
    "pocket_text_conditioner", "pocket_vocab", "pocket_token_scores", "reference_caro", "reference_stuart", "dependency"};
static void write_file(int fd, const char *name, const char *data) {
    int f = openat(fd,name,O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW,0600); CHECK(f>=0);
    size_t n=strlen(data); CHECK(write(f,data,n)==(ssize_t)n); CHECK(close(f)==0);
}
static void manifest(int fd, char pin[65], const char *platform, const char *abi, const char *extra,
    int count, int duplicate, const char *special_name, const char *special_role, int kokoro) {
    char *body=NULL; size_t length=0; FILE *f=open_memstream(&body,&length); CHECK(f!=NULL);
    fprintf(f,"{\"format\":\"samosa.sherpa-speech.v1\",\"abi_version\":\"%s\",\"platform\":\"%s\",\"kind\":\"%s\",\"threads\":%d%s%s,\"files\":[",
        abi,platform,kokoro?"kokoro":"pocket",kokoro?6:2,kokoro?",\"data_dir\":\"data\"":"",extra);
    char hash[65];hash_bytes("artifact",8,hash);
    for(int i=0;i<count;i++) {
        const char *name=i==0&&special_name?special_name:names[duplicate&&i==1?0:i];
        const char *role=i==0&&special_role?special_role:roles[i];
        if(kokoro) { const char *kn[]={"runtime/api","model/flow","model/main","model/encoder","data/resource"};
            const char *kr[]={"library","kokoro_model","kokoro_voices","kokoro_tokens","kokoro_data"};name=kn[i];role=kr[i]; }
        fprintf(f,"%s{\"name\":\"%s\",\"role\":\"%s\",\"bytes\":8,\"sha256\":\"%s\"}",i?",":"",name,role,hash);
    }
    fputs("]}",f);CHECK(fclose(f)==0);hash_bytes(body,length,pin);write_file(fd,"manifest.json",body);free(body);
}
static void bulk_manifest(int fd,char pin[65],unsigned extras,int over_bytes) {
    char *body=NULL;size_t length=0;FILE *f=open_memstream(&body,&length);CHECK(f!=NULL);
    fprintf(f,"{\"format\":\"samosa.sherpa-speech.v1\",\"abi_version\":\"1.13.4\",\"platform\":\"%s\",\"kind\":\"pocket\",\"threads\":2,\"files\":[",speech_package_platform());
    char hash[65];hash_bytes("artifact",8,hash);
    for(unsigned i=0;i<11+extras;i++) {
        char name[64];snprintf(name,sizeof(name),"model/extra%03u",i>=11?i-11:0);
        fprintf(f,"%s{\"name\":\"%s\",\"role\":\"%s\",\"bytes\":%llu,\"sha256\":\"%s\"}",i?",":"",i<11?names[i]:name,i<11?roles[i]:"metadata",over_bytes&&i==0?8589934592ULL:8ULL,hash);
    }
    fputs("]}",f);CHECK(fclose(f)==0);hash_bytes(body,length,pin);write_file(fd,"manifest.json",body);free(body);
}
static void fresh(int fd,char pin[65]) { manifest(fd,pin,speech_package_platform(),"1.13.4","",11,0,NULL,NULL,0); }
static void fail(int fd,const char *path,const char *pin,SpeechPackageReceipt *r,const char *expected) {
    const char *e=speech_package_verify(fd,path,pin,NULL,NULL,r,0);CHECK(e!=NULL);
    if(expected) { if(strcmp(e,expected)) fprintf(stderr,"expected %s, got %s\n",expected,e); CHECK(!strcmp(e,expected)); }CHECK(!r->verified);
}
typedef struct {int calls,stop;const char *error;int fd;const char *mutate;} Control;
static const char *control(void *arg) {
    Control *c=arg;c->calls++;
    if(c->calls==c->stop) {if(c->mutate)write_file(c->fd,c->mutate,"changed!");return c->error;}
    return NULL;
}
int main(void) {
#ifdef __APPLE__
    char root[]="/private/tmp/samosa-speech-package.XXXXXX";
#else
    char root[]="/tmp/samosa-speech-package.XXXXXX";
#endif
    CHECK(mkdtemp(root)!=NULL);int fd=open(root,O_RDONLY|O_DIRECTORY|O_NOFOLLOW);CHECK(fd>=0);
    CHECK(mkdirat(fd,"runtime",0700)==0);CHECK(mkdirat(fd,"model",0700)==0);CHECK(mkdirat(fd,"voices",0700)==0);
    for(size_t i=0;i<11;i++)write_file(fd,names[i],"artifact");
    SpeechPackageReceipt *r=calloc(1,sizeof(*r));CHECK(r!=NULL);char pin[65];fresh(fd,pin);
    CHECK(!speech_package_verify(fd,root,pin,NULL,NULL,r,0));CHECK(r->verified&&r->pocket&&r->threads==2&&r->count==11&&r->directory_count==3&&r->total_bytes==88);
    char identity[65];speech_package_identity(pin,identity);CHECK(!strcmp(identity,r->model_identity));
    SpeechRuntimeOptions o;const char *caro,*stuart;
    CHECK(!speech_package_options(r,&o,&caro,&stuart));CHECK(o.pocket&&o.threads==2&&strstr(o.library,"/runtime/api")&&strstr(caro,"/voices/caro.wav")&&strstr(stuart,"/voices/stuart.wav"));
    for(int i=0;i<3;i++)CHECK(!speech_package_verify(fd,root,pin,NULL,NULL,r,1));
    /* Replacing equal bytes invalidates retained identity; an explicit fresh
     * verification can accept the same content under the same manifest pin. */
    CHECK(unlinkat(fd,names[1],0)==0);write_file(fd,names[1],"artifact");
    CHECK(!strcmp(speech_package_verify(fd,root,pin,NULL,NULL,r,1),"artifact_changed"));CHECK(r->verified);
    CHECK(!speech_package_verify(fd,root,pin,NULL,NULL,r,0));
    write_file(fd,names[1],"changed!");fail(fd,root,pin,r,"artifact_checksum_mismatch");write_file(fd,names[1],"artifact");
    CHECK(fchmodat(fd,names[1],0666,0)==0);fail(fd,root,pin,r,"unsupported_artifact");CHECK(fchmodat(fd,names[1],0600,0)==0);
    CHECK(linkat(fd,names[1],fd,"model/alias",0)==0);fail(fd,root,pin,r,NULL);CHECK(unlinkat(fd,"model/alias",0)==0);
    CHECK(unlinkat(fd,names[1],0)==0);CHECK(symlinkat("main",fd,names[1])==0);fail(fd,root,pin,r,NULL);CHECK(unlinkat(fd,names[1],0)==0);write_file(fd,names[1],"artifact");
    write_file(fd,"runtime/unlisted","artifact");fail(fd,root,pin,r,"unlisted_speech_artifact");CHECK(unlinkat(fd,"runtime/unlisted",0)==0);
    CHECK(mkdirat(fd,"empty",0700)==0);fail(fd,root,pin,r,"unlisted_speech_artifact");CHECK(unlinkat(fd,"empty",AT_REMOVEDIR)==0);
    CHECK(fchmodat(fd,"runtime",0777,0)==0);fail(fd,root,pin,r,"unsupported_artifact");CHECK(fchmodat(fd,"runtime",0700,0)==0);
    CHECK(unlinkat(fd,names[2],0)==0);fail(fd,root,pin,r,"artifact_unavailable");write_file(fd,names[2],"artifact");
    char wrong[65];memset(wrong,'0',64);wrong[64]=0;fail(fd,root,wrong,r,"artifact_checksum_mismatch");
    manifest(fd,pin,"wrong-platform","1.13.4","",11,0,NULL,NULL,0);fail(fd,root,pin,r,"incompatible_speech_package");
    manifest(fd,pin,speech_package_platform(),"1.13.3","",11,0,NULL,NULL,0);fail(fd,root,pin,r,"invalid_speech_package");
    manifest(fd,pin,speech_package_platform(),"1.13.4",",\"command\":\"hostile\"",11,0,NULL,NULL,0);fail(fd,root,pin,r,"invalid_speech_package");
    manifest(fd,pin,speech_package_platform(),"1.13.4",",\"threads\":2",11,0,NULL,NULL,0);fail(fd,root,pin,r,"invalid_speech_package");
    manifest(fd,pin,speech_package_platform(),"1.13.4","",11,1,NULL,NULL,0);fail(fd,root,pin,r,"invalid_speech_package");
    const char *invalid[]={"../api","/runtime/api","runtime//api","runtime/./api","runtime/api/","manifest.json"};
    for(size_t i=0;i<sizeof(invalid)/sizeof(*invalid);i++) {manifest(fd,pin,speech_package_platform(),"1.13.4","",11,0,invalid[i],NULL,0);fail(fd,root,pin,r,"invalid_speech_package");}
    const char *badroles[]={"kokoro_model","unknown","reserved"};
    for(size_t i=0;i<3;i++){manifest(fd,pin,speech_package_platform(),"1.13.4","",11,0,NULL,badroles[i],0);fail(fd,root,pin,r,"invalid_speech_package");}
    manifest(fd,pin,speech_package_platform(),"1.13.4","",9,0,NULL,NULL,0);fail(fd,root,pin,r,"invalid_speech_package");
    for(unsigned i=0;i<501;i++) {char name[64];snprintf(name,sizeof(name),"model/extra%03u",i);write_file(fd,name,"artifact");}
    bulk_manifest(fd,pin,501,0);CHECK(!speech_package_verify(fd,root,pin,NULL,NULL,r,0));CHECK(r->count==512&&r->total_bytes==4096);
    bulk_manifest(fd,pin,502,0);fail(fd,root,pin,r,"invalid_speech_package");
    bulk_manifest(fd,pin,0,1);fail(fd,root,pin,r,"invalid_speech_package");
    for(unsigned i=0;i<501;i++) {char name[64];snprintf(name,sizeof(name),"model/extra%03u",i);CHECK(unlinkat(fd,name,0)==0);}
    fresh(fd,pin);
    CHECK(!speech_package_verify(fd,root,pin,NULL,NULL,r,0));
    write_file(fd,"manifest.json","{}");
    CHECK(!strcmp(speech_package_verify(fd,root,pin,NULL,NULL,r,1),"artifact_changed"));
    fresh(fd,pin);Control c={.stop=1,.error="operation_cancelled"};CHECK(!strcmp(speech_package_verify(fd,root,pin,control,&c,r,0),c.error));CHECK(!r->verified);
    CHECK(!strcmp(speech_package_options(r,&o,&caro,&stuart),"invalid_configuration"));
    c=(Control){.stop=8,.error="operation_deadline"};CHECK(!strcmp(speech_package_verify(fd,root,pin,control,&c,r,0),c.error));
    c=(Control){.stop=6,.fd=fd,.mutate="model/main"};CHECK(speech_package_verify(fd,root,pin,control,&c,r,0)!=NULL);write_file(fd,"model/main","artifact");
    fresh(fd,pin);CHECK(!speech_package_verify(fd,root,pin,NULL,NULL,r,0));
    CHECK(renameat(fd,"runtime",fd,"runtime-old")==0);CHECK(symlinkat("runtime-old",fd,"runtime")==0);fail(fd,root,pin,r,NULL);
    CHECK(unlinkat(fd,"runtime",0)==0);CHECK(renameat(fd,"runtime-old",fd,"runtime")==0);
    /* Kokoro requires a pinned data directory; unrelated Pocket files cannot
     * remain hidden in a supposedly closed Kokoro package. */
    for(size_t i=4;i<11;i++)CHECK(unlinkat(fd,names[i],0)==0);
    CHECK(unlinkat(fd,"voices",AT_REMOVEDIR)==0);CHECK(mkdirat(fd,"data",0700)==0);write_file(fd,"data/resource","artifact");
    manifest(fd,pin,speech_package_platform(),"1.13.4","",5,0,NULL,NULL,1);
    CHECK(!speech_package_verify(fd,root,pin,NULL,NULL,r,0));CHECK(!r->pocket&&r->threads==6&&r->count==5);
    CHECK(!speech_package_options(r,&o,&caro,&stuart));CHECK(!o.pocket&&!caro&&!stuart&&strstr(o.data_dir,"/data"));
    CHECK(!speech_package_verify(fd,root,pin,NULL,NULL,r,1));
    for(size_t i=0;i<4;i++)CHECK(unlinkat(fd,names[i],0)==0);
    CHECK(unlinkat(fd,"data/resource",0)==0);CHECK(unlinkat(fd,"manifest.json",0)==0);
    CHECK(unlinkat(fd,"runtime",AT_REMOVEDIR)==0);CHECK(unlinkat(fd,"model",AT_REMOVEDIR)==0);CHECK(unlinkat(fd,"data",AT_REMOVEDIR)==0);
    CHECK(close(fd)==0);CHECK(rmdir(root)==0);free(r);
    printf("PASS speech package: %u checks; pinned closed trees, native roles, platform, controls and retained identities\n",checks);
    return 0;
}
