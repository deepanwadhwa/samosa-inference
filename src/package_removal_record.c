#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "package_removal_record.h"
#include "sha256.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static void identity(const struct stat *s,PackageRemovalIdentity *out){
    uint64_t *v=out->value;v[0]=(uint64_t)s->st_dev;v[1]=(uint64_t)s->st_ino;v[2]=(uint64_t)s->st_uid;v[3]=(uint64_t)s->st_mode;v[4]=(uint64_t)s->st_nlink;v[5]=(uint64_t)s->st_size;
#ifdef __APPLE__
    v[6]=(uint64_t)s->st_mtimespec.tv_sec;v[7]=(uint64_t)s->st_mtimespec.tv_nsec;v[8]=(uint64_t)s->st_ctimespec.tv_sec;v[9]=(uint64_t)s->st_ctimespec.tv_nsec;
#else
    v[6]=(uint64_t)s->st_mtim.tv_sec;v[7]=(uint64_t)s->st_mtim.tv_nsec;v[8]=(uint64_t)s->st_ctim.tv_sec;v[9]=(uint64_t)s->st_ctim.tv_nsec;
#endif
}
static int node(const PackageRemovalIdentity *id,int directory){const uint64_t *v=id->value;
    return v[1]&&v[2]==(uint64_t)geteuid()&&v[3]<=UINT32_MAX&&!(v[3]&077)&&v[4]&&v[7]<1000000000&&v[9]<1000000000&&
        (directory?S_ISDIR((mode_t)v[3]):S_ISREG((mode_t)v[3])&&v[4]==1);
}
static int parent(const PackageIndex *index,const char *name){size_t n=strlen(name);
    for(size_t i=0;i<index->count;i++)if(strlen(index->files[i].name)>n&&!memcmp(index->files[i].name,name,n)&&index->files[i].name[n]=='/')return 1;return 0;
}
static int shape(const PackageIndex *index,const PackageRemovalRecord *r){
    if(!index||!r||!index->count||index->count>PACKAGE_INDEX_FILES||!memchr(index->manifest_sha256,0,65)||strlen(index->manifest_sha256)!=64||strspn(index->manifest_sha256,"0123456789abcdef")!=64||
        !memchr(r->index_sha256,0,65)||strcmp(index->manifest_sha256,r->index_sha256)||r->file_count!=index->count||r->directory_count>PACKAGE_TREE_DIRECTORIES||
        !node(&r->package,1)||!node(&r->payload,1)||!node(&r->completion,0)||r->package.value[0]!=r->payload.value[0]||r->package.value[1]==r->payload.value[1])return 0;
    for(size_t i=0;i<index->count;i++){
        if(!memchr(index->files[i].name,0,256)||!package_file_name_valid(index->files[i].name,index->files[i].role)||!node(&r->files[i],0)||
            r->files[i].value[0]!=r->payload.value[0]||r->files[i].value[5]!=index->files[i].bytes)return 0;
        for(size_t k=0;k<i;k++)if(r->files[k].value[1]==r->files[i].value[1])return 0;
    }
    for(size_t i=0;i<r->directory_count;i++){
        if(!memchr(r->directories[i].name,0,256)||!r->directories[i].name[0]||!parent(index,r->directories[i].name)||!node(&r->directories[i].identity,1)||
            r->directories[i].identity.value[0]!=r->payload.value[0]||r->directories[i].identity.value[1]==r->payload.value[1])return 0;
        for(size_t k=0;k<i;k++)if(!strcmp(r->directories[k].name,r->directories[i].name)||r->directories[k].identity.value[1]==r->directories[i].identity.value[1])return 0;
    }
    /* Every indexed ancestor must be present, not merely every listed path valid. */
    for(size_t i=0;i<index->count;i++)for(const char *p=index->files[i].name;*p;p++)if(*p=='/'){
        size_t n=(size_t)(p-index->files[i].name),k=0;for(;k<r->directory_count;k++)if(strlen(r->directories[k].name)==n&&!memcmp(r->directories[k].name,index->files[i].name,n))break;
        if(k==r->directory_count)return 0;
    }
    return 1;
}
const char *package_removal_record_capture(const PackageIndex *index,const PackageTreeSnapshot *snapshot,const struct stat *package,const struct stat *completion,PackageRemovalRecord *out){
    if(!index||!snapshot||!package||!completion||!out||snapshot->file_count>PACKAGE_INDEX_FILES||snapshot->directory_count>PACKAGE_TREE_DIRECTORIES)return "invalid_package_removal_record";
    PackageRemovalRecord *r=calloc(1,sizeof(*r));if(!r)return "allocation_failed";
    memcpy(r->index_sha256,snapshot->manifest_sha256,65);r->file_count=snapshot->file_count;r->directory_count=snapshot->directory_count;
    identity(package,&r->package);identity(&snapshot->root,&r->payload);identity(completion,&r->completion);
    for(size_t i=0;i<r->file_count;i++)identity(&snapshot->files[i],&r->files[i]);
    for(size_t i=0;i<r->directory_count;i++){memcpy(r->directories[i].name,snapshot->directories[i].name,256);identity(&snapshot->directories[i].identity,&r->directories[i].identity);}
    const char *e=shape(index,r)?NULL:"invalid_package_removal_record";if(!e)*out=*r;free(r);return e;
}
static void put64(unsigned char **at,uint64_t v){for(unsigned i=0;i<8;i++)*(*at)++=(unsigned char)(v>>(8*i));}
static uint64_t get64(const unsigned char **at){uint64_t v=0;for(unsigned i=0;i<8;i++)v|=(uint64_t)*(*at)++<<(8*i);return v;}
static void put_node(unsigned char **at,const PackageRemovalIdentity *id){for(unsigned i=0;i<10;i++)put64(at,id->value[i]);}
static void get_node(const unsigned char **at,PackageRemovalIdentity *id){for(unsigned i=0;i<10;i++)id->value[i]=get64(at);}
static void checksum(const void *body,size_t n,unsigned char out[32]){Sha256 hash;sha256_init(&hash);sha256_update(&hash,body,n);sha256_final(&hash,out);}
static unsigned char *encode(const PackageRemovalRecord *r,size_t *size){
    size_t n=8+64+16+240+r->file_count*80+32;for(size_t i=0;i<r->directory_count;i++)n+=8+strlen(r->directories[i].name)+80;
    if(n>PACKAGE_REMOVAL_BYTES)return NULL;unsigned char *body=malloc(n);if(!body)return NULL;unsigned char *at=body;
    memcpy(at,"SMRMV001",8);at+=8;memcpy(at,r->index_sha256,64);at+=64;put64(&at,r->file_count);put64(&at,r->directory_count);
    put_node(&at,&r->package);put_node(&at,&r->payload);put_node(&at,&r->completion);
    for(size_t i=0;i<r->file_count;i++)put_node(&at,&r->files[i]);
    for(size_t i=0;i<r->directory_count;i++){size_t length=strlen(r->directories[i].name);put64(&at,length);memcpy(at,r->directories[i].name,length);at+=length;put_node(&at,&r->directories[i].identity);}
    checksum(body,n-32,at);*size=n;return body;
}
static int bound(int directory,const char *name,int fd){struct stat a,b;return !fstat(fd,&a)&&!fstatat(directory,name,&b,AT_SYMLINK_NOFOLLOW)&&a.st_dev==b.st_dev&&a.st_ino==b.st_ino&&a.st_mode==b.st_mode&&a.st_nlink==b.st_nlink;}
static int root_matches(int directory,const PackageRemovalRecord *r){struct stat s;return !fstat(directory,&s)&&S_ISDIR(s.st_mode)&&s.st_uid==geteuid()&&!(s.st_mode&077)&&
    (uint64_t)s.st_dev==r->package.value[0]&&(uint64_t)s.st_ino==r->package.value[1]&&(uint64_t)s.st_mode==r->package.value[3]&&(fcntl(directory,F_GETFL)&O_ACCMODE)==O_RDONLY;}
static const char *recover(int directory,const PackageRemovalRecord *r,const unsigned char *body,size_t size,const char *(*control)(void *),void *context){
    int pending=openat(directory,".removal.record.pending",O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
    if(pending<0)return "unsafe_package_removal_record";
    struct stat before,after;const char *e=NULL;int final=-1;unsigned char chunk[65536];
    if(fstat(pending,&before)||!S_ISREG(before.st_mode)||before.st_uid!=geteuid()||(before.st_mode&077)||(before.st_nlink!=1&&before.st_nlink!=2)||
        before.st_size!=(off_t)size||!bound(directory,".removal.record.pending",pending))e="unsafe_package_removal_record";
    for(size_t at=0;!e&&at<size;){e=control?control(context):NULL;if(e)break;size_t n=size-at;if(n>sizeof(chunk))n=sizeof(chunk);
        ssize_t got=pread(pending,chunk,n,(off_t)at);if(got<0&&errno==EINTR)continue;
        if(got!=(ssize_t)n||memcmp(chunk,body+at,n)){e="unsafe_package_removal_record";break;}at+=n;}
    if(!e&&(fstat(pending,&after)||!bound(directory,".removal.record.pending",pending)))e="unsafe_package_removal_record";
    if(!e){PackageRemovalIdentity a,b;identity(&before,&a);identity(&after,&b);if(memcmp(&a,&b,sizeof(a)))e="unsafe_package_removal_record";}
    if(!e){final=openat(directory,"removal.record",O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
        if(before.st_nlink==2){struct stat named;if(final<0||fstat(final,&named)||named.st_dev!=before.st_dev||named.st_ino!=before.st_ino||!bound(directory,"removal.record",final))e="unsafe_package_removal_record";}
        else if(final>=0||errno!=ENOENT)e="package_removal_record_conflict";}
    if(!e&&(fchmod(pending,0400)||fsync(pending)))e="package_removal_record_failed";
    if(!e)e=control?control(context):NULL;
    if(!e&&(!root_matches(directory,r)||!bound(directory,".removal.record.pending",pending)))e="unsafe_package_removal_record";
    if(!e&&final<0&&linkat(directory,".removal.record.pending",directory,"removal.record",0))e="package_removal_record_conflict";
    if(!e&&(!bound(directory,"removal.record",pending)||unlinkat(directory,".removal.record.pending",0)||fsync(directory)))e="package_removal_record_failed";
    if(!e&&(fstat(pending,&after)||after.st_nlink!=1||!bound(directory,"removal.record",pending)))e="unsafe_package_removal_record";
    if(final>=0)close(final);close(pending);return e;
}
const char *package_removal_record_store(int directory,const PackageIndex *index,const PackageRemovalRecord *r,const char *(*control)(void *),void *context){
    if(!shape(index,r)||!root_matches(directory,r))return "invalid_package_removal_record";
    const char *e=control?control(context):NULL;if(e)return e;size_t size=0;unsigned char *body=encode(r,&size);if(!body)return "allocation_failed";
    int pending=openat(directory,".removal.record.pending",O_RDWR|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
    if(pending<0){e=errno==EEXIST?recover(directory,r,body,size,control,context):"unsafe_package_removal_record";free(body);return e;}
    size_t at=0;while(!e&&at<size){e=control?control(context):NULL;if(e)break;size_t n=size-at;if(n>65536)n=65536;ssize_t wrote=write(pending,body+at,n);
        if(wrote<0&&errno==EINTR)continue;if(wrote<=0){e="package_removal_record_failed";break;}at+=(size_t)wrote;}
    if(!e&&(fchmod(pending,0400)||fsync(pending)||!bound(directory,".removal.record.pending",pending)))e="package_removal_record_failed";
    if(!e)e=control?control(context):NULL;
    if(!e&&(!root_matches(directory,r)||linkat(directory,".removal.record.pending",directory,"removal.record",0)))e="package_removal_record_conflict";
    if(bound(directory,".removal.record.pending",pending)){if(unlinkat(directory,".removal.record.pending",0)&&!e)e="package_removal_record_failed";}else if(!e)e="unsafe_package_removal_record";
    if(!e&&(!bound(directory,"removal.record",pending)||fsync(directory)))e="package_removal_record_failed";
    close(pending);free(body);return e;
}
static const char *load(int directory,const PackageIndex *index,const char *(*control)(void *),void *context,PackageRemovalRecord *out,int pending){
    if(!out)return "invalid_package_removal_record";const char *e=control?control(context):NULL;if(e)return e;
    const char *name=pending?".removal.record.pending":"removal.record";
    int fd=openat(directory,name,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);if(fd<0)return errno==ENOENT?"package_removal_record_absent":"unsafe_package_removal_record";
    struct stat s,after;if(fstat(fd,&s)||!S_ISREG(s.st_mode)||s.st_uid!=geteuid()||(s.st_mode&077)||(s.st_nlink!=1&&!(pending&&s.st_nlink==2))||s.st_size<360||s.st_size>PACKAGE_REMOVAL_BYTES||!bound(directory,name,fd)){close(fd);return "unsafe_package_removal_record";}
    if(s.st_nlink==2){struct stat alias;if(fstatat(directory,"removal.record",&alias,AT_SYMLINK_NOFOLLOW)||alias.st_dev!=s.st_dev||alias.st_ino!=s.st_ino){close(fd);return "unsafe_package_removal_record";}}
    size_t size=(size_t)s.st_size,used=0;unsigned char *body=malloc(size);PackageRemovalRecord *r=calloc(1,sizeof(*r));if(!body||!r)e="allocation_failed";
    while(!e&&used<size){e=control?control(context):NULL;if(e)break;size_t n=size-used;if(n>65536)n=65536;ssize_t got=pread(fd,body+used,n,(off_t)used);if(got<0&&errno==EINTR)continue;if(got<=0){e="unsafe_package_removal_record";break;}used+=(size_t)got;}
    const unsigned char *at=body,*end=body?body+size-32:NULL;unsigned char digest[32];
    if(!e){checksum(body,size-32,digest);if(memcmp(digest,end,32)||memcmp(body,"SMRMV001",8))e="unsafe_package_removal_record";}
    if(!e){at+=8;memcpy(r->index_sha256,at,64);at+=64;uint64_t files=get64(&at),directories=get64(&at);
        if(files>PACKAGE_INDEX_FILES||directories>PACKAGE_TREE_DIRECTORIES||files>(uint64_t)(end-at-240)/80)e="unsafe_package_removal_record";
        else{r->file_count=(size_t)files;r->directory_count=(size_t)directories;get_node(&at,&r->package);get_node(&at,&r->payload);get_node(&at,&r->completion);for(size_t i=0;i<r->file_count;i++)get_node(&at,&r->files[i]);}
    }
    for(size_t i=0;!e&&i<r->directory_count;i++){
        if(end-at<8){e="unsafe_package_removal_record";break;}uint64_t n=get64(&at);
        if(!n||n>255||end-at<80||(uint64_t)(end-at-80)<n||memchr(at,0,(size_t)n)){e="unsafe_package_removal_record";break;}
        memcpy(r->directories[i].name,at,(size_t)n);at+=(size_t)n;get_node(&at,&r->directories[i].identity);
    }
    if(!e&&(at!=end||!shape(index,r)||!root_matches(directory,r)||fstat(fd,&after)||!bound(directory,name,fd)))e="unsafe_package_removal_record";
    if(!e){PackageRemovalIdentity original,latest;identity(&s,&original);identity(&after,&latest);if(memcmp(&original,&latest,sizeof(original)))e="unsafe_package_removal_record";}
    if(!e&&s.st_nlink==2){struct stat alias;if(fstatat(directory,"removal.record",&alias,AT_SYMLINK_NOFOLLOW)||alias.st_dev!=s.st_dev||alias.st_ino!=s.st_ino)e="unsafe_package_removal_record";}
    if(!e)e=control?control(context):NULL;if(!e)*out=*r;free(r);free(body);close(fd);return e;
}
const char *package_removal_record_load(int directory,const PackageIndex *index,const char *(*control)(void *),void *context,PackageRemovalRecord *out){return load(directory,index,control,context,out,0);}
const char *package_removal_record_load_pending(int directory,const PackageIndex *index,const char *(*control)(void *),void *context,PackageRemovalRecord *out){return load(directory,index,control,context,out,1);}
