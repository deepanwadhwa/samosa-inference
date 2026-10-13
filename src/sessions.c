#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "sessions.h"
#include "json.h"
#include <sqlite3.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <unistd.h>
#define CACHE_MAX (8u*1048576u)
const char *inference_admit(const char *path,int *lock) {
    *lock=-1;if(!path||!*path)return "invalid_configuration";
    if(mkdir(path,0700)<0&&errno!=EEXIST)return "state_unavailable";
    int dir=open(path,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(dir<0)return "state_unavailable";
    struct stat st;int valid=fstat(dir,&st)==0&&st.st_uid==geteuid()&&!(st.st_mode&077);
    int fd=valid?openat(dir,"provider.lock",O_CREAT|O_RDWR|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK,0600):-1;close(dir);
    if(fd<0)return "state_unavailable";
    if(fstat(fd,&st)<0||!S_ISREG(st.st_mode)||st.st_uid!=geteuid()||(st.st_mode&077)||st.st_nlink!=1){close(fd);return "state_unavailable";}
    if(flock(fd,LOCK_EX|LOCK_NB)<0){close(fd);return "busy";}*lock=fd;return NULL;
}
static const char *open_state(const InferenceConfig *cfg,sqlite3 **db) {
    *db=NULL;if(!cfg||!cfg->admission_state||!cfg->owner||!*cfg->owner||strlen(cfg->owner)>128)return "state_unavailable";
    if(mkdir(cfg->admission_state,0700)<0&&errno!=EEXIST)return "state_unavailable";
    int dir=open(cfg->admission_state,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(dir<0)return "state_unavailable";
    struct stat st;int valid=fstat(dir,&st)==0&&st.st_uid==geteuid()&&!(st.st_mode&077);
    int fd=valid?openat(dir,"sessions.sqlite3",O_CREAT|O_RDWR|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK,0600):-1;
    close(dir);if(fd<0)return "state_unavailable";
    valid=fstat(fd,&st)==0&&S_ISREG(st.st_mode)&&st.st_uid==geteuid()&&!(st.st_mode&077)&&st.st_nlink==1;close(fd);if(!valid)return "state_unavailable";
    size_t n=strlen(cfg->admission_state)+32;char *path=malloc(n);if(!path)return "resource_limit";snprintf(path,n,"%s/sessions.sqlite3",cfg->admission_state);
    int rc=sqlite3_open_v2(path,db,SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE|SQLITE_OPEN_FULLMUTEX,NULL);free(path);
    if(rc!=SQLITE_OK){if(*db)sqlite3_close(*db);*db=NULL;return "state_unavailable";}
    sqlite3_busy_timeout(*db,2000);sqlite3_limit(*db,SQLITE_LIMIT_LENGTH,262144);
    if(sqlite3_exec(*db,"PRAGMA journal_mode=DELETE; PRAGMA synchronous=FULL; PRAGMA max_page_count=4096; CREATE TABLE IF NOT EXISTS sessions(id TEXT PRIMARY KEY,owner TEXT NOT NULL,provider TEXT NOT NULL,context INTEGER NOT NULL,revision INTEGER NOT NULL DEFAULT 0,busy INTEGER NOT NULL DEFAULT 0,prompt TEXT,result TEXT);",NULL,NULL,NULL)!=SQLITE_OK){sqlite3_close(*db);*db=NULL;return "state_unavailable";}
    return NULL;
}
static int prepare(sqlite3 *db,sqlite3_stmt **stmt,const char *sql){return sqlite3_prepare_v2(db,sql,-1,stmt,NULL)==SQLITE_OK;}
static void text(sqlite3_stmt *stmt,int index,const char *value){sqlite3_bind_text(stmt,index,value,-1,SQLITE_TRANSIENT);}
static const char *lookup(sqlite3 *db,const InferenceConfig *cfg,const char *id,size_t expected,int revision_check,sqlite3_stmt **row) {
    *row=NULL;if(!id||strlen(id)!=48)return "unknown_session";
    if(!prepare(db,row,"SELECT revision,busy,provider,context FROM sessions WHERE id=? AND owner=?"))return "store_failed";
    text(*row,1,id);text(*row,2,cfg->owner);int rc=sqlite3_step(*row);
    if(rc!=SQLITE_ROW)return rc==SQLITE_DONE?"unknown_session":"store_failed";
    if(strcmp((const char*)sqlite3_column_text(*row,2),cfg->provider_id)||(size_t)sqlite3_column_int64(*row,3)!=cfg->context_tokens)return "provider_changed";
    if(revision_check && (size_t)sqlite3_column_int64(*row,0)!=expected)return "stale_session";
    if(revision_check && sqlite3_column_int(*row,1))return "session_busy";
    return NULL;
}
const char *session_begin(const InferenceConfig *cfg,const char *id,size_t expected,const char *prompt) {
    if(strlen(prompt)>131072)return "session_context_limit";
    sqlite3 *db=NULL;const char *error=open_state(cfg,&db);if(error)return error;
    if(sqlite3_exec(db,"BEGIN IMMEDIATE",NULL,NULL,NULL)!=SQLITE_OK)error="store_busy";
    /* Caller owns the kernel work lease: any prior busy marker is interrupted. */
    if(!error&&sqlite3_exec(db,"UPDATE sessions SET busy=0 WHERE busy=1",NULL,NULL,NULL)!=SQLITE_OK)error="store_failed";
    sqlite3_stmt *stmt=NULL;if(!error)error=lookup(db,cfg,id,expected,1,&stmt);sqlite3_finalize(stmt);stmt=NULL;
    if(!error) {
        if(!prepare(db,&stmt,"SELECT coalesce(sum(coalesce(length(CAST(prompt AS BLOB)),0)+coalesce(length(CAST(result AS BLOB)),0)),0) FROM sessions"))error="store_failed";
        else if(sqlite3_step(stmt)!=SQLITE_ROW)error="store_failed";
        else if((sqlite3_uint64)sqlite3_column_int64(stmt,0)>CACHE_MAX-262144){error="storage_limit";}
        sqlite3_finalize(stmt);stmt=NULL;
    }
    if(!error){if(!prepare(db,&stmt,"UPDATE sessions SET busy=1 WHERE id=? AND owner=?"))error="store_failed";else{text(stmt,1,id);text(stmt,2,cfg->owner);if(sqlite3_step(stmt)!=SQLITE_DONE)error="store_failed";}sqlite3_finalize(stmt);stmt=NULL;}
    if(error)sqlite3_exec(db,"ROLLBACK",NULL,NULL,NULL);else if(sqlite3_exec(db,"COMMIT",NULL,NULL,NULL)!=SQLITE_OK){sqlite3_exec(db,"ROLLBACK",NULL,NULL,NULL);error="store_failed";}
    sqlite3_close(db);return error;
}
const char *session_finish(const InferenceConfig *cfg,const char *id,size_t expected,const char *prompt,const char *answer) {
    if(answer&&(strlen(prompt)>131072||strlen(answer)>65536))return "session_context_limit";
    sqlite3 *db=NULL;const char *error=open_state(cfg,&db);if(error)return error;
    if(sqlite3_exec(db,"BEGIN IMMEDIATE",NULL,NULL,NULL)!=SQLITE_OK)error="store_busy";
    sqlite3_stmt *stmt=NULL;
    if(!error) {
        const char *sql=answer?"UPDATE sessions SET busy=0,revision=revision+1,prompt=?,result=? WHERE id=? AND owner=? AND provider=? AND context=? AND revision=? AND busy=1":"UPDATE sessions SET busy=0 WHERE id=? AND owner=? AND provider=? AND context=? AND revision=? AND busy=1";
        if(!prepare(db,&stmt,sql))error="store_failed";
        else {
            int column=1;if(answer){text(stmt,column++,prompt);text(stmt,column++,answer);}text(stmt,column++,id);text(stmt,column++,cfg->owner);text(stmt,column++,cfg->provider_id);sqlite3_bind_int64(stmt,column++,(sqlite3_int64)cfg->context_tokens);sqlite3_bind_int64(stmt,column,(sqlite3_int64)expected);
            if(sqlite3_step(stmt)!=SQLITE_DONE||sqlite3_changes(db)!=1)error="stale_session";
        }
        sqlite3_finalize(stmt);
    }
    if(error)sqlite3_exec(db,"ROLLBACK",NULL,NULL,NULL);else if(sqlite3_exec(db,"COMMIT",NULL,NULL,NULL)!=SQLITE_OK){sqlite3_exec(db,"ROLLBACK",NULL,NULL,NULL);error="store_failed";}
    sqlite3_close(db);return error;
}
static char *new_id(void) {
    unsigned char data[24];int fd=open("/dev/urandom",O_RDONLY|O_CLOEXEC);if(fd<0)return NULL;size_t n=0;
    while(n<sizeof(data)){ssize_t got=read(fd,data+n,sizeof(data)-n);if(got<0&&errno==EINTR)continue;if(got<=0){close(fd);return NULL;}n+=(size_t)got;}close(fd);
    char *id=malloc(49);if(!id)return NULL;const char *hex="0123456789abcdef";for(size_t i=0;i<24;i++){id[i*2]=hex[data[i]>>4];id[i*2+1]=hex[data[i]&15];}id[48]=0;return id;
}
int session_public(const InferenceConfig *cfg,const char *op,const char *id,FILE *out) {
    sqlite3 *db=NULL;const char *error=open_state(cfg,&db);char *created=NULL;size_t revision=0;int busy=0;sqlite3_stmt *stmt=NULL;
    if(!error&&!strcmp(op,"session_create")) {
        if(sqlite3_exec(db,"BEGIN IMMEDIATE",NULL,NULL,NULL)!=SQLITE_OK)error="store_busy";
        if(!error){if(!prepare(db,&stmt,"SELECT count(*) FROM sessions"))error="store_failed";else if(sqlite3_step(stmt)!=SQLITE_ROW)error="store_failed";else if(sqlite3_column_int(stmt,0)>=128)error="storage_limit";sqlite3_finalize(stmt);stmt=NULL;}
        if(!error){created=new_id();if(!created)error="resource_limit";else id=created;}
        if(!error){if(!prepare(db,&stmt,"INSERT INTO sessions(id,owner,provider,context) VALUES(?,?,?,?)"))error="store_failed";else{text(stmt,1,id);text(stmt,2,cfg->owner);text(stmt,3,cfg->provider_id);sqlite3_bind_int64(stmt,4,(sqlite3_int64)cfg->context_tokens);if(sqlite3_step(stmt)!=SQLITE_DONE)error="store_failed";}sqlite3_finalize(stmt);stmt=NULL;}
        if(error)sqlite3_exec(db,"ROLLBACK",NULL,NULL,NULL);else if(sqlite3_exec(db,"COMMIT",NULL,NULL,NULL)!=SQLITE_OK){sqlite3_exec(db,"ROLLBACK",NULL,NULL,NULL);error="store_failed";}
    } else if(!error) {
        int lock=-1;const char *locked=inference_admit(cfg->admission_state,&lock);
        if(!locked) {
            if(sqlite3_exec(db,"UPDATE sessions SET busy=0 WHERE busy=1",NULL,NULL,NULL)!=SQLITE_OK)error="store_failed";
            close(lock);
        } else if(strcmp(locked,"busy"))error=locked;
        if(!error){error=lookup(db,cfg,id,0,0,&stmt);}if(!error){revision=(size_t)sqlite3_column_int64(stmt,0);busy=sqlite3_column_int(stmt,1);}sqlite3_finalize(stmt);stmt=NULL;
    }
    if(!error)fprintf(out,"{\"contract_version\":1,\"session_id\":\"%s\",\"revision\":%zu,\"provider_id\":\"%s\",\"busy\":%s}\n",id,revision,cfg->provider_id,busy?"true":"false");
    if(db){sqlite3_close(db);}free(created);
    if(error){fprintf(out,"{\"contract_version\":1,\"error\":{\"code\":\"%s\"}}\n",error);return 1;}return 0;
}
