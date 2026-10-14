#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "artifact.h"
#include <errno.h>
#include "sha256.h"
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>

static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void status(ArtifactConfig *c,const char *expected){const char *s=NULL;CHECK(!artifact_status(c,&s));CHECK(!strcmp(s,expected));}
static const char *stop_read(void *p){(void)p;return "cancelled";}
typedef struct {unsigned calls,target;} ReadStop;
static int stop_at(void *p){ReadStop *stop=p;return ++stop->calls==stop->target;}
static int interrupt(void *p){unsigned *calls=p;return ++*calls==2;}
static int death(void *p){unsigned *calls=p;if(++*calls==2)kill(getpid(),SIGKILL);return 0;}
static void publication_death(const char *phase,void *p){(void)p;if(!strcmp(phase,"published"))kill(getpid(),SIGKILL);}
static void fixture(char root[64],char source[128],char state[128],ArtifactConfig *c,char sha[65]){
    strcpy(root,"/tmp/samosa-install-XXXXXX");CHECK(mkdtemp(root));
    snprintf(source,128,"%s/input",root);snprintf(state,128,"%s/state",root);
    int fd=open(source,O_CREAT|O_EXCL|O_WRONLY,0600);CHECK(fd>=0);
    unsigned char data[100000];for(size_t i=0;i<sizeof(data);i++)data[i]=(unsigned char)(i%251);
    CHECK(write(fd,data,sizeof(data))==(ssize_t)sizeof(data));CHECK(!close(fd));hash_bytes(data,sizeof(data),sha);
    *c=(ArtifactConfig){.state_path=state,.source_path=source,.sha256=sha,.size=sizeof(data),.byte_ceiling=sizeof(data),.disk_ceiling=4000000};
}
typedef struct {ArtifactConfig *config;int done;} Replacement;
static int relax_root(void *p){Replacement *r=p;if(++r->done==2)CHECK(!chmod(r->config->state_path,0755));return 0;}
static int mutate_source(void *p){Replacement *r=p;if(++r->done!=2)return 0;
    int fd=open(r->config->source_path,O_WRONLY);CHECK(fd>=0);struct stat st;CHECK(!fstat(fd,&st));
    CHECK(pwrite(fd,"x",1,0)==1);
#ifdef __APPLE__
    struct timespec times[2]={st.st_atimespec,st.st_mtimespec};
#else
    struct timespec times[2]={st.st_atim,st.st_mtim};
#endif
    CHECK(!futimens(fd,times));CHECK(!close(fd));return 0;}
static int replace_root(void *p){Replacement *r=p;if(r->done++)return 0;
    ArtifactConfig *c=r->config;char renamed[256];snprintf(renamed,sizeof(renamed),"%s-old",c->state_path);
    CHECK(!rename(c->state_path,renamed));CHECK(!mkdir(c->state_path,0700));return 0;}
int main(int argc,char **argv){
    (void)argc;CHECK(argv[0][0]=='/');
    char root[64],source[128],state[128],sha[65];ArtifactConfig c;
    fixture(root,source,state,&c,sha);status(&c,"absent");CHECK(!artifact_install(&c));status(&c,"verified");
    int verified=-7;CHECK(!artifact_open_verified(&c,NULL,NULL,&verified));CHECK(verified>=0);
    CHECK((fcntl(verified,F_GETFL)&O_ACCMODE)==O_RDONLY);CHECK(!artifact_verify(verified,c.size,c.byte_ceiling,c.sha256));CHECK(!close(verified));
    verified=-7;CHECK(!strcmp(artifact_open_verified(&c,stop_read,NULL,&verified),"cancelled"));CHECK(verified==-7);
    const char *executable_error=artifact_make_executable(&c);CHECK(executable_error&&!strcmp(executable_error,"unsupported_executable"));
    CHECK(!unlink(source));CHECK(!artifact_install(&c)); /* exact installed retry is offline */
    char ready[256];snprintf(ready,sizeof(ready),"%s/%s/artifact",state,sha);
    int fd=open(ready,O_WRONLY);CHECK(fd>=0);CHECK(pwrite(fd,"x",1,0)==1);CHECK(!close(fd));
    verified=-7;CHECK(!strcmp(artifact_open_verified(&c,NULL,NULL,&verified),"artifact_checksum_mismatch"));CHECK(verified==-7);
    const char *s;CHECK(!strcmp(artifact_status(&c,&s),"artifact_checksum_mismatch"));CHECK(!strcmp(s,"invalid"));
    /* Four local-copy chunks finish, then cancellation on the second bounded
     * hash read prevents publication of an otherwise complete staged artifact. */
    fixture(root,source,state,&c,sha);ReadStop stop={.target=6};c.cancelled=stop_at;c.cancellation=&stop;
    CHECK(!strcmp(artifact_install(&c),"artifact_interrupted"));CHECK(stop.calls==6);status(&c,"interrupted");
    c.cancelled=NULL;c.resume=1;CHECK(!artifact_install(&c));status(&c,"verified");
    CHECK(!unlink(source));stop=(ReadStop){.target=2};c.cancelled=stop_at;
    CHECK(!strcmp(artifact_install(&c),"artifact_interrupted"));CHECK(stop.calls==2);
    /* Status remains queryable after a cancelled retry; it does not reuse the
     * installation cancellation hook or change completion state. */
    status(&c,"verified");c.cancelled=NULL;CHECK(!artifact_install(&c));
    fixture(root,source,state,&c,sha);unsigned calls=0;c.cancelled=interrupt;c.cancellation=&calls;
    CHECK(!strcmp(artifact_install(&c),"artifact_interrupted"));status(&c,"interrupted");
    c.cancelled=NULL;CHECK(!strcmp(artifact_install(&c),"artifact_resume_required"));c.resume=1;
    CHECK(!artifact_install(&c));status(&c,"verified");
    fixture(root,source,state,&c,sha);calls=0;c.cancelled=death;c.cancellation=&calls;
    pid_t pid=fork();CHECK(pid>=0);if(!pid){artifact_install(&c);_exit(3);}int wait_status;
    CHECK(waitpid(pid,&wait_status,0)==pid);CHECK(WIFSIGNALED(wait_status)&&WTERMSIG(wait_status)==SIGKILL);
    status(&c,"interrupted");c.cancelled=NULL;c.resume=1;CHECK(!artifact_install(&c));status(&c,"verified");
    fixture(root,source,state,&c,sha);calls=0;c.cancelled=interrupt;c.cancellation=&calls;
    CHECK(!strcmp(artifact_install(&c),"artifact_interrupted"));c.cancelled=NULL;c.resume=1;
    fd=open(source,O_WRONLY);CHECK(fd>=0);CHECK(pwrite(fd,"x",1,0)==1);CHECK(!close(fd));
    CHECK(!strcmp(artifact_install(&c),"artifact_resume_conflict"));status(&c,"interrupted");
    fixture(root,source,state,&c,sha);char wrong[65];memset(wrong,'0',64);wrong[64]=0;c.sha256=wrong;
    CHECK(!strcmp(artifact_install(&c),"artifact_checksum_mismatch"));status(&c,"interrupted");
    fixture(root,source,state,&c,sha);CHECK(!truncate(source,99999));
    CHECK(!strcmp(artifact_install(&c),"artifact_size_mismatch"));status(&c,"interrupted");
    fixture(root,source,state,&c,sha);CHECK(!artifact_install(&c));
    char lock[256];snprintf(lock,sizeof(lock),"%s/install.lock",state);fd=open(lock,O_RDWR);CHECK(fd>=0);CHECK(!flock(fd,LOCK_EX|LOCK_NB));
    CHECK(!strcmp(artifact_install(&c),"artifact_busy"));status(&c,"installing");CHECK(!close(fd));
    fixture(root,source,state,&c,sha);calls=0;c.cancelled=interrupt;c.cancellation=&calls;
    CHECK(!strcmp(artifact_install(&c),"artifact_interrupted"));c.cancelled=NULL;c.resume=1;
    snprintf(ready,sizeof(ready),"%s/%s/artifact",state,sha);fd=open(ready,O_WRONLY|O_CREAT|O_EXCL,0600);CHECK(fd>=0);CHECK(!close(fd));
    CHECK(!strcmp(artifact_install(&c),"foreign_artifact"));struct stat st;CHECK(!stat(ready,&st)&&st.st_size==0);
    fixture(root,source,state,&c,sha);Replacement replacement={.config=&c};c.cancelled=replace_root;c.cancellation=&replacement;
    CHECK(!strcmp(artifact_install(&c),"artifact_state_changed"));status(&c,"absent");
    fixture(root,source,state,&c,sha);Replacement mutation={.config=&c};c.cancelled=mutate_source;c.cancellation=&mutation;
    CHECK(!strcmp(artifact_install(&c),"artifact_source_changed"));status(&c,"interrupted");
    fixture(root,source,state,&c,sha);Replacement permissions={.config=&c};c.cancelled=relax_root;c.cancellation=&permissions;
    CHECK(!strcmp(artifact_install(&c),"artifact_state_changed"));snprintf(ready,sizeof(ready),"%s/%s/artifact",state,sha);
    CHECK(access(ready,F_OK)<0&&errno==ENOENT);CHECK(!chmod(state,0700));c.cancelled=NULL;c.resume=1;CHECK(!artifact_install(&c));status(&c,"verified");
    char alias[256];snprintf(alias,sizeof(alias),"%s/external-alias",root);CHECK(!link(ready,alias));
    CHECK(!strcmp(artifact_install(&c),"foreign_artifact"));CHECK(!unlink(alias));status(&c,"verified");
    fixture(root,source,state,&c,sha);c.disk_ceiling=c.size*2-1;
    CHECK(!strcmp(artifact_install(&c),"invalid_artifact_configuration"));
    fixture(root,source,state,&c,sha);c.checkpoint=publication_death;
    pid=fork();CHECK(pid>=0);if(!pid){artifact_install(&c);_exit(3);}
    CHECK(waitpid(pid,&wait_status,0)==pid);CHECK(WIFSIGNALED(wait_status)&&WTERMSIG(wait_status)==SIGKILL);
    c.checkpoint=NULL;status(&c,"verified");CHECK(!unlink(source));CHECK(!artifact_install(&c));
    fixture(root,source,state,&c,sha);char moved[256];snprintf(moved,sizeof(moved),"%s/original",root);
    CHECK(!rename(source,moved));CHECK(!symlink(moved,source));
    CHECK(!strcmp(artifact_install(&c),"artifact_source_unavailable"));
    CHECK(!unlink(source));CHECK(!mkfifo(source,0600));
    CHECK(!strcmp(artifact_install(&c),"unsupported_artifact"));
    fixture(root,source,state,&c,sha);CHECK(!mkdir(state,0755));
    CHECK(!strcmp(artifact_install(&c),"unsafe_artifact_state"));
    fixture(root,source,state,&c,sha);CHECK(!artifact_install(&c));c.disk_ceiling=200000;
    char second[65];strcpy(second,sha);second[0]=second[0]=='0'?'1':'0';c.sha256=second;
    CHECK(!strcmp(artifact_install(&c),"artifact_quota"));
    fixture(root,source,state,&c,sha);unsigned char data[100000];for(size_t i=0;i<sizeof(data);i++)data[i]=(unsigned char)(i%251);
    for(unsigned i=0;i<17;i++){
        data[0]=(unsigned char)i;hash_bytes(data,sizeof(data),sha);
        fd=open(source,O_WRONLY);CHECK(fd>=0);CHECK(pwrite(fd,data,1,0)==1);CHECK(!close(fd));
        const char *e=artifact_install(&c);if(i<16)CHECK(!e);else CHECK(e&&!strcmp(e,"artifact_quota"));
    }
    /* A maximal512-file package needs another identity for its pinned index.
     * Exercise actual acquisition/publication of513 distinct payloads, retaining
     * the old default limit and independent byte-quota enforcement. */
    fixture(root,source,state,&c,sha);c.artifact_capacity=513;c.size=c.byte_ceiling=3;c.disk_ceiling=3084;
    char first_sha[65];
    for(unsigned i=0;i<514;i++){
        unsigned char small[]={(unsigned char)i,(unsigned char)(i>>8),'x'};
        hash_bytes(small,sizeof(small),sha);if(!i)strcpy(first_sha,sha);
        fd=open(source,O_WRONLY|O_TRUNC);CHECK(fd>=0);CHECK(write(fd,small,sizeof(small))==sizeof(small));CHECK(!close(fd));
        const char *e=artifact_install(&c);if(i<513)CHECK(!e);else CHECK(e&&!strcmp(e,"artifact_quota"));
    }
    strcpy(sha,first_sha);status(&c,"verified");
    c.artifact_capacity=0;status(&c,"verified");CHECK(!strcmp(artifact_install(&c),"artifact_quota"));
    c.artifact_capacity=1024;c.disk_ceiling=3076;CHECK(!strcmp(artifact_install(&c),"artifact_quota"));
    c.disk_ceiling=3084;CHECK(!artifact_install(&c));
    fixture(root,source,state,&c,sha);c.artifact_capacity=ARTIFACT_CAPACITY_MAX+1u;
    CHECK(!strcmp(artifact_install(&c),"invalid_artifact_configuration"));CHECK(access(state,F_OK)<0&&errno==ENOENT);
    fixture(root,source,state,&c,sha);fd=open(argv[0],O_RDONLY);CHECK(fd>=0);struct stat executable_before;CHECK(!fstat(fd,&executable_before));
    Sha256 hash;sha256_init(&hash);unsigned char block[65536];
    for(;;){ssize_t n=read(fd,block,sizeof(block));CHECK(n>=0);if(!n)break;sha256_update(&hash,block,(size_t)n);}
    CHECK(!close(fd));unsigned char digest[32];sha256_final(&hash,digest);sha256_hex(digest,sha);
    c.source_path=argv[0];c.size=(uint64_t)executable_before.st_size;c.byte_ceiling=c.size;c.disk_ceiling=4*c.size;
    CHECK(!artifact_install(&c));CHECK(!artifact_make_executable(&c));status(&c,"verified");CHECK(!artifact_install(&c));
    snprintf(ready,sizeof(ready),"%s/%s/artifact",state,sha);CHECK(!stat(ready,&st));CHECK((st.st_mode&0777)==0500);
    CHECK(!stat(argv[0],&st));CHECK(st.st_mode==executable_before.st_mode);
    /* Refuse a pinned executable for the other supported architecture before
     * changing permissions; the actual host executable above remains the
     * positive gate, not a synthetic header accepted as a runnable engine. */
    unsigned char foreign[64];fd=open(argv[0],O_RDONLY);CHECK(fd>=0);
    CHECK(read(fd,foreign,sizeof(foreign))==sizeof(foreign));CHECK(!close(fd));
#if defined(__APPLE__) && defined(__aarch64__)
    foreign[4]=7;
#elif defined(__APPLE__) && defined(__x86_64__)
    foreign[4]=12;
#elif defined(__linux__) && defined(__aarch64__)
    foreign[18]=62;
#elif defined(__linux__) && defined(__x86_64__)
    foreign[18]=183;
#endif
    fixture(root,source,state,&c,sha);fd=open(source,O_WRONLY|O_TRUNC);CHECK(fd>=0);
    CHECK(write(fd,foreign,sizeof(foreign))==sizeof(foreign));CHECK(!close(fd));hash_bytes(foreign,sizeof(foreign),sha);
    c.size=sizeof(foreign);CHECK(!artifact_install(&c));CHECK(!strcmp(artifact_make_executable(&c),"unsupported_executable"));
    snprintf(ready,sizeof(ready),"%s/%s/artifact",state,sha);CHECK(!stat(ready,&st));CHECK((st.st_mode&0777)==0600);
    printf("PASS native install: %u checks; verified/offline retry, cancel/death/resume, foreign/root/quota refusal\n",checks);
    return 0;
}
