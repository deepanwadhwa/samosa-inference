#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "activation.h"
#include "artifact.h"
#include "json.h"
#include "sha256.h"
#include "engine_runtime.h"
#include <sqlite3.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>

typedef struct {int fd;struct stat before;} Input;
static int private_file(int fd,int directory) {
    struct stat s;return !fstat(fd,&s)&&s.st_uid==geteuid()&&!(s.st_mode&077)&&
        (directory?S_ISDIR(s.st_mode):S_ISREG(s.st_mode));
}
static int same_path(const char *path,int fd) {
    struct stat a,b;return !fstat(fd,&a)&&!lstat(path,&b)&&a.st_dev==b.st_dev&&a.st_ino==b.st_ino&&a.st_mode==b.st_mode;
}
static int stable_db(sqlite3 *db,const char *state,int root,int anchor) {
    char path[4096];int moved=1;
    if(snprintf(path,sizeof(path),"%s/activation.sqlite3",state)>=(int)sizeof(path))return 0;
    return sqlite3_file_control(db,"main",SQLITE_FCNTL_HAS_MOVED,&moved)==SQLITE_OK&&!moved&&
        same_path(state,root)&&same_path(path,anchor);
}
static int lease_same(Activation *a) {
    struct stat held,current;
    return !fstat(a->lease,&held)&&!fstatat(a->root,"residency.lock",&current,AT_SYMLINK_NOFOLLOW)&&
        held.st_dev==current.st_dev&&held.st_ino==current.st_ino&&held.st_mode==current.st_mode;
}
static int unchanged(const Input *input,const char *path) {
    struct stat now;
    if(fstat(input->fd,&now)||!same_path(path,input->fd)||now.st_size!=input->before.st_size)return 0;
#ifdef __APPLE__
    return now.st_mtimespec.tv_sec==input->before.st_mtimespec.tv_sec&&now.st_mtimespec.tv_nsec==input->before.st_mtimespec.tv_nsec&&
        now.st_ctimespec.tv_sec==input->before.st_ctimespec.tv_sec&&now.st_ctimespec.tv_nsec==input->before.st_ctimespec.tv_nsec;
#else
    return now.st_mtim.tv_sec==input->before.st_mtim.tv_sec&&now.st_mtim.tv_nsec==input->before.st_mtim.tv_nsec&&
        now.st_ctim.tv_sec==input->before.st_ctim.tv_sec&&now.st_ctim.tv_nsec==input->before.st_ctim.tv_nsec;
#endif
}
static const char *verified(const char *path,const char *sha,uint64_t size,uint64_t ceiling,Input *input) {
    input->fd=-1;
    if(!path||path[0]!='/')return "invalid_activation_configuration";
    input->fd=open(path,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
    if(input->fd<0)return "activation_input_unavailable";
    if(!private_file(input->fd,0))return "unsafe_activation_input";
    const char *e=artifact_verify(input->fd,size,ceiling,sha);
    if(!e&&(fstat(input->fd,&input->before)||!same_path(path,input->fd)))e="activation_input_changed";
    return e;
}
static char *render(const ActivationConfig *c) {
    if(!c->argv_template||strlen(c->argv_template)>16384)return NULL;
    Json j;if(!json_parse(&j,c->argv_template,strlen(c->argv_template))||j.tokens[0].type!=JT_ARRAY||
        j.tokens[0].count<2||j.tokens[0].count>32)return NULL;
    char *out=NULL;size_t n;FILE *f=open_memstream(&out,&n);if(!f)return NULL;
    unsigned engines=0,models=0,index=0;int valid=1;fputc('[',f);
    for(int t=1;t<j.tokens[0].next;t=j.tokens[t].next){char *arg=json_string(&j,t);
        if(!arg||strlen(arg)>4096){free(arg);valid=0;break;}
        const char *value=arg;
        if(!strcmp(arg,"@ENGINE@")){engines++;if(index)valid=0;value=c->engine;}
        else if(!strcmp(arg,"@MODEL@")){models++;value="/dev/fd/5";}
        else if(!index)valid=0;
        if(index++)fputc(',',f);
        json_quote(f,value,strlen(value));free(arg);
    }
    fputc(']',f);if(fclose(f)||!valid||engines!=1||models!=1||n>16384){free(out);return NULL;}return out;
}
static int root_open(const char *state,int create) {
    if(!state||state[0]!='/')return -1;
    if(create&&mkdir(state,0700)&&errno!=EEXIST)return -1;
    int fd=open(state,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if(fd>=0&&!private_file(fd,1)){close(fd);return -1;}return fd;
}
static const char *db_open(const char *state,int root,sqlite3 **db,int *anchor) {
    *db=NULL;*anchor=-1;char path[4096];
    if(snprintf(path,sizeof(path),"%s/activation.sqlite3",state)>=(int)sizeof(path))return "invalid_activation_configuration";
    *anchor=openat(root,"activation.sqlite3",O_RDWR|O_CREAT|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC,0600);
    if(*anchor<0||!private_file(*anchor,0)||!same_path(path,*anchor)||!same_path(state,root))return "unsafe_activation_state";
    struct stat s;if(fstat(*anchor,&s)||s.st_nlink!=1||s.st_size>1048576)return "activation_state_limit";
    int journal=openat(root,"activation.sqlite3-journal",O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
    if(journal>=0){int safe=private_file(journal,0)&&!fstat(journal,&s)&&s.st_nlink==1&&s.st_size<=1048576;close(journal);
        if(!safe)return "unsafe_activation_state";}
    else if(errno!=ENOENT)return "unsafe_activation_state";
    if(sqlite3_open_v2(path,db,SQLITE_OPEN_READWRITE|SQLITE_OPEN_NOMUTEX,NULL)!=SQLITE_OK)return "activation_state_unavailable";
    sqlite3_busy_timeout(*db,0);sqlite3_limit(*db,SQLITE_LIMIT_LENGTH,32768);
    if(!stable_db(*db,state,root,*anchor))return "activation_state_changed";
    if(sqlite3_exec(*db,"PRAGMA synchronous=FULL;PRAGMA page_size=4096",NULL,NULL,NULL)!=SQLITE_OK)
        return "activation_state_unavailable";
    sqlite3_stmt *page=NULL;
    if(sqlite3_prepare_v2(*db,"PRAGMA page_size",-1,&page,NULL)!=SQLITE_OK)return "activation_state_unavailable";
    int correct=sqlite3_step(page)==SQLITE_ROW&&sqlite3_column_int(page,0)==4096;sqlite3_finalize(page);
    if(!correct)return "activation_state_limit";
    if(sqlite3_exec(*db,"PRAGMA journal_mode=DELETE;PRAGMA max_page_count=256;"
        "CREATE TABLE IF NOT EXISTS current(id INTEGER PRIMARY KEY CHECK(id=1),revision INTEGER NOT NULL,config TEXT NOT NULL,digest TEXT NOT NULL);",NULL,NULL,NULL)!=SQLITE_OK)
        return "activation_state_unavailable";
    if(!stable_db(*db,state,root,*anchor))return "activation_state_changed";
    return NULL;
}
static const char *current(sqlite3 *db,unsigned *revision,char **config) {
    *revision=0;*config=NULL;sqlite3_stmt *q=NULL;
    if(sqlite3_prepare_v2(db,"SELECT revision,config,digest FROM current WHERE id=1",-1,&q,NULL)!=SQLITE_OK)return "activation_state_unavailable";
    int rc=sqlite3_step(q);const char *e=NULL;
    if(rc==SQLITE_ROW){sqlite3_int64 n=sqlite3_column_int64(q,0);const char *text=(const char*)sqlite3_column_text(q,1),*saved=(const char*)sqlite3_column_text(q,2);
        char digest[65];Json j;
        if(!text||!saved||n<1||n>2147483646||strlen(text)>16384||strlen(saved)!=64||!json_parse(&j,text,strlen(text))||j.tokens[0].type!=JT_OBJECT)e="corrupt_activation_state";
        else{hash_bytes(text,strlen(text),digest);if(strcmp(digest,saved))e="corrupt_activation_state";
            else{*config=strdup(text);if(!*config)e="activation_resource_limit";else *revision=(unsigned)n;}}
    }else if(rc!=SQLITE_DONE)e="activation_state_unavailable";
    sqlite3_finalize(q);return e;
}
const char *activation_stop(Activation *a) {
    const char *e=engine_process_stop(&a->process);
    if(e&&!strcmp(e,"engine_shutdown_pending"))return e;
    if(a->engine_fd>=0){close(a->engine_fd);a->engine_fd=-1;}
    if(a->model_fd>=0){close(a->model_fd);a->model_fd=-1;}
    if(a->lease>=0){close(a->lease);a->lease=-1;}
    if(a->root>=0){close(a->root);a->root=-1;}return e;
}
const char *activation_get(const char *state,FILE *out) {
    int root=root_open(state,0),anchor=-1;sqlite3 *db=NULL;
    if(root<0)return "activation_state_unavailable";
    const char *e=db_open(state,root,&db,&anchor);unsigned revision=0;char *config=NULL;
    if(!e)e=current(db,&revision,&config);
    if(!e&&!stable_db(db,state,root,anchor))e="activation_state_changed";
    if(!e){fprintf(out,"{\"contract_version\":1,\"revision\":%u,\"live_readiness\":\"unobserved\",\"last_good\":%s}\n",revision,config?config:"null");}
    free(config);if(db)sqlite3_close(db);if(anchor>=0)close(anchor);close(root);return e;
}
const char *activation_start(const ActivationConfig *c,Activation *a) {
    *a=(Activation){.process={.guardian=-1,.control=-1,.response=-1},.root=-1,.lease=-1,.engine_fd=-1,.model_fd=-1};
    if(!c||!c->qualify||!c->self||c->self[0]!='/'||!c->engine||!c->model||c->expected_revision>=2147483646||
        (c->advance_current!=0&&c->advance_current!=1))
        return "invalid_activation_configuration";
    a->root=root_open(c->state,1);if(a->root<0)return "unsafe_activation_state";
    a->lease=openat(a->root,"residency.lock",O_RDWR|O_CREAT|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC,0600);
    const char *e=NULL;
    if(a->lease<0||!private_file(a->lease,0))e="unsafe_activation_state";
    else if(flock(a->lease,LOCK_EX|LOCK_NB))e=errno==EWOULDBLOCK?"activation_busy":"activation_state_unavailable";
    sqlite3 *db=NULL;int anchor=-1;unsigned revision=0;char *saved=NULL,*args=NULL,*config=NULL;Input engine={.fd=-1},model={.fd=-1};
    if(!e)e=db_open(c->state,a->root,&db,&anchor);
    if(!e)e=current(db,&revision,&saved);
    free(saved);
    if(!e&&revision>=2147483646)e="activation_revision_limit";
    if(!e&&!c->advance_current&&revision!=c->expected_revision)e="activation_revision_conflict";
    if(!e)e=verified(c->engine,c->engine_sha,c->engine_size,c->byte_ceiling,&engine);
    a->engine_fd=engine.fd;
    if(!e&&c->require_system_runtime)e=engine_runtime_environment();
    if(!e&&c->require_system_runtime)e=engine_system_runtime(a->engine_fd);
    if(!e)e=verified(c->model,c->model_sha,c->model_size,c->byte_ceiling,&model);
    a->model_fd=model.fd;
    if(!e && !(args=render(c)))e="invalid_activation_configuration";
    if(!e)e=engine_process_start_bound(c->self,args,c->lifetime,c->log_limit,a->lease,a->model_fd,&a->process);
    if(!e)e=c->qualify(c->qualification);
    struct pollfd terminal={a->process.response,POLLIN|POLLHUP,0};
    if(!e&&poll(&terminal,1,0)!=0)e="activation_engine_exited";
    if(!e&&(!unchanged(&engine,c->engine)||!unchanged(&model,c->model)||!same_path(c->state,a->root)||!lease_same(a)))e="activation_input_changed";
    if(!e){size_t n;FILE *f=open_memstream(&config,&n);if(!f)e="activation_resource_limit";
        else{fputs("{\"engine_sha256\":",f);json_quote(f,c->engine_sha,64);fputs(",\"model_sha256\":",f);json_quote(f,c->model_sha,64);
            fprintf(f,",\"engine_size\":%llu,\"model_size\":%llu,\"model_path\":",(unsigned long long)c->engine_size,(unsigned long long)c->model_size);
            json_quote(f,c->model,strlen(c->model));fprintf(f,",\"runtime_policy\":\"%s\",\"argv\":%s}",c->require_system_runtime?"system-runtime-v1":"unqualified",args);
            if(fclose(f)||n>16384)e="activation_resource_limit";}}
    if(!e){char digest[65];hash_bytes(config,strlen(config),digest);sqlite3_stmt *q=NULL;
        if(sqlite3_exec(db,"BEGIN IMMEDIATE",NULL,NULL,NULL)!=SQLITE_OK)e="activation_state_unavailable";
        if(!e&&sqlite3_prepare_v2(db,"INSERT INTO current(id,revision,config,digest) VALUES(1,?1,?2,?3) ON CONFLICT(id) DO UPDATE SET revision=excluded.revision,config=excluded.config,digest=excluded.digest",-1,&q,NULL)!=SQLITE_OK)e="activation_state_unavailable";
        if(!e){sqlite3_bind_int(q,1,(int)revision+1);sqlite3_bind_text(q,2,config,-1,SQLITE_TRANSIENT);sqlite3_bind_text(q,3,digest,-1,SQLITE_TRANSIENT);
            if(sqlite3_step(q)!=SQLITE_DONE)e="activation_state_unavailable";}
        sqlite3_finalize(q);
        if(!e&&(!stable_db(db,c->state,a->root,anchor)||!lease_same(a)))e="activation_state_changed";
        if(!e&&sqlite3_exec(db,"COMMIT",NULL,NULL,NULL)!=SQLITE_OK)e="activation_commit_uncertain";
        if(e)(void)sqlite3_exec(db,"ROLLBACK",NULL,NULL,NULL);
        if(!e){a->revision=revision+1;
#ifdef SAMOSA_INFERENCE_TEST_FAULTS
            if(c->checkpoint)c->checkpoint("committed",c->checkpoint_context);
#endif
            if(fsync(a->root))e="activation_commit_uncertain";}
    }
    free(args);free(config);if(db)sqlite3_close(db);if(anchor>=0)close(anchor);
    if(e&&strcmp(e,"activation_commit_uncertain")){const char *cleanup=activation_stop(a);if(cleanup&&!strcmp(cleanup,"engine_shutdown_pending"))e=cleanup;}
    return e;
}
