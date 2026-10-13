#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "remote_artifact.h"
#include "sha256.h"
#include <curl/curl.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
typedef struct {
    const RemoteArtifactSource *source;const ArtifactConfig *config;
    int stage;uint64_t retained,offset,content_length;size_t headers,redirect_bytes;long status;
    int ranged,range_seen,length_seen;
    int checkpoint,entry;struct stat checkpoint_identity;uint64_t committed;unsigned checkpoint_slot;
    Sha256 hash,prefix_hash;
    char location[4096];unsigned char compare[32768];const char *error;
} Transfer;
static double clock_now(void){struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t))return -1;return (double)t.tv_sec+(double)t.tv_nsec/1e9;}
static int checkpoint_scope(Transfer *t,int entry){
    struct stat held,named;return !fstat(t->checkpoint,&held)&&S_ISREG(held.st_mode)&&held.st_uid==geteuid()&&!(held.st_mode&077)&&held.st_nlink==1&&held.st_size==288&&
        held.st_dev==t->checkpoint_identity.st_dev&&held.st_ino==t->checkpoint_identity.st_ino&&
        !fstatat(entry,"remote-checkpoint",&named,AT_SYMLINK_NOFOLLOW)&&named.st_dev==held.st_dev&&named.st_ino==held.st_ino;
}
static const char *checkpoint_save(Transfer *t,int entry){
    if(!checkpoint_scope(t,entry))return "unsafe_artifact_checkpoint";
    char record[145],digest[65];unsigned char raw[32];Sha256 hash=t->hash;
    sha256_final(&hash,raw);sha256_hex(raw,digest);
    snprintf(record,81,"%016llx%s",(unsigned long long)t->offset,digest);hash_bytes(record,80,digest);memcpy(record+80,digest,64);
    if(fsync(t->stage))return "artifact_checkpoint_failed";
    size_t written=0;while(written<144){ssize_t n=pwrite(t->checkpoint,record+written,144-written,(off_t)(t->checkpoint_slot*144+written));
        if(n<0&&errno==EINTR)continue;
        if(n<=0)return "artifact_checkpoint_failed";
        written+=(size_t)n;}
    if(fsync(t->checkpoint)||!checkpoint_scope(t,entry))return "artifact_checkpoint_failed";
    t->committed=t->offset;t->checkpoint_slot^=1;return NULL;
}
static const char *checkpoint_open(Transfer *t,int entry){
    t->checkpoint=openat(entry,"remote-checkpoint",O_RDWR|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);int created=0;
    if(t->checkpoint<0&&errno==ENOENT){t->checkpoint=openat(entry,"remote-checkpoint",O_RDWR|O_CREAT|O_EXCL|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC,0600);created=1;}
    if(t->checkpoint<0)return "unsafe_artifact_checkpoint";
    if(fstat(t->checkpoint,&t->checkpoint_identity)||!S_ISREG(t->checkpoint_identity.st_mode)||t->checkpoint_identity.st_uid!=geteuid()||
        (t->checkpoint_identity.st_mode&077)||t->checkpoint_identity.st_nlink!=1)return "unsafe_artifact_checkpoint";
    sha256_init(&t->hash);sha256_init(&t->prefix_hash);
    if(created){
        if(ftruncate(t->checkpoint,288))return "artifact_checkpoint_failed";
        const char *e=checkpoint_save(t,entry);if(e)return e;
        if(fsync(entry))return "artifact_checkpoint_failed";
        /* Old full-replay stages have no durable prefix proof: replay once. */
        t->ranged=0;return NULL;
    }
    if(!checkpoint_scope(t,entry))return "unsafe_artifact_checkpoint";
    char records[288],selected[65];if(pread(t->checkpoint,records,sizeof(records),0)!=(ssize_t)sizeof(records))return "artifact_checkpoint_failed";
    int valid=0;for(unsigned slot=0;slot<2;slot++){
        char *record=records+144*slot,digest[65];hash_bytes(record,80,digest);if(memcmp(record+80,digest,64))continue;
        uint64_t bytes=0;int good=1;for(unsigned i=0;i<16;i++){unsigned char c=(unsigned char)record[i];unsigned v;
            if(c>='0'&&c<='9')v=c-'0';else if(c>='a'&&c<='f')v=c-'a'+10;else {good=0;break;}bytes=(bytes<<4)|v;}
        if(!good||bytes>t->retained)return "artifact_checkpoint_mismatch";
        if(!valid||bytes>t->committed){t->committed=bytes;memcpy(selected,record+16,64);t->checkpoint_slot=slot^1;}valid=1;
    }
    if(!valid)return "artifact_checkpoint_mismatch";
    double started=clock_now();if(started<0)return "artifact_clock_failed";
    uint64_t offset=0;while(offset<t->committed){
        if(t->config->cancelled&&t->config->cancelled(t->config->cancellation))return "artifact_interrupted";
        double now=clock_now();if(now<0)return "artifact_clock_failed";if(now-started>600)return "artifact_deadline_exceeded";
        size_t n=sizeof(t->compare);if(t->committed-offset<n)n=(size_t)(t->committed-offset);
        ssize_t got=pread(t->stage,t->compare,n,(off_t)offset);if(got<0&&errno==EINTR)continue;
        if(got<=0)return "artifact_checkpoint_mismatch";
        sha256_update(&t->prefix_hash,t->compare,(size_t)got);offset+=(size_t)got;
    }
    Sha256 hash=t->prefix_hash;unsigned char raw[32];char digest[65];sha256_final(&hash,raw);sha256_hex(raw,digest);
    if(memcmp(selected,digest,64))return "artifact_checkpoint_mismatch";
    t->hash=t->prefix_hash;t->ranged=t->committed>0;return NULL;
}
static int url_parts(const RemoteArtifactSource *s,const char *url,char host[256],char origin[512]){
    if(!url||strlen(url)>4095)return 0;
    CURLU *u=curl_url();if(!u)return 0;char *scheme=NULL,*name=NULL,*port=NULL,*extra=NULL;int valid=0;
    if(curl_url_set(u,CURLUPART_URL,url,0)||curl_url_get(u,CURLUPART_SCHEME,&scheme,0)||curl_url_get(u,CURLUPART_HOST,&name,0)||strlen(name)>255)goto done;
    if(!strcmp(scheme,"https"))valid=1;
#ifdef SAMOSA_INFERENCE_TEST_FAULTS
    if(s->loopback_http&&!strcmp(scheme,"http")&&!strcmp(name,"127.0.0.1"))valid=1;
#else
    (void)s;
#endif
    if(!valid)goto done;
    if(curl_url_get(u,CURLUPART_USER,&extra,0)==CURLUE_OK){valid=0;goto done;}
    if(curl_url_get(u,CURLUPART_FRAGMENT,&extra,0)==CURLUE_OK){valid=0;goto done;}
    if(curl_url_get(u,CURLUPART_PORT,&port,CURLU_DEFAULT_PORT)){valid=0;goto done;}
    strcpy(host,name);if(snprintf(origin,512,"%s://%s:%s",scheme,name,port)>=512)valid=0;
done:
    curl_free(scheme);curl_free(name);curl_free(port);curl_free(extra);curl_url_cleanup(u);return valid;
}
static const char *source_record(int entry,uint64_t retained,const RemoteArtifactSource *s){
    char digest[65];hash_bytes(s->url,strlen(s->url),digest);
    int fd=openat(entry,"remote-source",O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
    if(fd<0&&errno==ENOENT){if(retained)return "artifact_source_identity_missing";
        fd=openat(entry,"remote-source",O_RDWR|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
        if(fd<0)return "unsafe_artifact_source_record";
        int ok=write(fd,digest,64)==64&&!fsync(fd)&&!fsync(entry);close(fd);return ok?NULL:"artifact_source_record_failed";}
    struct stat st,named;char saved[64];int valid=fd>=0&&!fstat(fd,&st)&&S_ISREG(st.st_mode)&&st.st_uid==geteuid()&&!(st.st_mode&077)&&st.st_nlink==1&&st.st_size==64&&
        !fstatat(entry,"remote-source",&named,AT_SYMLINK_NOFOLLOW)&&st.st_dev==named.st_dev&&st.st_ino==named.st_ino&&pread(fd,saved,64,0)==64;
    if(fd>=0)close(fd);
    if(!valid)return "unsafe_artifact_source_record";
    return memcmp(saved,digest,64)?"artifact_source_changed":NULL;
}
static int decimal(const char **cursor,const char *end,uint64_t *value){
    const char *p=*cursor;uint64_t v=0;int digits=0;
    while(p<end&&isdigit((unsigned char)*p)){unsigned d=(unsigned)(*p-'0');if(v>(UINT64_MAX-d)/10)return 0;v=v*10+d;p++;digits=1;}
    if(!digits)return 0;
    *cursor=p;*value=v;return 1;
}
static size_t header(char *bytes,size_t size,size_t count,void *opaque){
    Transfer *t=opaque;if(size&&count>SIZE_MAX/size)return 0;size_t n=size*count;
    if(n>4096||n>16384-t->headers){t->error="artifact_header_limit";return 0;}t->headers+=n;
    if(n>=5&&!memcmp(bytes,"HTTP/",5)){char line[4097];memcpy(line,bytes,n);line[n]=0;char *space=strchr(line,' ');t->status=space?strtol(space+1,NULL,10):0;
        t->location[0]=0;t->range_seen=0;t->length_seen=0;t->offset=0;return n;}
    if((n==2&&bytes[0]=='\r'&&bytes[1]=='\n')||(n==1&&bytes[0]=='\n')){
        if(t->status==206){
            if(!t->ranged||!t->range_seen){t->error="artifact_range_mismatch";return 0;}
            t->offset=t->committed;t->hash=t->prefix_hash;
        }
        else if(t->status==200)sha256_init(&t->hash);
        if(t->length_seen&&(t->status==200||t->status==206)&&t->content_length!=t->config->size-t->offset){t->error="artifact_size_mismatch";return 0;}
        return n;
    }
    if(n>=14&&!strncasecmp(bytes,"Content-Range:",14)){
        const char *p=bytes+14,*end=bytes+n;uint64_t first,last,total;
        while(p<end&&isspace((unsigned char)*p))p++;
        if(t->range_seen++||end-p<6||memcmp(p,"bytes ",6)){t->error="artifact_range_mismatch";return 0;}p+=6;
        if(!decimal(&p,end,&first)||p==end||*p++!='-'||!decimal(&p,end,&last)||p==end||*p++!='/'||!decimal(&p,end,&total)){
            t->error="artifact_range_mismatch";return 0;}
        while(p<end&&isspace((unsigned char)*p))p++;
        if(p!=end||!t->ranged||t->status!=206||first!=t->committed||total!=t->config->size||last!=total-1){t->error="artifact_range_mismatch";return 0;}
    }
    if(n>=9&&!strncasecmp(bytes,"Location:",9)){size_t first=9,last=n;while(first<last&&isspace((unsigned char)bytes[first]))first++;
        while(last>first&&isspace((unsigned char)bytes[last-1]))last--;
        if(last-first>=sizeof(t->location)){t->error="artifact_redirect_limit";return 0;}
        memcpy(t->location,bytes+first,last-first);t->location[last-first]=0;}
    if(n>=17&&!strncasecmp(bytes,"Content-Encoding:",17)){size_t first=17,last=n;while(first<last&&isspace((unsigned char)bytes[first]))first++;
        while(last>first&&isspace((unsigned char)bytes[last-1]))last--;
        if(last-first!=8||strncasecmp(bytes+first,"identity",8)){t->error="unsupported_artifact_encoding";return 0;}}
    if(n>=15&&!strncasecmp(bytes,"Content-Length:",15)&&(t->status==200||t->status==206)){size_t first=15;while(first<n&&isspace((unsigned char)bytes[first]))first++;
        uint64_t length=0;int digits=0;for(;first<n&&isdigit((unsigned char)bytes[first]);first++){unsigned d=(unsigned)(bytes[first]-'0');
            if(length>(UINT64_MAX-d)/10){t->error="artifact_size_mismatch";return 0;}length=length*10+d;digits=1;}
        while(first<n&&isspace((unsigned char)bytes[first]))first++;
        if(!digits||first!=n||t->length_seen++){t->error="artifact_size_mismatch";return 0;}t->content_length=length;}
    return n;
}
static size_t receive(char *bytes,size_t size,size_t count,void *opaque){
    Transfer *t=opaque;if(size&&count>SIZE_MAX/size)return 0;size_t n=size*count;
    if(t->status>=300&&t->status<400){if(n>32768-t->redirect_bytes){t->error="artifact_redirect_limit";return 0;}t->redirect_bytes+=n;return n;}
    if(t->status!=200&&t->status!=206){t->error="artifact_http_error";return 0;}
    if(t->status==206&&(!t->ranged||!t->range_seen)){t->error="artifact_range_mismatch";return 0;}
    if(n>t->config->size-t->offset){t->error="artifact_size_mismatch";return 0;}
    size_t used=0;while(used<n){size_t part=n-used;if(part>sizeof(t->compare))part=sizeof(t->compare);
        if(t->config->cancelled&&t->config->cancelled(t->config->cancellation)){t->error="artifact_interrupted";return 0;}
        size_t old=0;if(t->offset<t->retained){old=(size_t)(t->retained-t->offset);if(old>part)old=part;
            if(pread(t->stage,t->compare,old,(off_t)t->offset)!=(ssize_t)old||memcmp(t->compare,bytes+used,old)){t->error="artifact_resume_conflict";return 0;}}
        size_t written=old;while(written<part){ssize_t result=pwrite(t->stage,bytes+used+written,part-written,(off_t)(t->offset+written));
            if(result<0&&errno==EINTR)continue;
            if(result<=0){t->error="artifact_write_failed";return 0;}written+=(size_t)result;}
        sha256_update(&t->hash,bytes+used,part);t->offset+=part;used+=part;
        uint64_t interval=1048576;
#ifdef SAMOSA_INFERENCE_TEST_FAULTS
        if(t->source->checkpoint_bytes)interval=t->source->checkpoint_bytes;
#endif
        if(t->offset>t->committed&&t->offset-t->committed>=interval){t->error=checkpoint_save(t,t->entry);if(t->error)return 0;}
    }
    return n;
}
static int progress(void *opaque,curl_off_t a,curl_off_t b,curl_off_t c,curl_off_t d){
    (void)a;(void)b;(void)c;(void)d;Transfer *t=opaque;
    if(t->config->cancelled&&t->config->cancelled(t->config->cancellation)){t->error="artifact_interrupted";return 1;}return 0;
}
static const char *acquire(void *opaque,int entry,int stage,uint64_t retained,const ArtifactConfig *c){
    const RemoteArtifactSource *s=opaque;const char *e=source_record(entry,retained,s);if(e)return e;
    if(retained==c->size)return NULL;
    Transfer *t=calloc(1,sizeof(*t));if(!t)return "artifact_resource_limit";
    t->source=s;t->config=c;t->stage=stage;t->retained=retained;t->checkpoint=-1;t->entry=entry;
    e=checkpoint_open(t,entry);if(e){if(t->checkpoint>=0)close(t->checkpoint);free(t);return e;}
    char url[4096],initial_host[256],initial_origin[512];strcpy(url,s->url);(void)url_parts(s,url,initial_host,initial_origin);
    double started=clock_now();if(started<0){close(t->checkpoint);free(t);return "artifact_clock_failed";}
    size_t timeout=s->timeout_ms?s->timeout_ms:600000;
    for(unsigned redirect=0;redirect<=5;redirect++){
        char host[256],origin[512];if(!url_parts(s,url,host,origin)){e="unsupported_artifact_url";break;}
        int allowed=!strcasecmp(host,initial_host);for(size_t i=0;i<s->redirect_count;i++)if(!strcasecmp(host,s->redirect_hosts[i]))allowed=1;
        if(!allowed){e="artifact_redirect_refused";break;}
        double now=clock_now();if(now<0){e="artifact_clock_failed";break;}double remaining=(double)timeout-(now-started)*1000;
        if(remaining<1){e="artifact_deadline_exceeded";break;}
        CURL *curl=curl_easy_init();if(!curl){e="artifact_resource_limit";break;}struct curl_slist *headers=NULL;
        if(s->bearer_key&&!strcasecmp(origin,initial_origin)){char authorization[192];snprintf(authorization,sizeof(authorization),"Authorization: Bearer %s",s->bearer_key);
            headers=curl_slist_append(NULL,authorization);if(!headers){curl_easy_cleanup(curl);e="artifact_resource_limit";break;}}
        t->headers=0;t->status=0;t->location[0]=0;t->offset=0;t->redirect_bytes=0;t->error=NULL;
        curl_easy_setopt(curl,CURLOPT_URL,url);curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headers);curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,0L);
        char range[32];if(t->ranged){snprintf(range,sizeof(range),"%llu-",(unsigned long long)t->committed);curl_easy_setopt(curl,CURLOPT_RANGE,range);}
        curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,1L);curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,2L);if(s->ca_file)curl_easy_setopt(curl,CURLOPT_CAINFO,s->ca_file);
        curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT_MS,remaining<2000?(long)remaining:2000L);curl_easy_setopt(curl,CURLOPT_TIMEOUT_MS,(long)remaining);
        curl_easy_setopt(curl,CURLOPT_NOSIGNAL,1L);curl_easy_setopt(curl,CURLOPT_PROXY,"");curl_easy_setopt(curl,CURLOPT_BUFFERSIZE,32768L);
        curl_easy_setopt(curl,CURLOPT_HEADERFUNCTION,header);curl_easy_setopt(curl,CURLOPT_HEADERDATA,t);curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,receive);curl_easy_setopt(curl,CURLOPT_WRITEDATA,t);
        curl_easy_setopt(curl,CURLOPT_NOPROGRESS,0L);curl_easy_setopt(curl,CURLOPT_XFERINFOFUNCTION,progress);curl_easy_setopt(curl,CURLOPT_XFERINFODATA,t);
        CURLcode rc=curl_easy_perform(curl);curl_easy_cleanup(curl);curl_slist_free_all(headers);
        if((t->status==200||t->status==206)&&t->offset>t->committed){const char *saved=checkpoint_save(t,entry);if(saved){e=saved;break;}}
        if(t->error){e=t->error;break;}
        if(rc!=CURLE_OK){e=rc==CURLE_OPERATION_TIMEDOUT?"artifact_deadline_exceeded":rc==CURLE_PEER_FAILED_VERIFICATION?"artifact_tls_verification_failed":"artifact_transfer_failed";break;}
        if(t->status>=300&&t->status<400){if(!t->location[0]||redirect==5){e="artifact_redirect_limit";break;}
            CURLU *u=curl_url();char *next=NULL;int ok=u&&!curl_url_set(u,CURLUPART_URL,url,0)&&!curl_url_set(u,CURLUPART_URL,t->location,0)&&!curl_url_get(u,CURLUPART_URL,&next,0)&&strlen(next)<sizeof(url);
            if(ok)strcpy(url,next);
            curl_free(next);if(u)curl_url_cleanup(u);if(!ok){e="artifact_redirect_refused";break;}continue;}
        if(t->status!=200&&t->status!=206)e="artifact_http_error";else if(t->offset!=c->size)e="artifact_size_mismatch";break;
    }
    close(t->checkpoint);free(t);return e;
}
const char *artifact_install_remote(const ArtifactConfig *c,const RemoteArtifactSource *s){
    char host[256],origin[512];if(!c||!s||s->redirect_count>8||s->timeout_ms>600000||!url_parts(s,s->url,host,origin))return "invalid_remote_artifact_configuration";
    for(size_t i=0;i<s->redirect_count;i++)if(!s->redirect_hosts[i]||!s->redirect_hosts[i][0]||strlen(s->redirect_hosts[i])>255)return "invalid_remote_artifact_configuration";
    if(s->bearer_key){size_t n=strlen(s->bearer_key);if(!n||n>128)return "invalid_remote_artifact_configuration";
        for(size_t i=0;i<n;i++)if(!isalnum((unsigned char)s->bearer_key[i])&&!strchr("-_.",s->bearer_key[i]))return "invalid_remote_artifact_configuration";}
    ArtifactConfig configured=*c;configured.acquire=acquire;configured.acquisition=(void*)s;return artifact_install(&configured);
}
