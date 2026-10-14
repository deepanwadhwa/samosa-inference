#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "package_stage.h"
#include "artifact.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static int private(int fd,int directory){struct stat s;return !fstat(fd,&s)&&s.st_uid==geteuid()&&!(s.st_mode&077)&&
    (directory?S_ISDIR(s.st_mode):S_ISREG(s.st_mode));}
static int bound(int parent,const char *name,int fd){struct stat a,b;return !fstat(fd,&a)&&!fstatat(parent,name,&b,AT_SYMLINK_NOFOLLOW)&&
    a.st_dev==b.st_dev&&a.st_ino==b.st_ino&&a.st_mode==b.st_mode&&a.st_uid==b.st_uid&&a.st_nlink==b.st_nlink;}
const char *package_stage_file(int root,const PackageIndex *index,size_t selected,int input,const char *(*control)(void *),void *context){
    if(!index||!index->count||index->count>PACKAGE_INDEX_FILES||selected>=index->count)return "invalid_package_index";
    const PackageFile *file=&index->files[selected];
    if(!memchr(file->name,0,sizeof(file->name))||!memchr(file->sha256,0,sizeof(file->sha256))||
        !package_file_name_valid(file->name,file->role)||!file->bytes||file->bytes>PACKAGE_PAYLOAD_BYTES)return "invalid_package_index";
    const char *e=control?control(context):NULL;if(e)return e;
    if(!private(root,1)||(fcntl(root,F_GETFL)&O_ACCMODE)!=O_RDONLY||!private(input,0)||
       (fcntl(input,F_GETFL)&O_ACCMODE)!=O_RDONLY)return "unsupported_package_source";
    e=artifact_verify_controlled(input,file->bytes,PACKAGE_PAYLOAD_BYTES,file->sha256,control,context);if(e)return e;
    char path[256];strcpy(path,file->name);int parent=openat(root,".",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if(parent<0)return "package_stage_unavailable";
    char *leaf=path;
    for(char *slash=strchr(leaf,'/');!e&&slash;slash=strchr(leaf,'/')){
        *slash=0;e=control?control(context):NULL;if(e)break;
        if(mkdirat(parent,leaf,0700)&&errno!=EEXIST){e="package_stage_unavailable";break;}
        int next=openat(parent,leaf,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
        if(next<0||!private(next,1)||!bound(parent,leaf,next)){if(next>=0)close(next);e="unsafe_package_stage";break;}
        if(fsync(parent)){close(next);e="package_stage_failed";break;}
        close(parent);parent=next;leaf=slash+1;
    }
    int existing=-1,output=-1;char temporary[80];int created=0;
    if(!e){existing=openat(parent,leaf,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
        if(existing>=0){struct stat s;
            if(!private(existing,0)||fstat(existing,&s)||s.st_nlink!=1||!bound(parent,leaf,existing))e="unsafe_package_stage";
            if(!e)e=artifact_verify_controlled(existing,file->bytes,PACKAGE_PAYLOAD_BYTES,file->sha256,control,context);
            if(!e&&!bound(parent,leaf,existing))e="package_tree_changed";
            close(existing);close(parent);return e;
        }else if(errno!=ENOENT)e="unsafe_package_stage";
    }
    /* Reserved names cannot collide with an indexed payload. Never adopt or
     * delete a previous process's partial file without a recovery ledger. */
    if(!e){snprintf(temporary,sizeof(temporary),".copy-%zu-%s",selected,file->sha256);
        output=openat(parent,temporary,O_RDWR|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
        if(output<0)e=errno==EEXIST?"package_stage_busy":"package_stage_unavailable";else created=1;}
    unsigned char bytes[65536];uint64_t offset=0;
    while(!e&&offset<file->bytes){
        e=control?control(context):NULL;if(e)break;
        size_t n=sizeof(bytes);if(file->bytes-offset<n)n=(size_t)(file->bytes-offset);
        ssize_t read_bytes=pread(input,bytes,n,(off_t)offset);
        if(read_bytes<0&&errno==EINTR)continue;
        if(read_bytes<=0){e="artifact_changed";break;}
        size_t at=0;while(at<(size_t)read_bytes){ssize_t written=write(output,bytes+at,(size_t)read_bytes-at);
            if(written<0&&errno==EINTR)continue;if(written<=0){e="package_stage_failed";break;}at+=(size_t)written;}
        offset+=(uint64_t)read_bytes;
    }
    if(!e)e=artifact_verify_controlled(output,file->bytes,PACKAGE_PAYLOAD_BYTES,file->sha256,control,context);
    if(!e)e=artifact_verify_controlled(input,file->bytes,PACKAGE_PAYLOAD_BYTES,file->sha256,control,context);
    if(!e)e=control?control(context):NULL;
    if(!e&&(!bound(parent,temporary,output)||fchmod(output,0400)||fsync(output)))e="package_stage_failed";
    if(!e){
        /* linkat refuses an existing final name; no foreign file is overwritten.
         * The caller's lease covers the brief two-link publication interval. */
        if(linkat(parent,temporary,parent,leaf,0))e="package_stage_conflict";
    }
    if(created){if(bound(parent,temporary,output)){if(unlinkat(parent,temporary,0)&&!e)e="package_stage_failed";}
        else if(!e)e="package_tree_changed";}
    if(!e&&fsync(parent))e="package_stage_failed";
    if(output>=0)close(output);close(parent);return e;
}
