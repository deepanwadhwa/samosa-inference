#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "audio_store.h"
#include "speech_worker.h"
#include "sha256.h"
#include <sqlite3.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#define STORE_MAX (64u*1048576u)
#define ITEMS_MAX 128
/* Audio uses a separate bounded store; operation JSON limits stay unchanged.
 * SQLite transactions make partial copies and interrupted publication invisible. */
typedef struct {sqlite3 *db;int directory;} Store;
static int name(const char *s) {
    if(!s||!*s||strlen(s)>128)return 0;
    for(;*s;s++)if(!isalnum((unsigned char)*s)&&!strchr("-_:.",*s))return 0;
    return 1;
}
static int pin(const char *s) {
    if(!s||strlen(s)!=64)return 0;
    for(size_t i=0;i<64;i++)if(!strchr("0123456789abcdef",s[i]))return 0;
    return 1;
}
static void close_store(Store *s) {if(s->db)sqlite3_close(s->db);if(s->directory>=0)close(s->directory);}
static void bind(sqlite3_stmt *q,int i,const char *s) {sqlite3_bind_text(q,i,s,-1,SQLITE_TRANSIENT);}
static int sql(Store *s,const char *text) {return sqlite3_exec(s->db,text,NULL,NULL,NULL)==SQLITE_OK;}
static int prepare(Store *s,sqlite3_stmt **q,const char *text) {return sqlite3_prepare_v2(s->db,text,-1,q,NULL)==SQLITE_OK;}
static const char *open_store(const AudioStoreConfig *c,Store *s) {
    *s=(Store){.directory=-1};
    if(!c||!c->state||c->state[0]!='/'||!name(c->owner)||!name(c->provider))return "invalid_configuration";
    if(mkdir(c->state,0700)&&errno!=EEXIST)return "audio_store_unavailable";
    s->directory=open(c->state,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    struct stat dir,file,after;
    if(s->directory<0||fstat(s->directory,&dir)||dir.st_uid!=geteuid()||(dir.st_mode&077))return "audio_store_unavailable";
    int fd=openat(s->directory,"audio.sqlite3",O_CREAT|O_RDWR|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK,0600);
    if(fd<0)return "audio_store_unavailable";
    int valid=!fstat(fd,&file)&&S_ISREG(file.st_mode)&&file.st_uid==geteuid()&&!(file.st_mode&077)&&file.st_nlink==1;
    close(fd);if(!valid)return "audio_store_unavailable";
    size_t n=strlen(c->state)+32;char *path=malloc(n);if(!path)return "resource_limit";
    snprintf(path,n,"%s/audio.sqlite3",c->state);
    int rc=sqlite3_open_v2(path,&s->db,SQLITE_OPEN_READWRITE|SQLITE_OPEN_FULLMUTEX,NULL);free(path);
    if(rc!=SQLITE_OK||fstatat(s->directory,"audio.sqlite3",&after,AT_SYMLINK_NOFOLLOW)||
        after.st_dev!=file.st_dev||after.st_ino!=file.st_ino)return "audio_store_unavailable";
    sqlite3_busy_timeout(s->db,100);sqlite3_limit(s->db,SQLITE_LIMIT_LENGTH,SPEECH_PACKET_LIMIT+65536);
    sqlite3_stmt *page=NULL;
    int page_ok=sqlite3_prepare_v2(s->db,"PRAGMA page_size",-1,&page,NULL)==SQLITE_OK&&
        sqlite3_step(page)==SQLITE_ROW&&sqlite3_column_int(page,0)==4096;
    sqlite3_finalize(page);if(!page_ok)return "audio_store_unavailable";
    if(!sql(s,"PRAGMA journal_mode=DELETE;PRAGMA synchronous=FULL;PRAGMA max_page_count=17408;"
        "CREATE TABLE IF NOT EXISTS audio(owner TEXT NOT NULL,provider TEXT NOT NULL,operation TEXT NOT NULL,request TEXT NOT NULL,id TEXT NOT NULL,rate INTEGER NOT NULL,samples INTEGER NOT NULL,digest TEXT NOT NULL,pcm BLOB NOT NULL,UNIQUE(owner,provider,operation),UNIQUE(owner,provider,id));"))return "audio_store_failed";
    return NULL;
}
static const char *receipt(sqlite3_stmt *q,AudioReceipt *r) {
    const char *id=(const char*)sqlite3_column_text(q,1),*digest=(const char*)sqlite3_column_text(q,4);
    sqlite3_int64 samples=sqlite3_column_int64(q,3),bytes=sqlite3_column_int64(q,5);
    int rate=sqlite3_column_int(q,2);
    if(!pin(id)||!pin(digest)||rate<8000||rate>96000||samples<=0||samples>(sqlite3_int64)rate*45||bytes!=samples*2||bytes>SPEECH_PACKET_LIMIT-SPEECH_PACKET_HEADER)return "corrupt_audio";
    snprintf(r->id,sizeof(r->id),"%s",id);snprintf(r->pcm_sha256,sizeof(r->pcm_sha256),"%s",digest);
    r->sample_rate=rate;r->samples=(uint64_t)samples;r->bytes=(uint64_t)bytes;return NULL;
}
/* Read under a SQLite transaction so hash and range share one snapshot. */
static const char *check_blob(Store *s,sqlite3_int64 row,const AudioReceipt *r,
    uint64_t offset,size_t length,unsigned char *output,const char *(*control)(void *),void *context) {
    sqlite3_blob *blob=NULL;
    if(sqlite3_blob_open(s->db,"main","audio","pcm",row,0,&blob)!=SQLITE_OK)return "corrupt_audio";
    const char *e=NULL;Sha256 hash;sha256_init(&hash);unsigned char chunk[65536],digest[32];char hex[65];
    if(sqlite3_blob_bytes(blob)!=(int)r->bytes)e="corrupt_audio";
    for(uint64_t at=0;!e&&at<r->bytes;) {
        e=control?control(context):NULL;if(e)break;
        size_t n=(size_t)(r->bytes-at);if(n>sizeof(chunk))n=sizeof(chunk);
        if(sqlite3_blob_read(blob,chunk,(int)n,(int)at)!=SQLITE_OK)e="corrupt_audio";
        else sha256_update(&hash,chunk,n);
        at+=n;
    }
    if(!e){sha256_final(&hash,digest);sha256_hex(digest,hex);if(strcmp(hex,r->pcm_sha256))e="corrupt_audio";}
    if(!e)e=control?control(context):NULL;
    if(!e&&length&&sqlite3_blob_read(blob,output,(int)length,(int)offset)!=SQLITE_OK)e="corrupt_audio";
    if(!e)e=control?control(context):NULL;
    if(sqlite3_blob_close(blob)!=SQLITE_OK&&!e)e="audio_store_failed";
    return e;
}
const char *audio_store_publish(const AudioStoreConfig *c,const char *operation,
    const char *request,int packet,const char *(*control)(void *),void *context,AudioReceipt *r) {
    if(!r)return "invalid_configuration";
    memset(r,0,sizeof(*r));if(!name(operation)||!pin(request))return "invalid_request";
    SpeechRuntimeResult native;const char *e=speech_worker_result(packet,control,context,&native);
    Store s={.directory=-1};sqlite3_stmt *q=NULL;int transaction=0;sqlite3_blob *blob=NULL;
    if(!e)e=open_store(c,&s);
    if(!e){if(!sql(&s,"BEGIN IMMEDIATE"))e="audio_store_busy";else transaction=1;}
    sqlite3_int64 row=0;
    if(!e&&!prepare(&s,&q,"SELECT rowid,id,rate,samples,digest,length(pcm),request FROM audio WHERE owner=? AND provider=? AND operation=?"))e="audio_store_failed";
    if(!e) {
        bind(q,1,c->owner);bind(q,2,c->provider);bind(q,3,operation);int rc=sqlite3_step(q);
        if(rc==SQLITE_ROW) {
            const char *old=(const char*)sqlite3_column_text(q,6);
            if(!old||strcmp(old,request))e="idempotency_conflict";
            else e=receipt(q,r);
            if(!e&&(r->sample_rate!=native.sample_rate||r->samples!=native.samples||strcmp(r->pcm_sha256,native.pcm_sha256)))e="audio_identity_conflict";
            row=sqlite3_column_int64(q,0);
        }else if(rc!=SQLITE_DONE)e="audio_store_failed";
    }
    sqlite3_finalize(q);q=NULL;
    if(!e&&row)e=check_blob(&s,row,r,0,0,NULL,control,context);
    if(!e&&!row) {
        if(!prepare(&s,&q,"SELECT count(*),COALESCE(sum(length(pcm)),0) FROM audio"))e="audio_store_failed";
        else if(sqlite3_step(q)!=SQLITE_ROW)e="audio_store_failed";
        else {sqlite3_int64 count=sqlite3_column_int64(q,0),bytes=sqlite3_column_int64(q,1);
            if(count<0||bytes<0)e="corrupt_audio";
            else if(count>=ITEMS_MAX||bytes>(sqlite3_int64)STORE_MAX-(sqlite3_int64)native.samples*2)e="audio_store_limit";}
        sqlite3_finalize(q);q=NULL;
        if(!e) {
            Sha256 h;unsigned char d[32];sha256_init(&h);
            /* Separators are outside allowed host/operation identities. */
            sha256_update(&h,c->owner,strlen(c->owner)+1);sha256_update(&h,c->provider,strlen(c->provider)+1);
            sha256_update(&h,operation,strlen(operation)+1);sha256_update(&h,request,64);sha256_update(&h,native.pcm_sha256,64);
            sha256_final(&h,d);sha256_hex(d,r->id);strcpy(r->pcm_sha256,native.pcm_sha256);
            r->sample_rate=native.sample_rate;r->samples=native.samples;r->bytes=native.samples*2;
            if(!prepare(&s,&q,"INSERT INTO audio(owner,provider,operation,request,id,rate,samples,digest,pcm) VALUES(?,?,?,?,?,?,?,?,zeroblob(?))"))e="audio_store_failed";
        }
        if(!e) {bind(q,1,c->owner);bind(q,2,c->provider);bind(q,3,operation);bind(q,4,request);bind(q,5,r->id);
            sqlite3_bind_int(q,6,r->sample_rate);sqlite3_bind_int64(q,7,(sqlite3_int64)r->samples);bind(q,8,r->pcm_sha256);
            sqlite3_bind_int(q,9,(int)r->bytes);if(sqlite3_step(q)!=SQLITE_DONE)e="audio_store_failed";
            else row=sqlite3_last_insert_rowid(s.db);}
        sqlite3_finalize(q);q=NULL;
        if(!e&&sqlite3_blob_open(s.db,"main","audio","pcm",row,1,&blob)!=SQLITE_OK)e="audio_store_failed";
        unsigned char chunk[65536];
        for(uint64_t at=0;!e&&at<r->bytes;) {
            e=control?control(context):NULL;if(e)break;
            size_t n=(size_t)(r->bytes-at);if(n>sizeof(chunk))n=sizeof(chunk);
            size_t got=0;while(got<n){ssize_t z=pread(packet,chunk+got,n-got,(off_t)(SPEECH_PACKET_HEADER+at+got));
                if(z<0&&errno==EINTR)continue;
                if(z<=0){e="speech_output_changed";break;}got+=(size_t)z;}
            if(!e&&sqlite3_blob_write(blob,chunk,(int)n,(int)at)!=SQLITE_OK)e="audio_store_failed";
            at+=n;
        }
        if(blob){if(sqlite3_blob_close(blob)!=SQLITE_OK&&!e)e="audio_store_failed";blob=NULL;}
        /* A second source check plus stored digest check binds the copied bytes
         * to the completed worker receipt even if a retained fd was modified. */
        SpeechRuntimeResult after;
        if(!e)e=speech_worker_result(packet,control,context,&after);
        if(!e&&(after.samples!=native.samples||after.sample_rate!=native.sample_rate||strcmp(after.pcm_sha256,native.pcm_sha256)))e="speech_output_changed";
        if(!e)e=check_blob(&s,row,r,0,0,NULL,control,context);
    }
    if(!e)e=control?control(context):NULL;
    if(transaction){if(e)sql(&s,"ROLLBACK");else if(!sql(&s,"COMMIT")){sql(&s,"ROLLBACK");e="audio_store_failed";}}
    close_store(&s);if(e)memset(r,0,sizeof(*r));return e;
}
const char *audio_store_read(const AudioStoreConfig *c,const char *id,uint64_t offset,
    size_t length,unsigned char *output,const char *(*control)(void *),void *context,AudioReceipt *r) {
    if(!r)return "invalid_configuration";
    memset(r,0,sizeof(*r));if(length>AUDIO_RANGE_MAX||!output)return "invalid_audio_range";
    memset(output,0,length);if(!pin(id)||offset%2||length%2)return "invalid_audio_range";
    Store s={.directory=-1};sqlite3_stmt *q=NULL;const char *e=control?control(context):NULL;int transaction=0;
    if(!e)e=open_store(c,&s);
    if(!e){if(!sql(&s,"BEGIN"))e="audio_store_busy";else transaction=1;}
    if(!e&&!prepare(&s,&q,"SELECT rowid,id,rate,samples,digest,length(pcm) FROM audio WHERE owner=? AND provider=? AND id=?"))e="audio_store_failed";
    sqlite3_int64 row=0;
    if(!e){bind(q,1,c->owner);bind(q,2,c->provider);bind(q,3,id);int rc=sqlite3_step(q);
        if(rc==SQLITE_ROW){row=sqlite3_column_int64(q,0);e=receipt(q,r);}
        else e=rc==SQLITE_DONE?"unknown_audio":"audio_store_failed";}
    sqlite3_finalize(q);
    if(!e&&(offset>r->bytes||length>r->bytes-offset))e="invalid_audio_range";
    if(!e)e=check_blob(&s,row,r,offset,length,output,control,context);
    if(transaction&&!sql(&s,"ROLLBACK")&&!e)e="audio_store_failed";
    close_store(&s);if(e){memset(r,0,sizeof(*r));memset(output,0,length);}return e;
}
const char *audio_store_delete(const AudioStoreConfig *c,const char *id) {
    if(!pin(id))return "invalid_request";
    Store s={.directory=-1};sqlite3_stmt *q=NULL;const char *e=open_store(c,&s);
    if(!e&&!prepare(&s,&q,"DELETE FROM audio WHERE owner=? AND provider=? AND id=?"))e="audio_store_failed";
    if(!e){bind(q,1,c->owner);bind(q,2,c->provider);bind(q,3,id);
        if(sqlite3_step(q)!=SQLITE_DONE)e="audio_store_failed";
        else if(sqlite3_changes(s.db)!=1)e="unknown_audio";}
    sqlite3_finalize(q);close_store(&s);return e;
}
