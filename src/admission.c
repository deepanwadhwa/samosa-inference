#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "admission.h"
#include "budget.h"
#include <sqlite3.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {int directory,gate,waiter,slot;sqlite3 *db;sqlite3_int64 ticket;} Admission;
static int safe_file(int directory,const char *name){
    int fd=openat(directory,name,O_CREAT|O_RDWR|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK,0600);
    struct stat info;
    if(fd>=0&&(fstat(fd,&info)||!S_ISREG(info.st_mode)||info.st_uid!=geteuid()||
               (info.st_mode&077)||info.st_nlink!=1)){close(fd);fd=-1;}
    return fd;
}
static int sql(Admission *a,const char *text){return sqlite3_exec(a->db,text,NULL,NULL,NULL)==SQLITE_OK;}
static int prepare(Admission *a,sqlite3_stmt **stmt,const char *text){return sqlite3_prepare_v2(a->db,text,-1,stmt,NULL)==SQLITE_OK;}
static sqlite3_int64 scalar(Admission *a,const char *text){
    sqlite3_stmt *stmt=NULL;sqlite3_int64 n=-1;
    if(prepare(a,&stmt,text)&&sqlite3_step(stmt)==SQLITE_ROW)n=sqlite3_column_int64(stmt,0);
    sqlite3_finalize(stmt);return n;
}
static int slot_file(Admission *a,int slot){char name[32];snprintf(name,sizeof(name),"waiter-%d.lock",slot);return safe_file(a->directory,name);}
static const char *remove_ticket(Admission *a,sqlite3_int64 ticket){
    sqlite3_stmt *stmt=NULL;if(!prepare(a,&stmt,"DELETE FROM tickets WHERE sequence=?"))return "store_failed";
    sqlite3_bind_int64(stmt,1,ticket);int rc=sqlite3_step(stmt);sqlite3_finalize(stmt);return rc==SQLITE_DONE?NULL:"store_failed";
}
/* Gate serializes association changes; a held per-slot lease proves liveness. */
static const char *reap(Admission *a){
    sqlite3_stmt *stmt=NULL;sqlite3_int64 dead[9];size_t count=0;
    if(!prepare(a,&stmt,"SELECT sequence,slot FROM tickets ORDER BY sequence LIMIT 9"))return "store_failed";
    int rc;const char *error=NULL;
    while((rc=sqlite3_step(stmt))==SQLITE_ROW){
        int slot=sqlite3_column_int(stmt,1);
        if(slot<0||slot>8){error="corrupt_admission";break;}
        sqlite3_int64 ticket=sqlite3_column_int64(stmt,0);
        if(ticket==a->ticket)continue;
        int fd=slot_file(a,slot);
        if(fd<0){error="state_unavailable";break;}
        if(!flock(fd,LOCK_EX|LOCK_NB))dead[count++]=ticket;
        else if(errno!=EWOULDBLOCK&&errno!=EAGAIN)error="state_unavailable";
        close(fd);if(error)break;
    }
    sqlite3_finalize(stmt);
    if(!error&&rc!=SQLITE_DONE)error="store_failed";
    for(size_t i=0;!error&&i<count;i++)error=remove_ticket(a,dead[i]);
    return error;
}
static const char *provider(Admission *a,int *lock){
    int fd=safe_file(a->directory,"provider.lock");if(fd<0)return "state_unavailable";
    if(flock(fd,LOCK_EX|LOCK_NB)){
        int busy=errno==EWOULDBLOCK||errno==EAGAIN;close(fd);return busy?"busy":"state_unavailable";
    }
    *lock=fd;return NULL;
}
static const char *open_admission(Admission *a,const char *path){
    if(!path||!*path)return "invalid_configuration";
    if(mkdir(path,0700)&&errno!=EEXIST)return "state_unavailable";
    a->directory=open(path,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    struct stat info;
    if(a->directory<0||fstat(a->directory,&info)||info.st_uid!=geteuid()||(info.st_mode&077))return "state_unavailable";
    a->gate=safe_file(a->directory,"admission.lock");if(a->gate<0)return "state_unavailable";
    int db_file=safe_file(a->directory,"admission.sqlite3");if(db_file<0)return "state_unavailable";close(db_file);
    size_t length=strlen(path)+32;char *database=malloc(length);if(!database)return "resource_limit";
    snprintf(database,length,"%s/admission.sqlite3",path);
    int rc=sqlite3_open_v2(database,&a->db,SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE|SQLITE_OPEN_FULLMUTEX,NULL);free(database);
    if(rc!=SQLITE_OK)return "state_unavailable";
    sqlite3_busy_timeout(a->db,100);sqlite3_limit(a->db,SQLITE_LIMIT_LENGTH,16384);
    return NULL;
}
static const char *schema(Admission *a){
    return sql(a,"PRAGMA journal_mode=DELETE;PRAGMA synchronous=FULL;PRAGMA max_page_count=256;CREATE TABLE IF NOT EXISTS ordering(id INTEGER PRIMARY KEY CHECK(id=1),sequence INTEGER NOT NULL);INSERT OR IGNORE INTO ordering VALUES(1,0);CREATE TABLE IF NOT EXISTS tickets(sequence INTEGER PRIMARY KEY,slot INTEGER UNIQUE NOT NULL CHECK(slot BETWEEN 0 AND 8));")?NULL:"store_failed";
}
static const char *join(Admission *a,size_t capacity,int *lock){
    const char *error=reap(a);if(error)return error;
    sqlite3_int64 count=scalar(a,"SELECT count(*) FROM tickets");if(count<0||count>8)return "corrupt_admission";
    if(!count){error=provider(a,lock);if(!error)return NULL;if(strcmp(error,"busy"))return error;error=NULL;}
    if(!capacity)return "busy";
    if((size_t)count>=capacity)return "queue_full";
    for(int slot=0;slot<9;slot++){
        int fd=slot_file(a,slot);if(fd<0)return "state_unavailable";
        if(!flock(fd,LOCK_EX|LOCK_NB)){a->waiter=fd;a->slot=slot;break;}
        int busy=errno==EWOULDBLOCK||errno==EAGAIN;close(fd);if(!busy)return "state_unavailable";
    }
    if(a->waiter<0)return "queue_full";
    if(!sql(a,"BEGIN IMMEDIATE"))return "store_busy";
    sqlite3_int64 sequence=scalar(a,"SELECT sequence FROM ordering WHERE id=1");
    if(sequence<0||sequence==INT64_MAX)error="admission_sequence_limit";
    sqlite3_stmt *stmt=NULL;
    if(!error&&!sql(a,"UPDATE ordering SET sequence=sequence+1 WHERE id=1"))error="store_failed";
    if(!error&&!prepare(a,&stmt,"INSERT INTO tickets(sequence,slot) VALUES(?,?)"))error="store_failed";
    if(!error){sqlite3_bind_int64(stmt,1,sequence+1);sqlite3_bind_int(stmt,2,a->slot);if(sqlite3_step(stmt)!=SQLITE_DONE)error="store_failed";}
    sqlite3_finalize(stmt);
    if(error)sql(a,"ROLLBACK");
    else if(!sql(a,"COMMIT")){sql(a,"ROLLBACK");error="store_failed";}
    else a->ticket=sequence+1;
    return error;
}
static double now(void){struct timespec time;if(clock_gettime(CLOCK_MONOTONIC,&time))return -1;return (double)time.tv_sec+(double)time.tv_nsec/1000000000.0;}
static void pause_wait(void){struct timespec delay={0,20000000};nanosleep(&delay,NULL);}
const char *inference_schedule(const InferenceConfig *cfg,int *lock){
    *lock=-1;
    if(!cfg||cfg->queue_capacity>8||cfg->admission_wait_ms>60000)return "invalid_configuration";
    Admission a={.directory=-1,.gate=-1,.waiter=-1,.slot=-1};
    double start=now();if(start<0)return "clock_unavailable";
    double deadline=start+(double)(cfg->admission_wait_ms?cfg->admission_wait_ms:10000)/1000.0;
    const char *error=open_admission(&a,cfg->admission_state);int held=0;
    while(!error&&!held){
        if((error=inference_interrupted(cfg)))break;
        if(!flock(a.gate,LOCK_EX|LOCK_NB)){held=1;break;}
        if(errno!=EWOULDBLOCK&&errno!=EAGAIN){error="state_unavailable";break;}
        if(!cfg->queue_capacity){error="busy";break;}
        double time=now();
        if(time<0){error="clock_unavailable";break;}
        if(time>=deadline){error="admission_deadline_exceeded";break;}
        pause_wait();
    }
    if(!error)error=schema(&a);
    if(!error)error=inference_interrupted(cfg);
    if(!error)error=join(&a,cfg->queue_capacity,lock);
    if(held){flock(a.gate,LOCK_UN);held=0;}
#ifdef SAMOSA_INFERENCE_TEST_FAULTS
    if(!error&&a.ticket){const char *marker=getenv("SAMOSA_INFERENCE_QUEUE_READY");if(marker){int fd=open(marker,O_CREAT|O_WRONLY|O_EXCL|O_CLOEXEC,0600);if(fd>=0)close(fd);}}
#endif
    while(!error&&a.ticket&&*lock<0){
        if((error=inference_interrupted(cfg)))break;
        double time=now();if(time<0){error="clock_unavailable";break;}
        if(time>=deadline){error="admission_deadline_exceeded";break;}
        if(flock(a.gate,LOCK_EX|LOCK_NB)){
            if(errno!=EWOULDBLOCK&&errno!=EAGAIN){error="state_unavailable";break;}
            pause_wait();continue;
        }
        held=1;error=reap(&a);
        sqlite3_int64 first=error?-1:scalar(&a,"SELECT min(sequence) FROM tickets");
        if(!error&&first<0)error="store_failed";
        if(!error&&first==a.ticket){
            const char *admitted=provider(&a,lock);
            if(admitted&&strcmp(admitted,"busy"))error=admitted;
            if(!admitted){error=remove_ticket(&a,a.ticket);if(!error)a.ticket=0;}
        }
        flock(a.gate,LOCK_UN);held=0;
        if(!error&&*lock<0)pause_wait();
    }
    /* On failure the own ticket stays protected until deleted or lease closes;
     * later admission safely reaps it if a concurrent gate prevents cleanup. */
    if(a.ticket&&a.gate>=0&&!flock(a.gate,LOCK_EX|LOCK_NB)){held=1;remove_ticket(&a,a.ticket);}
    if(held)flock(a.gate,LOCK_UN);
    if(error&&*lock>=0){close(*lock);*lock=-1;}
    if(a.db)sqlite3_close(a.db);
    if(a.waiter>=0)close(a.waiter);
    if(a.gate>=0)close(a.gate);
    if(a.directory>=0)close(a.directory);
    return error;
}
