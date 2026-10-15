#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "package_stage.h"
#include "package_copy_record.h"
#include "package_tree.h"
#include "sha256.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"stage failure %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static const char *cancel(void *p){(void)p;return "cancelled";}
typedef struct {int root;const char *path;} CopyCancel;
static const char *cancel_copy(void *p){CopyCancel *c=p;return !faccessat(c->root,c->path,F_OK,0)?"cancelled":NULL;}
typedef struct {CopyCancel copy;uint64_t bytes;} FullCopy;
static const char *cancel_full(void *p){FullCopy *c=p;struct stat s;
    return !fstatat(c->copy.root,c->copy.path,&s,AT_SYMLINK_NOFOLLOW)&&(uint64_t)s.st_size==c->bytes?"cancelled":NULL;
}
static const char *kill_copy(void *p){CopyCancel *c=p;struct stat s;
    if(!fstatat(c->root,c->path,&s,AT_SYMLINK_NOFOLLOW)&&s.st_size>0)kill(getpid(),SIGKILL);
    return NULL;
}
static const char *kill_removed_copy(void *p){CopyCancel *c=p;
    if(faccessat(c->root,c->path,F_OK,0)<0)kill(getpid(),SIGKILL);return NULL;
}
static const char *kill_bootstrap(void *p){CopyCancel *c=p;
    if(!faccessat(c->root,c->path,F_OK,0))kill(getpid(),SIGKILL);return NULL;
}
int main(void){
    char base[]="/tmp/samosa-package-stage.XXXXXX";CHECK(mkdtemp(base));char root_path[256],source_path[256];
    snprintf(root_path,sizeof(root_path),"%s/payload",base);snprintf(source_path,sizeof(source_path),"%s/source",base);
    CHECK(!mkdir(root_path,0700));int root=open(root_path,O_RDONLY|O_DIRECTORY);CHECK(root>=0);
    int writer=open(source_path,O_WRONLY|O_CREAT|O_EXCL,0600);CHECK(writer>=0);CHECK(write(writer,"abc",3)==3);CHECK(!close(writer));
    int input=open(source_path,O_RDONLY);CHECK(input>=0);CHECK(lseek(input,1,SEEK_SET)==1);
    char source_alias[256];snprintf(source_alias,sizeof(source_alias),"%s/source-alias",base);CHECK(!link(source_path,source_alias));
    const char *body="{\"format\":\"samosa.inference.package.v1\",\"platform\":\"macos-arm64\",\"minimum_os\":\"14.0\",\"recipe\":\"fixture-v1\",\"files\":[{\"name\":\"models/model.gguf\",\"role\":\"model\",\"bytes\":3,\"sha256\":\"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad\"}]}";
    PackageIndex *index=malloc(sizeof(*index));CHECK(index);char pin[65];hash_bytes(body,strlen(body),pin);CHECK(!package_index_parse(body,strlen(body),pin,index));
    CHECK(!strcmp(package_stage_file(root,index,0,input,cancel,NULL),"cancelled"));CHECK(faccessat(root,"models",F_OK,0)<0);
    CHECK(!package_stage_file(root,index,0,input,NULL,NULL));CHECK(lseek(input,0,SEEK_CUR)==1);
    struct stat s;CHECK(!fstatat(root,"models/model.gguf",&s,0));CHECK((s.st_mode&0777)==0400&&s.st_nlink==1);
    PackageTreeResult result;CHECK(!package_tree_verify(root,index,NULL,NULL,&result));CHECK(result.files==1&&result.payload_bytes==3);
    CHECK(!package_stage_file(root,index,0,input,NULL,NULL));CHECK(!package_tree_verify(root,index,NULL,NULL,&result));
    CHECK(!unlinkat(root,"models/model.gguf",0));
    int parent=openat(root,"models",O_RDONLY|O_DIRECTORY);CHECK(parent>=0);char temporary[80];snprintf(temporary,sizeof(temporary),".copy-0-%s",index->files[0].sha256);
    writer=openat(parent,temporary,O_WRONLY|O_CREAT|O_EXCL,0600);CHECK(writer>=0);CHECK(write(writer,"foreign",7)==7);CHECK(!close(writer));
    CHECK(!strcmp(package_stage_file(root,index,0,input,NULL,NULL),"package_stage_busy"));CHECK(!fstatat(parent,temporary,&s,0)&&s.st_size==7);
    CHECK(!unlinkat(parent,temporary,0));
    char partial[128];snprintf(partial,sizeof(partial),"models/%s",temporary);CopyCancel interrupted={root,partial};
    CHECK(!strcmp(package_stage_file(root,index,0,input,cancel_copy,&interrupted),"cancelled"));
    CHECK(faccessat(parent,temporary,F_OK,0)<0&&faccessat(parent,"model.gguf",F_OK,0)<0);
    CHECK(!symlinkat(source_path,parent,"model.gguf"));CHECK(!strcmp(package_stage_file(root,index,0,input,NULL,NULL),"unsafe_package_stage"));CHECK(!unlinkat(parent,"model.gguf",0));
    writer=openat(parent,"model.gguf",O_WRONLY|O_CREAT|O_EXCL,0600);CHECK(writer>=0);CHECK(write(writer,"abd",3)==3);CHECK(!close(writer));
    CHECK(!strcmp(package_stage_file(root,index,0,input,NULL,NULL),"artifact_checksum_mismatch"));CHECK(!fstatat(parent,"model.gguf",&s,0)&&s.st_size==3);
    CHECK(!unlinkat(parent,"model.gguf",0));CHECK(!package_stage_file(root,index,0,input,NULL,NULL));
    size_t large_size=131081;unsigned char *large=malloc(large_size);CHECK(large);
    for(size_t i=0;i<large_size;i++)large[i]=(unsigned char)(i%251);
    writer=open(source_path,O_WRONLY|O_TRUNC);CHECK(writer>=0);CHECK(write(writer,large,large_size)==(ssize_t)large_size);CHECK(!close(writer));
    char large_sha[65],large_index[1024];hash_bytes(large,large_size,large_sha);free(large);
    snprintf(large_index,sizeof(large_index),"{\"format\":\"samosa.inference.package.v1\",\"platform\":\"macos-arm64\",\"minimum_os\":\"14.0\",\"recipe\":\"fixture-v1\",\"files\":[{\"name\":\"models/model.gguf\",\"role\":\"model\",\"bytes\":%zu,\"sha256\":\"%s\"}]}",large_size,large_sha);
    hash_bytes(large_index,strlen(large_index),pin);CHECK(!package_index_parse(large_index,strlen(large_index),pin,index));
    CHECK(!strcmp(package_stage_file(root,index,0,input,NULL,NULL),"artifact_size_mismatch"));
    CHECK(!unlinkat(parent,"model.gguf",0));CHECK(!package_stage_file(root,index,0,input,NULL,NULL));
    CHECK(lseek(input,0,SEEK_CUR)==1);CHECK(!package_tree_verify(root,index,NULL,NULL,&result));CHECK(result.payload_bytes==large_size);
    CHECK(!unlinkat(parent,"model.gguf",0));
    char journal_path[256];snprintf(journal_path,sizeof(journal_path),"%s/journal",base);CHECK(!mkdir(journal_path,0700));int journal=open(journal_path,O_RDONLY|O_DIRECTORY);CHECK(journal>=0);
    snprintf(temporary,sizeof(temporary),".copy-0-%s",index->files[0].sha256);snprintf(partial,sizeof(partial),"models/%s",temporary);interrupted=(CopyCancel){root,partial};
    /* No durable reservation means an existing temporary remains foreign. */
    writer=openat(parent,temporary,O_WRONLY|O_CREAT|O_EXCL,0600);CHECK(writer>=0);CHECK(!close(writer));
    CHECK(!strcmp(package_stage_file_registered(root,journal,index,0,input,NULL,NULL),"package_copy_unregistered"));CHECK(!unlinkat(parent,temporary,0));
    pid_t bootstrap=fork();CHECK(bootstrap>=0);if(!bootstrap)_exit(package_stage_file_registered(root,journal,index,0,input,kill_bootstrap,&interrupted)?2:3);
    int bootstrap_status;CHECK(waitpid(bootstrap,&bootstrap_status,0)==bootstrap&&WIFSIGNALED(bootstrap_status)&&WTERMSIG(bootstrap_status)==SIGKILL);
    CHECK(!fstatat(parent,temporary,&s,AT_SYMLINK_NOFOLLOW)&&s.st_size==0);
    char intent_name[96],pending_intent[112];snprintf(intent_name,sizeof(intent_name),"intent-0-%s",index->files[0].sha256);snprintf(pending_intent,sizeof(pending_intent),".pending-%s",intent_name);
    CHECK(!renameat(journal,intent_name,journal,pending_intent));
    writer=openat(journal,pending_intent,O_RDONLY|O_NOFOLLOW);CHECK(writer>=0);CHECK(!fchmod(writer,0600));CHECK(!close(writer));
    writer=openat(journal,pending_intent,O_RDWR|O_NOFOLLOW);CHECK(writer>=0);char intent_first;CHECK(pread(writer,&intent_first,1,0)==1);CHECK(pwrite(writer,"X",1,0)==1);
    CHECK(!strcmp(package_stage_file_registered(root,journal,index,0,input,NULL,NULL),"unsafe_package_copy_record"));CHECK(!fstatat(parent,temporary,&s,0)&&s.st_size==0);
    CHECK(pwrite(writer,&intent_first,1,0)==1);CHECK(!fchmod(writer,0400));CHECK(!close(writer));
    CHECK(!package_stage_file_registered(root,journal,index,0,input,NULL,NULL));CHECK(!package_tree_verify(root,index,NULL,NULL,&result));CHECK(!unlinkat(parent,"model.gguf",0));
    pid_t child=fork();CHECK(child>=0);if(!child)_exit(package_stage_file_registered(root,journal,index,0,input,kill_copy,&interrupted)?2:3);
    int status;CHECK(waitpid(child,&status,0)==child&&WIFSIGNALED(status)&&WTERMSIG(status)==SIGKILL);
    CHECK(!fstatat(parent,temporary,&s,AT_SYMLINK_NOFOLLOW)&&s.st_size==65536);
    /* Recover a completed registration record interrupted before its link.
     * Malformed pending metadata is retained; only the exact recorded tuple
     * can authorize publication and subsequent payload resumption. */
    char record_name[96],pending_record[112];snprintf(record_name,sizeof(record_name),"copy-0-%s",index->files[0].sha256);snprintf(pending_record,sizeof(pending_record),".pending-%s",record_name);
    CHECK(!renameat(journal,record_name,journal,pending_record));writer=openat(journal,pending_record,O_RDONLY|O_NOFOLLOW);CHECK(writer>=0);CHECK(!fchmod(writer,0600));CHECK(!close(writer));
    writer=openat(journal,pending_record,O_RDWR|O_NOFOLLOW);CHECK(writer>=0);char first;CHECK(pread(writer,&first,1,0)==1);CHECK(pwrite(writer,"X",1,0)==1);
    CHECK(!strcmp(package_stage_file_registered(root,journal,index,0,input,NULL,NULL),"unsafe_package_copy_record"));CHECK(faccessat(journal,pending_record,F_OK,0)==0);CHECK(pwrite(writer,&first,1,0)==1);CHECK(!close(writer));
    /* A recorded name alone cannot authorize a substituted inode or prefix. */
    CHECK(!renameat(parent,temporary,parent,"saved-copy"));writer=openat(parent,temporary,O_WRONLY|O_CREAT|O_EXCL,0600);CHECK(writer>=0);CHECK(write(writer,"foreign",7)==7);CHECK(!close(writer));
    CHECK(!strcmp(package_stage_file_registered(root,journal,index,0,input,NULL,NULL),"unsafe_package_copy_record"));CHECK(!fstatat(parent,temporary,&s,0)&&s.st_size==7);
    CHECK(!unlinkat(parent,temporary,0));CHECK(!renameat(parent,"saved-copy",parent,temporary));
    writer=openat(parent,temporary,O_WRONLY|O_NOFOLLOW);CHECK(writer>=0);CHECK(pwrite(writer,"x",1,0)==1);CHECK(!close(writer));
    CHECK(!strcmp(package_stage_file_registered(root,journal,index,0,input,NULL,NULL),"package_copy_prefix_mismatch"));CHECK(!fstatat(parent,temporary,&s,0)&&s.st_size==65536);
    unsigned char original;CHECK(pread(input,&original,1,0)==1);writer=openat(parent,temporary,O_WRONLY|O_NOFOLLOW);CHECK(writer>=0);CHECK(pwrite(writer,&original,1,0)==1);CHECK(!close(writer));
    /* The prefix checks above also exercise recovered registration. Recreate
     * its known two-link metadata interval and resume once more. */
    CHECK(!linkat(journal,record_name,journal,pending_record,0));
    CHECK(!package_stage_file_registered(root,journal,index,0,input,NULL,NULL));CHECK(!package_tree_verify(root,index,NULL,NULL,&result));CHECK(result.payload_bytes==large_size);
    CHECK(faccessat(parent,temporary,F_OK,0)<0);CHECK(lseek(input,0,SEEK_CUR)==1);
    /* Recover the registered two-link publication interval as well as partial
     * copying; only the recorded temporary inode is removed. */
    CHECK(!unlinkat(parent,"model.gguf",0));FullCopy full={{root,partial},large_size};
    CHECK(!strcmp(package_stage_file_registered(root,journal,index,0,input,cancel_full,&full),"cancelled"));
    /* Only the known matching pending alias may be removed. A different
     * inode is preserved even if it occupies the expected metadata name. */
    writer=openat(journal,pending_intent,O_WRONLY|O_CREAT|O_EXCL,0600);CHECK(writer>=0);CHECK(write(writer,"foreign",7)==7);CHECK(!close(writer));
    CHECK(!strcmp(package_stage_file_registered(root,journal,index,0,input,NULL,NULL),"unsafe_package_copy_record"));CHECK(!fstatat(journal,pending_intent,&s,0)&&s.st_size==7);CHECK(faccessat(parent,"model.gguf",F_OK,0)<0);CHECK(!fstatat(parent,temporary,&s,0)&&(uint64_t)s.st_size==large_size);CHECK(!unlinkat(journal,pending_intent,0));
    CHECK(!linkat(journal,intent_name,journal,pending_intent,0));
    writer=openat(parent,temporary,O_RDONLY|O_NOFOLLOW);CHECK(writer>=0);CHECK(!fchmod(writer,0400));CHECK(!close(writer));
    CHECK(!linkat(parent,temporary,parent,"model.gguf",0));CHECK(!fstatat(parent,"model.gguf",&s,0)&&s.st_nlink==2);
    CHECK(!package_stage_file_registered(root,journal,index,0,input,NULL,NULL));CHECK(!package_tree_verify(root,index,NULL,NULL,&result));CHECK(faccessat(parent,temporary,F_OK,0)<0);
    /* Partial removal never borrows ownership from a published name. */
    CHECK(!strcmp(package_stage_remove_partial(root,journal,index,0,NULL,NULL),"package_partial_published"));
    CHECK(!package_tree_verify(root,index,NULL,NULL,&result));CHECK(!unlinkat(parent,"model.gguf",0));
    child=fork();CHECK(child>=0);if(!child)_exit(package_stage_file_registered(root,journal,index,0,input,kill_copy,&interrupted)?2:3);
    CHECK(waitpid(child,&status,0)==child&&WIFSIGNALED(status)&&WTERMSIG(status)==SIGKILL);
    CHECK(!strcmp(package_stage_remove_partial(root,journal,index,0,cancel,NULL),"cancelled"));
    CHECK(!fstatat(parent,temporary,&s,0)&&s.st_size==65536);
    CHECK(!strcmp(package_copy_forget_absent(journal,root,parent,index,0),"unsafe_package_copy_record"));
    CHECK(!renameat(parent,temporary,parent,"saved-copy"));
    writer=openat(parent,temporary,O_WRONLY|O_CREAT|O_EXCL,0600);CHECK(writer>=0);CHECK(write(writer,"foreign",7)==7);CHECK(!close(writer));
    CHECK(!strcmp(package_stage_remove_partial(root,journal,index,0,NULL,NULL),"unsafe_package_copy_record"));
    CHECK(!fstatat(parent,temporary,&s,0)&&s.st_size==7);CHECK(!unlinkat(parent,temporary,0));
    CHECK(!symlinkat(source_path,parent,temporary));
    CHECK(!strcmp(package_stage_remove_partial(root,journal,index,0,NULL,NULL),"unsafe_package_stage"));CHECK(!unlinkat(parent,temporary,0));
    CHECK(!renameat(parent,"saved-copy",parent,temporary));CHECK(!linkat(parent,temporary,parent,"foreign-alias",0));
    CHECK(!strcmp(package_stage_remove_partial(root,journal,index,0,NULL,NULL),"unsafe_package_stage"));CHECK(!unlinkat(parent,"foreign-alias",0));
    /* Kill precisely after durable payload unlink, before retiring its record.
     * Retry reconstructs only canonical metadata; no source descriptor is used. */
    child=fork();CHECK(child>=0);if(!child)_exit(package_stage_remove_partial(root,journal,index,0,kill_removed_copy,&interrupted)?2:3);
    CHECK(waitpid(child,&status,0)==child&&WIFSIGNALED(status)&&WTERMSIG(status)==SIGKILL);
    CHECK(faccessat(parent,temporary,F_OK,0)<0);CHECK(!faccessat(journal,record_name,F_OK,0));
    writer=openat(journal,record_name,O_RDONLY|O_NOFOLLOW);CHECK(writer>=0);CHECK(!fchmod(writer,0600));CHECK(!close(writer));
    writer=openat(journal,record_name,O_RDWR|O_NOFOLLOW);CHECK(writer>=0);CHECK(pread(writer,&first,1,0)==1);CHECK(pwrite(writer,"X",1,0)==1);
    CHECK(!strcmp(package_stage_remove_partial(root,journal,index,0,NULL,NULL),"unsafe_package_copy_record"));
    CHECK(!faccessat(journal,record_name,F_OK,0));CHECK(pwrite(writer,&first,1,0)==1);CHECK(!fchmod(writer,0400));CHECK(!close(writer));
    writer=openat(parent,"model.gguf",O_WRONLY|O_CREAT|O_EXCL,0600);CHECK(writer>=0);CHECK(write(writer,"foreign",7)==7);CHECK(!close(writer));
    CHECK(!strcmp(package_stage_remove_partial(root,journal,index,0,NULL,NULL),"package_partial_published"));
    CHECK(!faccessat(journal,record_name,F_OK,0));CHECK(!unlinkat(parent,"model.gguf",0));
    writer=openat(journal,record_name,O_RDONLY|O_NOFOLLOW);CHECK(writer>=0);CHECK(!fchmod(writer,0600));CHECK(!close(writer));
    writer=openat(journal,record_name,O_RDWR|O_NOFOLLOW);CHECK(writer>=0);char saved_record[768],overflow_record[768];
    ssize_t saved_length=pread(writer,saved_record,sizeof(saved_record)-1,0);CHECK(saved_length>0&&saved_length<700);saved_record[saved_length]=0;
    char *last_number=strrchr(saved_record,' ');CHECK(last_number);size_t prefix=(size_t)(last_number+1-saved_record);
    memcpy(overflow_record,saved_record,prefix);strcpy(overflow_record+prefix,"18446744073709551616\n");size_t overflow_length=strlen(overflow_record);
    CHECK(!ftruncate(writer,0));CHECK(pwrite(writer,overflow_record,overflow_length,0)==(ssize_t)overflow_length);
    CHECK(!strcmp(package_stage_remove_partial(root,journal,index,0,NULL,NULL),"unsafe_package_copy_record"));CHECK(!faccessat(journal,record_name,F_OK,0));
    CHECK(!ftruncate(writer,0));CHECK(pwrite(writer,saved_record,(size_t)saved_length,0)==saved_length);CHECK(!fchmod(writer,0400));CHECK(!close(writer));
    CHECK(!renameat(journal,record_name,journal,pending_record));CHECK(!linkat(journal,intent_name,journal,pending_intent,0));
    CHECK(!package_stage_remove_partial(root,journal,index,0,NULL,NULL));
    CHECK(faccessat(journal,pending_record,F_OK,0)<0&&faccessat(journal,record_name,F_OK,0)<0);
    CHECK(faccessat(journal,pending_intent,F_OK,0)<0&&faccessat(journal,intent_name,F_OK,0)<0);
    CHECK(!package_stage_remove_partial(root,journal,index,0,NULL,NULL));
    /* A zero-byte bootstrap intent lacks an inode registration: preserve it. */
    child=fork();CHECK(child>=0);if(!child)_exit(package_stage_file_registered(root,journal,index,0,input,kill_bootstrap,&interrupted)?2:3);
    CHECK(waitpid(child,&status,0)==child&&WIFSIGNALED(status)&&WTERMSIG(status)==SIGKILL);
    CHECK(!strcmp(package_stage_remove_partial(root,journal,index,0,NULL,NULL),"package_copy_unregistered"));
    CHECK(!fstatat(parent,temporary,&s,0)&&s.st_size==0);CHECK(!faccessat(journal,intent_name,F_OK,0));
    CHECK(!package_stage_file_registered(root,journal,index,0,input,NULL,NULL));
    CHECK(!close(journal));CHECK(!rmdir(journal_path));
    CHECK(!unlinkat(parent,"model.gguf",0));CHECK(!close(parent));CHECK(!unlinkat(root,"models",AT_REMOVEDIR));CHECK(!close(root));CHECK(!close(input));
    CHECK(!unlink(source_path));CHECK(!unlink(source_alias));CHECK(!rmdir(root_path));CHECK(!rmdir(base));free(index);
    printf("PASS package staging: %u checks; pinned copies, source seek preservation, closed tree, no replacement/adoption and cancellation\n",checks);return 0;
}
