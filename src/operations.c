#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "operations.h"
#include "json.h"
#include "sha256.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <unistd.h>
#define RESULT_MAX 524288u
#define RESULTS_MAX (8u*1048576u)
static int identity(const char *s){
    if(!s||!*s||strlen(s)>128)return 0;
    for(const char *p=s;*p;p++)if(!isalnum((unsigned char)*p)&&!strchr("-_:.",*p))return 0;
    return 1;
}
static int safe_file(int dir,const char *name){
    int fd=openat(dir,name,O_CREAT|O_RDWR|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK,0600);struct stat info;
    if(fd>=0&&(fstat(fd,&info)||!S_ISREG(info.st_mode)||info.st_uid!=geteuid()||(info.st_mode&077)||info.st_nlink!=1)){close(fd);fd=-1;}
    return fd;
}
static int prepare(InferenceOperation *o,sqlite3_stmt **stmt,const char *sql){return sqlite3_prepare_v2(o->db,sql,-1,stmt,NULL)==SQLITE_OK;}
static void bind(sqlite3_stmt *stmt,int at,const char *s){sqlite3_bind_text(stmt,at,s,-1,SQLITE_TRANSIENT);}
static int sql(InferenceOperation *o,const char *text){return sqlite3_exec(o->db,text,NULL,NULL,NULL)==SQLITE_OK;}
static const char *open_state(const InferenceConfig *cfg,InferenceOperation *o){
    memset(o,0,sizeof(*o));o->directory=o->lease=o->cancel=-1;
    if(!cfg||!cfg->admission_state||!identity(cfg->owner))return "invalid_configuration";
    o->owner=cfg->owner;
    if(mkdir(cfg->admission_state,0700)&&errno!=EEXIST)return "state_unavailable";
    o->directory=open(cfg->admission_state,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    struct stat info;
    if(o->directory<0||fstat(o->directory,&info)||info.st_uid!=geteuid()||(info.st_mode&077))return "state_unavailable";
    int fd=safe_file(o->directory,"operations.sqlite3");if(fd<0)return "state_unavailable";close(fd);
    size_t length=strlen(cfg->admission_state)+32;char *path=malloc(length);if(!path)return "resource_limit";
    snprintf(path,length,"%s/operations.sqlite3",cfg->admission_state);
    int rc=sqlite3_open_v2(path,&o->db,SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE|SQLITE_OPEN_FULLMUTEX,NULL);free(path);
    if(rc!=SQLITE_OK)return "state_unavailable";
    sqlite3_busy_timeout(o->db,100);sqlite3_limit(o->db,SQLITE_LIMIT_LENGTH,RESULT_MAX);
    if(!sql(o,"PRAGMA journal_mode=DELETE;PRAGMA synchronous=FULL;PRAGMA max_page_count=4096;CREATE TABLE IF NOT EXISTS operations(owner TEXT NOT NULL,id TEXT NOT NULL,request TEXT NOT NULL,slot INTEGER UNIQUE NOT NULL,status TEXT NOT NULL,result TEXT,digest TEXT,cancel_requested INTEGER NOT NULL DEFAULT 0,PRIMARY KEY(owner,id));CREATE TABLE IF NOT EXISTS operation_parents(owner TEXT NOT NULL,id TEXT NOT NULL,parent TEXT NOT NULL,PRIMARY KEY(owner,id));"))return "store_failed";
    return NULL;
}
void operation_close(InferenceOperation *o){
    if(o->db)sqlite3_close(o->db);
    if(o->cancel>=0)close(o->cancel);
    if(o->lease>=0)close(o->lease);
    if(o->directory>=0)close(o->directory);
    free(o->id);memset(o,0,sizeof(*o));o->directory=o->lease=o->cancel=-1;
}
static int lease_file(InferenceOperation *o,int slot,int cancel){char name[64];snprintf(name,sizeof(name),"operation-%d.%s",slot,cancel?"cancel":"lock");return safe_file(o->directory,name);}
static const char *recover(InferenceOperation *o,const char *id,int slot){
    int fd=lease_file(o,slot,0);if(fd<0)return "state_unavailable";
    const char *error=NULL;
    if(flock(fd,LOCK_EX|LOCK_NB)){
        if(errno!=EWOULDBLOCK&&errno!=EAGAIN)error="state_unavailable";
    }else{
        sqlite3_stmt *stmt=NULL;
        if(!prepare(o,&stmt,"UPDATE operations SET status='interrupted' WHERE owner=? AND id=? AND status='running'"))error="store_failed";
        else{bind(stmt,1,o->owner);bind(stmt,2,id);if(sqlite3_step(stmt)!=SQLITE_DONE)error="store_failed";}
        sqlite3_finalize(stmt);
    }
    close(fd);return error;
}
static const char *lookup(InferenceOperation *o,const char *id,const char *request,int *found,char **saved,int *slot){
    *found=0;sqlite3_stmt *stmt=NULL;
    if(!prepare(o,&stmt,"SELECT request,status,result,digest,slot FROM operations WHERE owner=? AND id=?"))return "store_failed";
    bind(stmt,1,o->owner);bind(stmt,2,id);int rc=sqlite3_step(stmt);const char *error=NULL;
    if(rc==SQLITE_ROW){
        *found=1;const char *old=(const char*)sqlite3_column_text(stmt,0),*status=(const char*)sqlite3_column_text(stmt,1);
        *slot=sqlite3_column_int(stmt,4);
        if(*slot<0||*slot>=128)error="corrupt_operation";
        else if(request&&(!old||strcmp(old,request)))error="idempotency_conflict";
        else if(!status)error="corrupt_operation";
        else if(!strcmp(status,"running"))error="operation_running";
        else if(!strcmp(status,"interrupted"))error="operation_interrupted";
        else if(saved){
            const char *result=(const char*)sqlite3_column_text(stmt,2),*expected=(const char*)sqlite3_column_text(stmt,3);char actual[65];
            if(!result||strlen(result)>RESULT_MAX||!expected)error="corrupt_operation";
            else{hash_bytes(result,strlen(result),actual);if(strcmp(expected,actual))error="corrupt_operation";else if(!(*saved=strdup(result)))error="resource_limit";}
        }
    }else if(rc!=SQLITE_DONE)error="store_failed";
    sqlite3_finalize(stmt);return error;
}
static const char *reap_dead(InferenceOperation *o){
    struct Dead {char *owner,*id;int slot;} rows[128]={{0}};
    size_t count=0;sqlite3_stmt *stmt=NULL;const char *error=NULL;
    if(!prepare(o,&stmt,"SELECT owner,id,slot FROM operations WHERE status='running' LIMIT 129"))return "store_failed";
    int rc;
    while((rc=sqlite3_step(stmt))==SQLITE_ROW){
        if(count==128){error="corrupt_operation";break;}
        const char *owner=(const char*)sqlite3_column_text(stmt,0),*id=(const char*)sqlite3_column_text(stmt,1);
        if(!identity(owner)||!identity(id)){error="corrupt_operation";break;}
        rows[count].owner=strdup(owner);rows[count].id=strdup(id);rows[count].slot=sqlite3_column_int(stmt,2);
        count++;
        if(!rows[count-1].owner||!rows[count-1].id){error="resource_limit";break;}
        if(rows[count-1].slot<0||rows[count-1].slot>=128){error="corrupt_operation";break;}
    }
    sqlite3_finalize(stmt);if(!error&&rc!=SQLITE_DONE)error="store_failed";
    const char *configured_owner=o->owner;
    for(size_t i=0;i<count;i++){
        if(!error){o->owner=rows[i].owner;error=recover(o,rows[i].id,rows[i].slot);}
        free(rows[i].owner);free(rows[i].id);
    }
    o->owner=configured_owner;return error;
}
const char *operation_begin_parent(const InferenceConfig *cfg,const char *id,const char *request,const char *parent,InferenceOperation *o,char **saved){
    *saved=NULL;const char *error=open_state(cfg,o);if(error)return error;
    if(!identity(id)||(parent&&(!identity(parent)||!strcmp(parent,id))))return "invalid_request";
    int found=0,slot=-1;error=lookup(o,id,request,&found,saved,&slot);
    if(error&&!strcmp(error,"operation_running")){
        error=recover(o,id,slot);if(!error)error=lookup(o,id,request,&found,saved,&slot);
    }
    if(error||found)return error;
    int gate=safe_file(o->directory,"operations.lock");if(gate<0)return "state_unavailable";
    if(flock(gate,LOCK_EX|LOCK_NB)){close(gate);return "busy";}
    error=lookup(o,id,request,&found,saved,&slot);
    int transaction=0;
    if(!error&&!found){if(!sql(o,"BEGIN IMMEDIATE"))error="store_busy";else transaction=1;}
    if(!error&&!found)error=reap_dead(o);
    sqlite3_stmt *stmt=NULL;
    if(!error&&!found){
        if(!prepare(o,&stmt,"SELECT count(*),COALESCE(sum(length(CAST(result AS BLOB))),0),COALESCE(sum(status='running'),0) FROM operations"))error="store_failed";
        else if(sqlite3_step(stmt)!=SQLITE_ROW)error="store_failed";
        else{
            slot=sqlite3_column_int(stmt,0);sqlite3_int64 bytes=sqlite3_column_int64(stmt,1);
            sqlite3_int64 running=sqlite3_column_int64(stmt,2);
            if(slot>=128)error="operation_limit";
            else if(bytes<0||running<0||running>128||bytes+running*RESULT_MAX>(sqlite3_int64)(RESULTS_MAX-RESULT_MAX))error="operation_bytes_limit";
        }
        sqlite3_finalize(stmt);stmt=NULL;
        if(!error){o->lease=lease_file(o,slot,0);o->cancel=lease_file(o,slot,1);o->id=strdup(id);
            if(o->lease<0||o->cancel<0||!o->id)error="state_unavailable";
            else if(flock(o->lease,LOCK_EX|LOCK_NB))error="state_unavailable";
            else if(ftruncate(o->cancel,1)||pwrite(o->cancel,"0",1,0)!=1||fsync(o->cancel))error="state_unavailable";
        }
        if(!error&&!prepare(o,&stmt,"INSERT INTO operations(owner,id,request,slot,status) VALUES(?,?,?,?,'running')"))error="store_failed";
        if(!error){bind(stmt,1,o->owner);bind(stmt,2,id);bind(stmt,3,request);sqlite3_bind_int(stmt,4,slot);if(sqlite3_step(stmt)!=SQLITE_DONE)error="store_failed";}
        sqlite3_finalize(stmt);stmt=NULL;
        if(!error&&parent) {
            if(!prepare(o,&stmt,"INSERT INTO operation_parents(owner,id,parent) VALUES(?,?,?)"))error="store_failed";
            else{bind(stmt,1,o->owner);bind(stmt,2,id);bind(stmt,3,parent);if(sqlite3_step(stmt)!=SQLITE_DONE)error="store_failed";}
            sqlite3_finalize(stmt);
        }
    }
    if(transaction){if(error)sql(o,"ROLLBACK");else if(!sql(o,"COMMIT")){sql(o,"ROLLBACK");error="store_failed";}}
    close(gate);return error;
}
const char *operation_begin(const InferenceConfig *cfg,const char *id,const char *request,InferenceOperation *o,char **saved){
    return operation_begin_parent(cfg,id,request,NULL,o,saved);
}
int operation_cancelled(void *opaque){
    InferenceOperation *o=opaque;char flag=0;
    if(o&&o->parent_cancelled&&o->parent_cancelled(o->parent_cancellation))return 1;
    return o&&o->cancel>=0&&(pread(o->cancel,&flag,1,0)!=1||flag!='0');
}
const char *operation_finish(InferenceOperation *o,const char *result,int failed){
    if(!result||strlen(result)>RESULT_MAX)return "response_too_large";
    char digest[65];hash_bytes(result,strlen(result),digest);sqlite3_stmt *stmt=NULL;
    if(!prepare(o,&stmt,"UPDATE operations SET status=?,result=?,digest=? WHERE owner=? AND id=? AND status='running'"))return "store_failed";
    const char *status=failed?(strstr(result,"\"code\":\"cancelled\"")?"cancelled":"failed"):"completed";
    bind(stmt,1,status);bind(stmt,2,result);bind(stmt,3,digest);bind(stmt,4,o->owner);bind(stmt,5,o->id);
    int rc=sqlite3_step(stmt),changed=sqlite3_changes(o->db);sqlite3_finalize(stmt);return rc==SQLITE_DONE&&changed==1?NULL:"store_failed";
}
static int fail(FILE *out,const char *error){fprintf(out,"{\"contract_version\":1,\"error\":{\"code\":\"%s\"}}\n",error);return 1;}
int operation_public(const InferenceConfig *cfg,const char *op,const char *id,FILE *out){
    InferenceOperation o;const char *error=open_state(cfg,&o);int found=0,slot=-1;
    if(!error&&!identity(id))error="invalid_request";
    if(!error)error=lookup(&o,id,NULL,&found,NULL,&slot);
    if(error&&!strcmp(error,"operation_running"))error=recover(&o,id,slot);
    if(!error&&!found)error="unknown_operation";
    /* Interrupted is a valid public status; other access errors remain failures. */
    if(error&&!strcmp(error,"operation_interrupted"))error=NULL;
    sqlite3_stmt *stmt=NULL;
    if(!error&&!strcmp(op,"operation_cancel")){
        /* Commit the request before exposing the marker: a worker can observe
         * that byte and finish immediately. Updating only still-running rows
         * afterwards loses the acknowledgement when completion wins the race. */
        int notify=0;
        if(!prepare(&o,&stmt,"UPDATE operations SET cancel_requested=1 WHERE owner=? AND id=? AND status='running'"))error="store_failed";
        if(!error){bind(stmt,1,o.owner);bind(stmt,2,id);if(sqlite3_step(stmt)!=SQLITE_DONE)error="store_failed";else notify=sqlite3_changes(o.db)==1;}
        sqlite3_finalize(stmt);stmt=NULL;
        if(!error&&notify){
            int fd=lease_file(&o,slot,1);
            if(fd<0||pwrite(fd,"1",1,0)!=1||fsync(fd))error="state_unavailable";
            if(fd>=0)close(fd);
        }
    }
    if(!error&&!prepare(&o,&stmt,"SELECT status,cancel_requested,(SELECT parent FROM operation_parents WHERE operation_parents.owner=operations.owner AND operation_parents.id=operations.id) FROM operations WHERE owner=? AND id=?"))error="store_failed";
    if(!error){
        bind(stmt,1,o.owner);bind(stmt,2,id);
        if(sqlite3_step(stmt)!=SQLITE_ROW)error="unknown_operation";
        else{fputs("{\"contract_version\":1,\"operation_id\":",out);json_quote(out,id,strlen(id));fputs(",\"status\":",out);
            const char *status=(const char*)sqlite3_column_text(stmt,0);json_quote(out,status,strlen(status));
            fprintf(out,",\"cancel_requested\":%s",sqlite3_column_int(stmt,1)?"true":"false");
            const char *parent=(const char*)sqlite3_column_text(stmt,2);
            if(parent){fputs(",\"parent_operation_id\":",out);json_quote(out,parent,strlen(parent));}fputs("}\n",out);}
    }
    sqlite3_finalize(stmt);operation_close(&o);return error?fail(out,error):0;
}
