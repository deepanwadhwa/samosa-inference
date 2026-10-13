#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "activation.h"
#include "json.h"
#include "sha256.h"
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <sqlite3.h>
extern char **environ;
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void delay(void){struct timespec t={0,10000000};nanosleep(&t,NULL);}
static int held(const char *path){int fd=open(path,O_RDONLY);if(fd<0)return 0;int locked=flock(fd,LOCK_EX|LOCK_NB);close(fd);return locked!=0;}
static const char *qualify(void *p){const char *path=p;for(int i=0;i<200;i++){if(held(path))return NULL;delay();}return "readiness_failed";}
static const char *fail(void *p){(void)p;return "readiness_failed";}
static const char *mutate(void *p){const char *e=qualify(p);if(e)return e;int fd=open(p,O_WRONLY);CHECK(fd>=0);CHECK(pwrite(fd,"x",1,0)==1);CHECK(!close(fd));return NULL;}
static void fail_directory_sync(const char *phase,void *p){Activation *a=p;CHECK(!strcmp(phase,"committed"));CHECK(!close(a->root));a->root=-1;}
static uint64_t copy(const char *from,const char *to,char digest[65]) {
    int src=open(from,O_RDONLY),dst=open(to,O_WRONLY|O_CREAT|O_EXCL,0700);CHECK(src>=0&&dst>=0);
    Sha256 h;sha256_init(&h);uint64_t size=0;unsigned char bytes[65536];
    for(;;){ssize_t n=read(src,bytes,sizeof(bytes));CHECK(n>=0);if(!n)break;
        CHECK(write(dst,bytes,(size_t)n)==n);sha256_update(&h,bytes,(size_t)n);size+=(uint64_t)n;}
    unsigned char hash[32];sha256_final(&h,hash);sha256_hex(hash,digest);CHECK(!close(src));CHECK(!close(dst));return size;
}
static char *get(const char *state){char *out=NULL;size_t n;FILE *f=open_memstream(&out,&n);CHECK(f);CHECK(!activation_get(state,f));CHECK(!fclose(f));return out;}
static void restart_get(const char *self,const char *state,const char *expected){int output[2];CHECK(!pipe(output));
    posix_spawn_file_actions_t actions;CHECK(!posix_spawn_file_actions_init(&actions));CHECK(!posix_spawn_file_actions_adddup2(&actions,output[1],STDOUT_FILENO));
    CHECK(!posix_spawn_file_actions_addclose(&actions,output[0]));CHECK(!posix_spawn_file_actions_addclose(&actions,output[1]));
    char *args[]={(char*)self,"--get-state",(char*)state,NULL};pid_t pid;CHECK(!posix_spawn(&pid,self,&actions,NULL,args,environ));
    posix_spawn_file_actions_destroy(&actions);CHECK(!close(output[1]));char text[4096];size_t n=0;
    for(;;){ssize_t got=read(output[0],text+n,sizeof(text)-1-n);CHECK(got>=0);if(!got)break;n+=(size_t)got;CHECK(n<sizeof(text)-1);}
    text[n]=0;CHECK(!close(output[0]));int status;CHECK(waitpid(pid,&status,0)==pid);CHECK(WIFEXITED(status)&&!WEXITSTATUS(status));CHECK(!strcmp(text,expected));}
int main(int argc,char **argv){
    int guardian=engine_guardian_dispatch(argc,argv);if(guardian>=0)return guardian;
    if(argc==3&&!strcmp(argv[1],"--get-state"))return activation_get(argv[2],stdout)?1:0;
    if(argc==3&&!strcmp(argv[1],"--hold-model")){int fd=open(argv[2],O_RDONLY);if(fd<0||flock(fd,LOCK_EX|LOCK_NB))return 3;for(;;)pause();}
    CHECK(argv[0][0]=='/');char root[]="/tmp/samosa-activation-XXXXXX";CHECK(mkdtemp(root));
    char engine[512],model[512],state[512],sha[65];snprintf(engine,sizeof(engine),"%s/engine",root);snprintf(model,sizeof(model),"%s/model",root);snprintf(state,sizeof(state),"%s/state",root);
    uint64_t size=copy(argv[0],engine,sha);int fd=open(model,O_WRONLY|O_CREAT|O_EXCL,0600);CHECK(fd>=0);CHECK(write(fd,"abc",3)==3);CHECK(!close(fd));
    ActivationConfig c={.state=state,.self=argv[0],.engine=engine,.model=model,.engine_sha=sha,
        .model_sha="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",.engine_size=size,.model_size=3,
        .byte_ceiling=size,.argv_template="[\"@ENGINE@\",\"--hold-model\",\"@MODEL@\"]",.lifetime=30,.log_limit=65536,.qualify=qualify,.qualification=model};
    Activation a,b;CHECK(!activation_start(&c,&a));CHECK(a.revision==1);CHECK(held(model));
    char *saved=get(state);CHECK(strstr(saved,"\"revision\":1"));CHECK(strstr(saved,"\"live_readiness\":\"unobserved\""));
    const char *e=activation_start(&c,&b);CHECK(e&&!strcmp(e,"activation_busy"));CHECK(held(model));
    CHECK(!activation_stop(&a));CHECK(!held(model));
    restart_get(argv[0],state,saved);
    e=activation_start(&c,&a);CHECK(e&&!strcmp(e,"activation_revision_conflict"));CHECK(!held(model));
    c.expected_revision=1;c.qualify=fail;e=activation_start(&c,&a);CHECK(e&&!strcmp(e,"readiness_failed"));CHECK(!held(model));
    char *current=get(state);CHECK(!strcmp(saved,current));free(current);
    c.qualify=qualify;const char *good=c.model_sha;c.model_sha=sha;
    e=activation_start(&c,&a);CHECK(e&&!strcmp(e,"artifact_checksum_mismatch"));CHECK(!held(model));c.model_sha=good;
    current=get(state);CHECK(!strcmp(saved,current));free(current);
    c.qualify=mutate;e=activation_start(&c,&a);CHECK(e&&!strcmp(e,"activation_input_changed"));CHECK(!held(model));
    current=get(state);CHECK(!strcmp(saved,current));free(current);free(saved);
    fd=open(model,O_WRONLY);CHECK(fd>=0);CHECK(pwrite(fd,"a",1,0)==1);CHECK(!close(fd));
    c.qualify=qualify;CHECK(!activation_start(&c,&a));CHECK(a.revision==2);CHECK(!activation_stop(&a));
    current=get(state);CHECK(strstr(current,"\"revision\":2"));free(current);
    c.expected_revision=2;c.argv_template="[\"@ENGINE@\",\"--hold-model\"]";
    e=activation_start(&c,&a);CHECK(e&&!strcmp(e,"invalid_activation_configuration"));
    c.argv_template="[\"@ENGINE@\",\"--hold-model\",\"@MODEL@\"]";c.checkpoint=fail_directory_sync;c.checkpoint_context=&a;
    e=activation_start(&c,&a);CHECK(e&&!strcmp(e,"activation_commit_uncertain"));CHECK(a.process.guardian>0);CHECK(held(model));
    current=get(state);CHECK(strstr(current,"\"revision\":3"));free(current);CHECK(!activation_stop(&a));CHECK(!held(model));
    char bad[512],bad_db[640];snprintf(bad,sizeof(bad),"%s/bad-state",root);CHECK(!mkdir(bad,0700));
    snprintf(bad_db,sizeof(bad_db),"%s/activation.sqlite3",bad);sqlite3 *db=NULL;CHECK(sqlite3_open(bad_db,&db)==SQLITE_OK);
    CHECK(sqlite3_exec(db,"PRAGMA page_size=65536;CREATE TABLE other(x)",NULL,NULL,NULL)==SQLITE_OK);CHECK(sqlite3_close(db)==SQLITE_OK);CHECK(!chmod(bad_db,0600));
    FILE *out=tmpfile();CHECK(out);e=activation_get(bad,out);CHECK(e&&!strcmp(e,"activation_state_limit"));CHECK(!fclose(out));
    c.checkpoint=NULL;c.checkpoint_context=NULL;c.expected_revision=0;c.advance_current=1;
    CHECK(!activation_start(&c,&a));CHECK(a.revision==4);CHECK(held(model));
    e=activation_start(&c,&b);CHECK(e&&!strcmp(e,"activation_busy"));CHECK(!activation_stop(&a));CHECK(!held(model));
    CHECK(!activation_start(&c,&a));CHECK(a.revision==5);CHECK(!activation_stop(&a));
    c.model_sha=sha;e=activation_start(&c,&a);CHECK(e&&!strcmp(e,"artifact_checksum_mismatch"));CHECK(!held(model));c.model_sha=good;
    current=get(state);CHECK(strstr(current,"\"revision\":5"));free(current);
    c.advance_current=0;e=activation_start(&c,&a);CHECK(e&&!strcmp(e,"activation_revision_conflict"));CHECK(!held(model));
    c.advance_current=2;e=activation_start(&c,&a);CHECK(e&&!strcmp(e,"invalid_activation_configuration"));
    printf("PASS activation: %u checks; verified inputs, exclusive residency, strict and host-current revision/failure/mutation\n",checks);return 0;
}
