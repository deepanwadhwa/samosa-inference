#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "package_stage.h"
#include "artifact.h"
#include "package_copy_record.h"
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
static const char *stage(int root,int journal,const PackageIndex *index,size_t selected,int input,const char *(*control)(void *),void *context){
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
    int existing=-1,output=-1;char temporary[80];int created=0,registered=0;
    snprintf(temporary,sizeof(temporary),".copy-%zu-%s",selected,file->sha256);
    if(!e){existing=openat(parent,leaf,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
        if(existing>=0){struct stat s;int tracked=0;
            if(!private(existing,0)||fstat(existing,&s)||!bound(parent,leaf,existing))e="unsafe_package_stage";
            if(!e&&journal>=0){const char *record=package_copy_record(journal,root,parent,existing,index,selected,0);
                if(!record)tracked=1;else if(strcmp(record,"package_copy_unregistered"))e=record;}
            if(!e&&journal>=0){e=package_copy_intent(journal,root,parent,index,selected,0);if(e&&!strcmp(e,"package_copy_unregistered"))e=NULL;}
            if(!e&&s.st_nlink!=1&&!(tracked&&s.st_nlink==2))e="unsafe_package_stage";
            if(!e)e=artifact_verify_controlled(existing,file->bytes,PACKAGE_PAYLOAD_BYTES,file->sha256,control,context);
            if(!e&&tracked&&s.st_nlink==2){int temporary_fd=openat(parent,temporary,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);struct stat t;
                if(temporary_fd<0||fstat(temporary_fd,&t)||t.st_dev!=s.st_dev||t.st_ino!=s.st_ino||!bound(parent,temporary,temporary_fd))e="unsafe_package_stage";
                if(!e&&(unlinkat(parent,temporary,0)||fsync(parent)))e="package_stage_failed";
                if(temporary_fd>=0)close(temporary_fd);}
            if(!e&&!bound(parent,leaf,existing))e="package_tree_changed";
            if(!e&&tracked)e=package_copy_record(journal,root,parent,existing,index,selected,2);
            if(!e&&journal>=0){e=package_copy_intent(journal,root,parent,index,selected,2);if(e&&!strcmp(e,"package_copy_unregistered"))e=NULL;}
            close(existing);close(parent);return e;
        }else if(errno!=ENOENT)e="unsafe_package_stage";
    }
    /* Reserved names cannot collide with an indexed payload. Never adopt or
     * delete a previous process's partial file without a recovery ledger. */
    if(!e){snprintf(temporary,sizeof(temporary),".copy-%zu-%s",selected,file->sha256);
        if(journal>=0){struct stat present;
            if(fstatat(parent,temporary,&present,AT_SYMLINK_NOFOLLOW)&&errno==ENOENT)e=package_copy_intent(journal,root,parent,index,selected,1);
        }
        if(e){close(parent);return e;}
        output=openat(parent,temporary,O_RDWR|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
        if(output<0&&errno==EEXIST&&journal>=0){
            output=openat(parent,temporary,O_RDWR|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
            if(output<0&&errno==EACCES)output=openat(parent,temporary,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
            if(output<0)e="unsafe_package_stage";
            else{e=package_copy_record(journal,root,parent,output,index,selected,0);if(!e)registered=1;
                else if(!strcmp(e,"package_copy_unregistered"))e=package_copy_intent(journal,root,parent,index,selected,0);}
        }else if(output<0)e=errno==EEXIST?"package_stage_busy":"package_stage_unavailable";else created=1;
        if(!e&&journal>=0&&!registered){e=control?control(context):NULL;if(!e)e=package_copy_record(journal,root,parent,output,index,selected,1);if(!e)registered=1;}}
    uint64_t retained=0;
    /* Validate intent metadata before touching a retained payload. Legacy
     * inode records may lack an intent, but conflicting metadata never waits
     * until cleanup to be reported. */
    if(!e&&registered){e=package_copy_intent(journal,root,parent,index,selected,0);if(e&&!strcmp(e,"package_copy_unregistered"))e=NULL;}
    if(!e&&registered){struct stat s;
        if(fstat(output,&s)||s.st_size<0||(uint64_t)s.st_size>file->bytes||s.st_nlink!=1||!bound(parent,temporary,output))e="unsafe_package_stage";
        else retained=(uint64_t)s.st_size;
        unsigned char source[65536],saved[65536];uint64_t at=0;
        while(!e&&at<retained){e=control?control(context):NULL;if(e)break;size_t n=sizeof(source);if(retained-at<n)n=(size_t)(retained-at);
            if(pread(input,source,n,(off_t)at)!=(ssize_t)n||pread(output,saved,n,(off_t)at)!=(ssize_t)n||memcmp(source,saved,n))e="package_copy_prefix_mismatch";at+=n;}
        if(!e&&retained<file->bytes&&lseek(output,(off_t)retained,SEEK_SET)<0)e="package_stage_failed";
    }
    unsigned char bytes[65536];uint64_t offset=retained;
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
    /* Settle buffered writes before comparing the verifier's before/after
     * metadata snapshots, particularly for large model files. */
    if(!e&&fsync(output))e="package_stage_failed";
    if(!e)e=artifact_verify_controlled(output,file->bytes,PACKAGE_PAYLOAD_BYTES,file->sha256,control,context);
    if(!e)e=artifact_verify_controlled(input,file->bytes,PACKAGE_PAYLOAD_BYTES,file->sha256,control,context);
    if(!e)e=control?control(context):NULL;
    if(!e&&(!bound(parent,temporary,output)||fchmod(output,0400)||fsync(output)))e="package_stage_failed";
    if(!e){
        /* linkat refuses an existing final name; no foreign file is overwritten.
         * The caller's lease covers the brief two-link publication interval. */
        if(linkat(parent,temporary,parent,leaf,0))e="package_stage_conflict";
    }
    if((created&&!registered)||(!e&&registered)){if(bound(parent,temporary,output)){if(unlinkat(parent,temporary,0)&&!e)e="package_stage_failed";}
        else if(!e)e="package_tree_changed";}
    if(registered&&output>=0&&fsync(output)&&!e)e="package_stage_failed";
    if(!e&&fsync(parent))e="package_stage_failed";
    if(!e&&registered)e=package_copy_record(journal,root,parent,output,index,selected,2);
    if(!e&&journal>=0){e=package_copy_intent(journal,root,parent,index,selected,2);if(e&&!strcmp(e,"package_copy_unregistered"))e=NULL;}
    if(output>=0)close(output);close(parent);return e;
}

const char *package_stage_file(int root,const PackageIndex *index,size_t selected,int input,const char *(*control)(void *),void *context){return stage(root,-1,index,selected,input,control,context);}
const char *package_stage_file_registered(int root,int journal,const PackageIndex *index,size_t selected,int input,const char *(*control)(void *),void *context){return stage(root,journal,index,selected,input,control,context);}
