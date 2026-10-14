#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "package_publish.h"
#include "catalog.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
static int private(int fd,int directory){struct stat s;return !fstat(fd,&s)&&s.st_uid==geteuid()&&!(s.st_mode&077)&&
    (directory?S_ISDIR(s.st_mode):S_ISREG(s.st_mode)&&s.st_nlink==1);}
static int bound(int parent,const char *name,int fd){struct stat a,b;return !fstat(fd,&a)&&!fstatat(parent,name,&b,AT_SYMLINK_NOFOLLOW)&&
    a.st_dev==b.st_dev&&a.st_ino==b.st_ino&&a.st_mode==b.st_mode&&a.st_uid==b.st_uid&&a.st_nlink==b.st_nlink;}
static const char *record(int parent,int marker,const char *expected,int length){
    struct stat s;char actual[256];
    return !private(marker,0)||!bound(parent,"complete",marker)||fstat(marker,&s)||s.st_size!=length||
        pread(marker,actual,(size_t)length,0)!=length||memcmp(expected,actual,(size_t)length)?"package_identity_conflict":NULL;
}
static const char *publication(const PackagePublication *c,PackageTreeResult *out,int create){
    if(!c||!c->index||!c->compatible||!out||!memchr(c->index->manifest_sha256,0,sizeof(c->index->manifest_sha256))||
        strlen(c->index->manifest_sha256)!=64||strspn(c->index->manifest_sha256,"0123456789abcdef")!=64)return "invalid_package_publication";
    const char *e=c->control?c->control(c->context):NULL;if(e)return e;
    if(!private(c->directory,1)||(fcntl(c->directory,F_GETFL)&O_ACCMODE)!=O_RDONLY)return "unsafe_package_publication";
    char platform[65],os[65];e=provider_catalog_host(platform,os);
    if(!e)e=package_index_qualify(c->index,platform,os);if(e)return e;
    int lock=-1,payload=-1,marker=-1,pending=-1,owned=0;
    lock=openat(c->directory,"publish.lock",O_RDWR|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC|(create?O_CREAT:0),0600);
    if(lock<0)return errno==ENOENT&&!create?"package_not_published":"unsafe_package_publication";
    if(!private(lock,0)||!bound(c->directory,"publish.lock",lock))e="unsafe_package_publication";
    if(!e&&flock(lock,LOCK_EX|LOCK_NB))e=errno==EWOULDBLOCK?"package_publication_busy":"package_publication_unavailable";
    if(!e){payload=openat(c->directory,"payload",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
        if(payload<0||!private(payload,1)||!bound(c->directory,"payload",payload))e="unsafe_package_publication";}
    struct stat identity;char expected[256];int length=0;
    if(!e){if(fstat(payload,&identity))e="package_tree_changed";
        else length=snprintf(expected,sizeof(expected),"samosa.package.complete.v1\n%s\n%llu %llu\n",c->index->manifest_sha256,
            (unsigned long long)identity.st_dev,(unsigned long long)identity.st_ino);
        if(length<=0||(size_t)length>=sizeof(expected))e="invalid_package_publication";}
    if(!e){marker=openat(c->directory,"complete",O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
        if(marker<0){if(errno!=ENOENT)e="unsafe_package_publication";else if(!create)e="package_not_published";}
        else e=record(c->directory,marker,expected,length);}
    PackageTreeResult observed;
    if(!e)e=package_tree_verify(payload,c->index,c->control,c->context,&observed);
    if(!e)e=c->compatible(payload,c->index,c->compatibility);
    /* Compatibility inspection may read files for a while. Reverify the entire
     * payload and its named root before making completion visible. */
    if(!e)e=create?package_tree_sync(payload,c->index,c->control,c->context,&observed):package_tree_verify(payload,c->index,c->control,c->context,&observed);
    if(!e&&(!bound(c->directory,"payload",payload)||!bound(c->directory,"publish.lock",lock)))e="package_tree_changed";
    if(!e&&marker>=0)e=record(c->directory,marker,expected,length);
    if(!e)e=c->control?c->control(c->context):NULL;
    if(!e&&marker<0){
        pending=openat(c->directory,".complete.pending",O_RDWR|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
        if(pending<0)e=errno==EEXIST?"package_publication_busy":"package_publish_failed";
        else{owned=1;size_t at=0;while(at<(size_t)length){ssize_t n=write(pending,expected+at,(size_t)length-at);
                if(n<0&&errno==EINTR)continue;if(n<=0){e="package_publish_failed";break;}at+=(size_t)n;}
            if(!e&&(fchmod(pending,0400)||fsync(pending)||!bound(c->directory,".complete.pending",pending)))e="package_publish_failed";
            if(!e)e=c->control?c->control(c->context):NULL;
            if(!e&&linkat(c->directory,".complete.pending",c->directory,"complete",0))e="package_publication_conflict";
        }
    }
    if(owned){if(bound(c->directory,".complete.pending",pending)){if(unlinkat(c->directory,".complete.pending",0)&&!e)e="package_publish_failed";}
        else if(!e)e="package_tree_changed";}
    if(!e&&create&&fsync(c->directory))e="package_publish_failed";
    if(!e){int completed=openat(c->directory,"complete",O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
        e=record(c->directory,completed,expected,length);if(completed>=0)close(completed);}
    if(!e)*out=observed;
    if(pending>=0)close(pending);if(marker>=0)close(marker);if(payload>=0)close(payload);close(lock);return e;
}
const char *package_publish(const PackagePublication *c,PackageTreeResult *out){return publication(c,out,1);}
const char *package_publication_verify(const PackagePublication *c,PackageTreeResult *out){return publication(c,out,0);}
