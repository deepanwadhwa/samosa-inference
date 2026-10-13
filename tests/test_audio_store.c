#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "audio_store.h"
#include "speech_worker.h"
#include "sha256.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
static int checks;
#define CHECK(x) do {checks++;if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);exit(1);}} while(0)
static void w32(unsigned char *p,uint32_t x){for(int i=0;i<4;i++)p[i]=(unsigned char)(x>>(i*8));}
static void w64(unsigned char *p,uint64_t x){for(int i=0;i<8;i++)p[i]=(unsigned char)(x>>(i*8));}
static int packet(const char *base,size_t bytes) {
    char path[1024];snprintf(path,sizeof(path),"%s/packet",base);
    int fd=open(path,O_CREAT|O_TRUNC|O_RDWR,0600);CHECK(fd>=0);
    unsigned char h[80]={0},chunk[65536];memset(chunk,0x41,sizeof(chunk));
    Sha256 sha;sha256_init(&sha);size_t at=0;
    while(at<bytes){size_t n=bytes-at;if(n>sizeof(chunk))n=sizeof(chunk);
        CHECK(pwrite(fd,chunk,n,(off_t)(80+at))==(ssize_t)n);sha256_update(&sha,chunk,n);at+=n;}
    memcpy(h,"SMPCM001",8);w64(h+8,bytes);w32(h+16,bytes>2160000?96000:24000);w64(h+24,bytes/2);w64(h+32,1);sha256_final(&sha,h+40);
    CHECK(pwrite(fd,h,80,0)==80);CHECK(!fsync(fd));return fd;
}
static void zero(const AudioReceipt *r){AudioReceipt empty={0};CHECK(!memcmp(r,&empty,sizeof(empty)));}
static void error(const char *got,const char *want){CHECK(got&&strcmp(got,want)==0);}
static int rows(const char *state){char path[1024];snprintf(path,sizeof(path),"%s/audio.sqlite3",state);sqlite3 *db=NULL;sqlite3_stmt *q=NULL;
    CHECK(sqlite3_open(path,&db)==SQLITE_OK);CHECK(sqlite3_prepare_v2(db,"SELECT count(*) FROM audio",-1,&q,NULL)==SQLITE_OK);
    CHECK(sqlite3_step(q)==SQLITE_ROW);int n=sqlite3_column_int(q,0);sqlite3_finalize(q);sqlite3_close(db);return n;}
static void change(const char *state,const char *sql){char path[1024];snprintf(path,sizeof(path),"%s/audio.sqlite3",state);sqlite3 *db=NULL;
    CHECK(sqlite3_open(path,&db)==SQLITE_OK);CHECK(sqlite3_exec(db,sql,NULL,NULL,NULL)==SQLITE_OK);sqlite3_close(db);}
typedef struct {const char *state;int notification,kill,hit;} Interrupt;
static const char *interrupt(void *opaque){Interrupt *x=opaque;char path[1024];snprintf(path,sizeof(path),"%s/audio.sqlite3-journal",x->state);
    struct stat s;if(!stat(path,&s)&&s.st_size>0){x->hit=1;
        /* A separate reader sees no uncommitted artifact, even during copying. */
        CHECK(rows(x->state)==0);
        if(x->kill){CHECK(write(x->notification,"x",1)==1);for(;;)pause();}
        return "cancelled";}
    return NULL;}
static const char *cancel(void *unused){(void)unused;return "deadline_exceeded";}
int main(void) {
    char base[]="/private/tmp/samosa-audio-store.XXXXXX";
#ifndef __APPLE__
    strcpy(base,"/tmp/samosa-audio-store.XXXXXX");
#endif
    CHECK(mkdtemp(base)!=NULL);char state[1024];snprintf(state,sizeof(state),"%s/state",base);
    AudioStoreConfig c={state,"alice","speech"};AudioReceipt r,got;char request[65],other[65];hash_bytes("request",7,request);hash_bytes("other",5,other);
    int fd=packet(base,8);CHECK(!audio_store_publish(&c,"op",request,fd,NULL,NULL,&r));CHECK(r.bytes==8&&r.samples==4&&r.sample_rate==24000);
    char id[65];strcpy(id,r.id);unsigned char out[AUDIO_RANGE_MAX];memset(out,1,sizeof(out));
    CHECK(!audio_store_read(&c,id,2,4,out,NULL,NULL,&got));CHECK(!memcmp(out,"AAAA",4));CHECK(!strcmp(got.id,id));
    CHECK(!audio_store_publish(&c,"op",request,fd,NULL,NULL,&got));CHECK(!memcmp(&r,&got,sizeof(r)));
    error(audio_store_publish(&c,"op",other,fd,NULL,NULL,&got),"idempotency_conflict");zero(&got);
    AudioStoreConfig foreign=c;foreign.owner="bob";
    error(audio_store_read(&foreign,id,0,8,out,NULL,NULL,&got),"unknown_audio");zero(&got);CHECK(!memcmp(out,(unsigned char[8]){0},8));
    error(audio_store_delete(&foreign,id),"unknown_audio");
    foreign=c;foreign.provider="other";error(audio_store_read(&foreign,id,0,8,out,NULL,NULL,&got),"unknown_audio");
    CHECK(!audio_store_publish(&foreign,"op",request,fd,NULL,NULL,&got));CHECK(strcmp(got.id,id));
    CHECK(!audio_store_delete(&foreign,got.id));CHECK(rows(state)==1);
    error(audio_store_read(&c,id,1,2,out,NULL,NULL,&got),"invalid_audio_range");
    error(audio_store_read(&c,id,0,3,out,NULL,NULL,&got),"invalid_audio_range");
    error(audio_store_read(&c,id,UINT64_MAX-1,2,out,NULL,NULL,&got),"invalid_audio_range");
    error(audio_store_read(&c,id,0,AUDIO_RANGE_MAX+1,out,NULL,NULL,&got),"invalid_audio_range");
    CHECK(!audio_store_read(&c,id,8,0,out,NULL,NULL,&got));
    error(audio_store_read(&c,id,0,8,out,cancel,NULL,&got),"deadline_exceeded");zero(&got);
    error(audio_store_publish(&c,"cancel",request,fd,cancel,NULL,&got),"deadline_exceeded");zero(&got);CHECK(rows(state)==1);
    CHECK(pwrite(fd,"B",1,80)==1);error(audio_store_publish(&c,"bad",request,fd,NULL,NULL,&got),"speech_output_checksum");zero(&got);
    CHECK(pwrite(fd,"A",1,80)==1);
    change(state,"UPDATE audio SET pcm=x'4141414141414142'");
    error(audio_store_read(&c,id,0,2,out,NULL,NULL,&got),"corrupt_audio");zero(&got);CHECK(out[0]==0&&out[1]==0);
    error(audio_store_publish(&c,"op",request,fd,NULL,NULL,&got),"corrupt_audio");
    CHECK(!audio_store_delete(&c,id));CHECK(rows(state)==0);
    /* Failed transactional publication and killed writers leave no artifact. */
    close(fd);fd=packet(base,1048576);Interrupt x={state,-1,0,0};
    error(audio_store_publish(&c,"partial",request,fd,interrupt,&x,&got),"cancelled");CHECK(x.hit);zero(&got);CHECK(rows(state)==0);
    int pipefd[2];CHECK(!pipe(pipefd));pid_t child=fork();CHECK(child>=0);
    if(!child){close(pipefd[0]);x.notification=pipefd[1];x.kill=1;(void)audio_store_publish(&c,"crash",request,fd,interrupt,&x,&got);_exit(2);}
    close(pipefd[1]);char signalbyte;CHECK(read(pipefd[0],&signalbyte,1)==1);close(pipefd[0]);CHECK(!kill(child,SIGKILL));int status;CHECK(waitpid(child,&status,0)==child&&WIFSIGNALED(status));
    CHECK(rows(state)==0);CHECK(!audio_store_publish(&c,"after-crash",request,fd,NULL,NULL,&r));
    CHECK(!audio_store_read(&c,r.id,65534,AUDIO_RANGE_MAX,out,NULL,NULL,&got));unsigned char expected[AUDIO_RANGE_MAX];memset(expected,'A',sizeof(expected));CHECK(!memcmp(out,expected,sizeof(expected)));
    CHECK(!audio_store_delete(&c,r.id));close(fd);fd=packet(base,8);
    for(int i=0;i<128;i++){char op[32];snprintf(op,sizeof(op),"bounded-%d",i);CHECK(!audio_store_publish(&c,op,request,fd,NULL,NULL,&r));}
    error(audio_store_publish(&c,"overflow",request,fd,NULL,NULL,&got),"audio_store_limit");zero(&got);CHECK(rows(state)==128);
    CHECK(!audio_store_delete(&c,r.id));CHECK(!audio_store_publish(&c,"replacement",request,fd,NULL,NULL,&got));
    change(state,"DELETE FROM audio");close(fd);fd=packet(base,8388608);
    for(int i=0;i<8;i++){char op[32];snprintf(op,sizeof(op),"bytes-%d",i);CHECK(!audio_store_publish(&c,op,request,fd,NULL,NULL,&r));}
    error(audio_store_publish(&c,"bytes-overflow",request,fd,NULL,NULL,&got),"audio_store_limit");zero(&got);CHECK(rows(state)==8);
    CHECK(!audio_store_delete(&c,r.id));CHECK(!audio_store_publish(&c,"bytes-replacement",request,fd,NULL,NULL,&got));
    /* Unsafe state and database aliases are refused before opening SQLite. */
    char unsafe[1024];snprintf(unsafe,sizeof(unsafe),"%s/unsafe",base);CHECK(!symlink(state,unsafe));foreign=c;foreign.state=unsafe;
    error(audio_store_read(&foreign,id,0,2,out,NULL,NULL,&got),"audio_store_unavailable");
    CHECK(!chmod(state,0755));error(audio_store_read(&c,id,0,2,out,NULL,NULL,&got),"audio_store_unavailable");CHECK(!chmod(state,0700));
    char dbpath[1100],alias[1100];snprintf(dbpath,sizeof(dbpath),"%s/audio.sqlite3",state);snprintf(alias,sizeof(alias),"%s/hardlink",base);
    CHECK(!link(dbpath,alias));error(audio_store_read(&c,id,0,2,out,NULL,NULL,&got),"audio_store_unavailable");CHECK(!unlink(alias));
    char pages[1024];snprintf(pages,sizeof(pages),"%s/pages",base);CHECK(!mkdir(pages,0700));
    change(pages,"PRAGMA page_size=8192;CREATE TABLE other(x)");snprintf(dbpath,sizeof(dbpath),"%s/audio.sqlite3",pages);CHECK(!chmod(dbpath,0600));
    foreign=c;foreign.state=pages;error(audio_store_read(&foreign,id,0,2,out,NULL,NULL,&got),"audio_store_unavailable");CHECK(!unlink(dbpath));CHECK(!rmdir(pages));
    close(fd);char path[1024];snprintf(path,sizeof(path),"%s/packet",base);CHECK(!unlink(path));CHECK(!unlink(unsafe));
    snprintf(path,sizeof(path),"%s/audio.sqlite3",state);CHECK(!unlink(path));CHECK(!rmdir(state));CHECK(!rmdir(base));
    printf("PASS owner-scoped audio store: %d checks; atomic publication, integrity, ranges, quotas and crash rollback\n",checks);return 0;
}
