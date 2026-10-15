#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "package_stage.h"
#include "artifact.h"
#include "package_copy_record.h"
#include "package_tree.h"
#include <dirent.h>
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
                if(!e&&unlinkat(parent,temporary,0))e="package_stage_failed";
                if(temporary_fd>=0)close(temporary_fd);}
            if(!e&&!bound(parent,leaf,existing))e="package_tree_changed";
            /* Settle retained publication links even on an already-single-link
             * retry before forgetting the journal that permits their recovery. */
            if(!e&&journal>=0&&(fsync(existing)||fsync(parent)))e="package_stage_failed";
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
    if(!e&&(!bound(parent,temporary,output)||fchmod(output,file->role==PACKAGE_ENGINE?0500:0400)||fsync(output)))e="package_stage_failed";
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

const char *package_stage_remove_partial(int root,int journal,const PackageIndex *index,size_t selected,const char *(*control)(void *),void *context){
    if(!index||!index->count||index->count>PACKAGE_INDEX_FILES||selected>=index->count)return "invalid_package_index";
    const PackageFile *file=&index->files[selected];
    if(!memchr(file->name,0,sizeof(file->name))||!memchr(file->sha256,0,sizeof(file->sha256))||
       !package_file_name_valid(file->name,file->role)||strlen(file->sha256)!=64||strspn(file->sha256,"0123456789abcdef")!=64||
       !file->bytes||file->bytes>PACKAGE_PAYLOAD_BYTES)return "invalid_package_index";
    const char *e=control?control(context):NULL;if(e)return e;
    if(!private(root,1)||!private(journal,1)||(fcntl(root,F_GETFL)&O_ACCMODE)!=O_RDONLY||
       (fcntl(journal,F_GETFL)&O_ACCMODE)!=O_RDONLY)return "unsafe_package_stage";
    char path[256];strcpy(path,file->name);char *leaf=path;
    int parent=openat(root,".",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(parent<0)return "package_stage_unavailable";
    for(char *slash=strchr(leaf,'/');!e&&slash;slash=strchr(leaf,'/')){
        *slash=0;int next=openat(parent,leaf,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
        if(next<0||!private(next,1)||!bound(parent,leaf,next)){if(next>=0)close(next);e="unsafe_package_stage";break;}
        close(parent);parent=next;leaf=slash+1;
    }
    char temporary[80];snprintf(temporary,sizeof(temporary),".copy-%zu-%s",selected,file->sha256);
    struct stat s;int copy=-1;
    /* Published files use the completed-file removal path. This operation owns
     * only the unpublished inode registered before the first copy write. */
    if(!e&&(!fstatat(parent,leaf,&s,AT_SYMLINK_NOFOLLOW)||errno!=ENOENT))e="package_partial_published";
    if(!e){copy=openat(parent,temporary,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
        if(copy<0&&errno!=ENOENT)e="unsafe_package_stage";}
    if(!e&&copy>=0){
        if(!private(copy,0)||fstat(copy,&s)||s.st_nlink!=1||s.st_size<0||(uint64_t)s.st_size>file->bytes||!bound(parent,temporary,copy))e="unsafe_package_stage";
        if(!e)e=package_copy_record(journal,root,parent,copy,index,selected,0);
        if(!e){e=package_copy_intent(journal,root,parent,index,selected,0);if(e&&!strcmp(e,"package_copy_unregistered"))e=NULL;}
        if(!e)e=control?control(context):NULL;
        if(!e&&(!bound(parent,temporary,copy)||!fstatat(parent,leaf,&s,AT_SYMLINK_NOFOLLOW)||errno!=ENOENT))e="package_tree_changed";
        if(!e&&unlinkat(parent,temporary,0))e="package_stage_failed";
    }
    /* Sync even an already-absent retry: its prior unlink may have completed
     * before a failed/interrupted parent sync. Metadata must outlive durability. */
    if(!e&&fsync(parent))e="package_stage_failed";
    /* Keep metadata until the unlink is durable. A killed process resumes from
     * canonical root/parent/index metadata with both payload names absent. */
    if(!e)e=control?control(context):NULL;
    if(!e){e=package_copy_forget_absent(journal,root,parent,index,selected);if(e&&!strcmp(e,"package_copy_unregistered"))e=NULL;}
    if(!e){e=package_copy_intent(journal,root,parent,index,selected,2);if(e&&!strcmp(e,"package_copy_unregistered"))e=NULL;}
    if(copy>=0)close(copy);close(parent);return e;
}

/* Open existing ancestors only. -2 is an absent ancestor, never permission to
 * discard metadata that still binds that ancestor's previous inode. */
static int partial_parent(int root,const char *name,char path[256],char **leaf){
    strcpy(path,name);*leaf=path;int parent=openat(root,".",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if(parent<0)return -1;
    for(char *slash=strchr(*leaf,'/');slash;slash=strchr(*leaf,'/')){
        *slash=0;int next=openat(parent,*leaf,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
        if(next<0){int absent=errno==ENOENT;close(parent);return absent?-2:-1;}
        if(!private(next,1)||!bound(parent,*leaf,next)){close(next);close(parent);return -1;}
        close(parent);parent=next;*leaf=slash+1;
    }
    return parent;
}
static void copy_names(const PackageIndex *index,size_t i,char names[4][112]){
    snprintf(names[0],112,"copy-%zu-%s",i,index->files[i].sha256);
    snprintf(names[1],112,"intent-%zu-%s",i,index->files[i].sha256);
    snprintf(names[2],112,".pending-copy-%zu-%s",i,index->files[i].sha256);snprintf(names[3],112,".pending-intent-%zu-%s",i,index->files[i].sha256);
}
static const char *partial_journal_names(int journal,const PackageIndex *index,const char *(*control)(void *),void *context){
    int copy=openat(journal,".",O_RDONLY|O_DIRECTORY|O_CLOEXEC);DIR *scan=copy>=0?fdopendir(copy):NULL;
    if(!scan){if(copy>=0)close(copy);return "unsafe_package_copy_record";}
    const char *e=NULL;size_t count=0;
    for(;;){e=control?control(context):NULL;if(e)break;errno=0;struct dirent *entry=readdir(scan);if(!entry){if(errno)e="unsafe_package_copy_record";break;}
        if(!strcmp(entry->d_name,".")||!strcmp(entry->d_name,".."))continue;
        if(++count>index->count*4){e="unsafe_package_copy_record";break;}
        int known=0;for(size_t i=0;!known&&i<index->count;i++){char names[4][112];copy_names(index,i,names);for(size_t k=0;k<4;k++)if(!strcmp(entry->d_name,names[k]))known=1;}
        if(!known){e="unsafe_package_copy_record";break;}
    }
    closedir(scan);return e;
}
static const char *partial_walk(int root,int fd,int journal,const char *prefix,const PackageIndex *index,size_t *visited,const char *(*control)(void *),void *context){
    int copy=openat(fd,".",O_RDONLY|O_DIRECTORY|O_CLOEXEC);DIR *scan=copy>=0?fdopendir(copy):NULL;
    if(!scan){if(copy>=0)close(copy);return "unsafe_package_stage";}const char *e=NULL;struct stat root_stat;
    if(fstat(root,&root_stat))e="unsafe_package_stage";
    while(!e){e=control?control(context):NULL;if(e)break;errno=0;struct dirent *entry=readdir(scan);if(!entry){if(errno)e="unsafe_package_stage";break;}
        if(!strcmp(entry->d_name,".")||!strcmp(entry->d_name,".."))continue;
        if(++*visited>PACKAGE_INDEX_FILES*2+PACKAGE_TREE_DIRECTORIES){e="unsafe_package_stage";break;}
        char path[256];int n=snprintf(path,sizeof(path),"%s%s%s",prefix,*prefix?"/":"",entry->d_name);struct stat s;
        if(n<0||(size_t)n>=sizeof(path)||fstatat(fd,entry->d_name,&s,AT_SYMLINK_NOFOLLOW)||s.st_uid!=geteuid()||(s.st_mode&077)||s.st_dev!=root_stat.st_dev){e="unsafe_package_stage";break;}
        int item=openat(fd,entry->d_name,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC|(S_ISDIR(s.st_mode)?O_DIRECTORY:0));
        if(item<0||!bound(fd,entry->d_name,item)){if(item>=0)close(item);e="unsafe_package_stage";break;}
        if(S_ISDIR(s.st_mode)){
            int needed=0;for(size_t i=0;i<index->count;i++)if(strlen(index->files[i].name)>(size_t)n&&!memcmp(path,index->files[i].name,(size_t)n)&&index->files[i].name[n]=='/')needed=1;
            e=needed?partial_walk(root,item,journal,path,index,visited,control,context):"unlisted_package_artifact";
        }else if(S_ISREG(s.st_mode)){
            size_t i=0;for(;i<index->count;i++)if(!strcmp(path,index->files[i].name))break;
            if(i<index->count)e=artifact_verify_controlled(item,index->files[i].bytes,PACKAGE_PAYLOAD_BYTES,index->files[i].sha256,control,context);
            else{for(i=0;i<index->count;i++){
                    const char *slash=strrchr(index->files[i].name,'/');size_t parent_length=slash?(size_t)(slash-index->files[i].name):0;
                    char temporary[80];snprintf(temporary,sizeof(temporary),".copy-%zu-%s",i,index->files[i].sha256);
                    if(strlen(prefix)==parent_length&&!memcmp(prefix,index->files[i].name,parent_length)&&!strcmp(entry->d_name,temporary))break;
                }
                if(i==index->count)e="unlisted_package_artifact";
                else if(journal<0||s.st_nlink>2||s.st_size<0||(uint64_t)s.st_size>index->files[i].bytes)e="unsafe_package_stage";
                else e=package_copy_record(journal,root,fd,item,index,i,0);
            }
        }else e="unsafe_package_stage";
        close(item);
    }
    closedir(scan);return e;
}
const char *package_stage_prepare_removal(int root,int journal,const PackageIndex *index,const char *(*control)(void *),void *context){
    if(!index||!index->count||index->count>PACKAGE_INDEX_FILES)return "invalid_package_index";
    for(size_t i=0;i<index->count;i++)if(!memchr(index->files[i].name,0,256)||!package_file_name_valid(index->files[i].name,index->files[i].role)||
       !memchr(index->files[i].sha256,0,65)||strlen(index->files[i].sha256)!=64||strspn(index->files[i].sha256,"0123456789abcdef")!=64||
       !index->files[i].bytes||index->files[i].bytes>PACKAGE_PAYLOAD_BYTES)return "invalid_package_index";
    /* Validate every path and journal entry before deleting any partial copy.
     * The sparse snapshot taken afterwards captures all published inode pins. */
    if(!private(root,1)||(journal>=0&&!private(journal,1)))return "unsafe_package_stage";
    const char *e=control?control(context):NULL;size_t visited=0;
    if(!e)e=partial_walk(root,root,journal,"",index,&visited,control,context);
    if(!e&&journal>=0)e=partial_journal_names(journal,index,control,context);
    for(int pass=0;!e&&pass<2;pass++)for(size_t i=0;!e&&i<index->count;i++){
        e=control?control(context):NULL;if(e)break;
        char path[256],*leaf;int parent=partial_parent(root,index->files[i].name,path,&leaf);
        if(parent==-2){if(journal>=0){char names[4][112];copy_names(index,i,names);struct stat s;
                for(size_t k=0;!e&&k<4;k++)if(!fstatat(journal,names[k],&s,AT_SYMLINK_NOFOLLOW)||errno!=ENOENT)e="unsafe_package_copy_record";}continue;}
        if(parent<0){e="unsafe_package_stage";break;}
        char temporary[80];snprintf(temporary,sizeof(temporary),".copy-%zu-%s",i,index->files[i].sha256);
        int final=openat(parent,leaf,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC),saved_final_errno=errno;
        int partial=openat(parent,temporary,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC),saved_partial_errno=errno;
        struct stat f={0},p={0};
        if((final<0&&saved_final_errno!=ENOENT)||(partial<0&&saved_partial_errno!=ENOENT)||
           (final>=0&&(!private(final,0)||fstat(final,&f)||!bound(parent,leaf,final)||f.st_nlink>2))||
           (partial>=0&&(!private(partial,0)||fstat(partial,&p)||!bound(parent,temporary,partial)||p.st_nlink>2)))e="unsafe_package_stage";
        if(!e&&((final>=0&&partial>=0&&(f.st_dev!=p.st_dev||f.st_ino!=p.st_ino||f.st_nlink!=2||p.st_nlink!=2))||
            (final>=0&&partial<0&&f.st_nlink!=1)||(partial>=0&&final<0&&p.st_nlink!=1)))e="unsafe_package_stage";
        if(!e&&journal>=0){
            e=final>=0||partial>=0?package_copy_record(journal,root,parent,final>=0?final:partial,index,i,0):package_copy_check_absent(journal,root,parent,index,i);
            if(e&&!strcmp(e,"package_copy_unregistered")&&partial<0)e=NULL;
            if(!e){e=package_copy_intent(journal,root,parent,index,i,0);if(e&&!strcmp(e,"package_copy_unregistered"))e=NULL;}
        }
        if(!e&&pass&&journal>=0)e=final>=0?package_stage_file_registered(root,journal,index,i,final,control,context):package_stage_remove_partial(root,journal,index,i,control,context);
        if(partial>=0)close(partial);if(final>=0)close(final);close(parent);
    }
    return e;
}
