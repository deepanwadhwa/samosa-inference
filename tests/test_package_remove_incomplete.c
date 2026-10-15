#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "package_remove.h"
#include "package_stage.h"
#include "package_identity.h"
#include "package_reserve.h"
#include "sha256.h"
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"incomplete removal failure %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static const char *cancel(void *p){(void)p;return "cancelled";}
static const char *kill_copy(void *p){int root=*(int *)p;struct stat s;
    int fd=openat(root,"models/sub",O_RDONLY|O_DIRECTORY);if(fd>=0){
        char path[80];unsigned char bytes[131081];for(size_t i=0;i<sizeof(bytes);i++)bytes[i]=(unsigned char)(i%251);char hash[65];hash_bytes(bytes,sizeof(bytes),hash);snprintf(path,sizeof(path),".copy-1-%s",hash);
        int found=!fstatat(fd,path,&s,AT_SYMLINK_NOFOLLOW)&&s.st_size>0;close(fd);if(found)kill(getpid(),SIGKILL);}
    return NULL;
}
static const char *kill_inventory(void *p){int root=*(int *)p;struct stat s;
    if(!fstatat(root,".removal.record.pending",&s,AT_SYMLINK_NOFOLLOW)&&(s.st_mode&0777)==0400)kill(getpid(),SIGKILL);return NULL;
}
typedef struct {int root;const char *path;uint64_t bytes;} FullCopy;
static const char *cancel_full(void *p){FullCopy *c=p;struct stat s;
    return !fstatat(c->root,c->path,&s,AT_SYMLINK_NOFOLLOW)&&(uint64_t)s.st_size==c->bytes?"cancelled":NULL;
}
static const char *kill_marker(void *p){if(faccessat(*(int *)p,"complete",F_OK,0)<0)kill(getpid(),SIGKILL);return NULL;}
static const char *kill_first(void *p){if(faccessat(*(int *)p,"payload/models/a",F_OK,0)<0)kill(getpid(),SIGKILL);return NULL;}
static void write_file(int parent,const char *name,const void *body,size_t size){int fd=openat(parent,name,O_WRONLY|O_CREAT|O_EXCL,0600);CHECK(fd>=0);CHECK(write(fd,body,size)==(ssize_t)size);CHECK(!close(fd));}
static void killed(pid_t child){int status;CHECK(waitpid(child,&status,0)==child);if(!WIFSIGNALED(status)||WTERMSIG(status)!=SIGKILL)fprintf(stderr,"child did not reach fault: status=%d\n",status);CHECK(WIFSIGNALED(status)&&WTERMSIG(status)==SIGKILL);}
int main(void){
    char path[]="/tmp/samosa-incomplete-remove-XXXXXX";CHECK(mkdtemp(path));int base=open(path,O_RDONLY|O_DIRECTORY);CHECK(base>=0);
    CHECK(!mkdirat(base,"package",0700));CHECK(!mkdirat(base,"pool",0700));int package=openat(base,"package",O_RDONLY|O_DIRECTORY),pool=openat(base,"pool",O_RDONLY|O_DIRECTORY);CHECK(package>=0&&pool>=0);
    int lock=openat(package,"assemble.lock",O_RDWR|O_CREAT|O_EXCL,0600);CHECK(lock>=0);
    PackageIndex *index=calloc(1,sizeof(*index));CHECK(index);memset(index->manifest_sha256,'a',64);index->count=3;
    const char *names[]={"models/a","models/sub/b","missing/c"};unsigned char bytes[131081];for(size_t i=0;i<sizeof(bytes);i++)bytes[i]=(unsigned char)(i%251);
    for(size_t i=0;i<3;i++){strcpy(index->files[i].name,names[i]);index->files[i].role=PACKAGE_MODEL;index->files[i].bytes=i==1?sizeof(bytes):3;
        hash_bytes(i==1?(const void *)bytes:(const void *)"abc",(size_t)index->files[i].bytes,index->files[i].sha256);index->payload_bytes+=index->files[i].bytes;}
    CHECK(!package_identity_bind(package,index,NULL,NULL));PackageReservationResult charge;CHECK(!package_reserve(pool,package,index,index->payload_bytes,1,&charge));
    CHECK(!mkdirat(package,"payload",0700));CHECK(!mkdirat(package,"copy-journal",0700));int payload=openat(package,"payload",O_RDONLY|O_DIRECTORY),journal=openat(package,"copy-journal",O_RDONLY|O_DIRECTORY);CHECK(payload>=0&&journal>=0);
    CHECK(!mkdirat(payload,"models",0700));write_file(payload,names[0],"abc",3);write_file(base,"source",bytes,sizeof(bytes));int source=openat(base,"source",O_RDONLY);CHECK(source>=0);
    pid_t child=fork();CHECK(child>=0);if(!child){const char *e=package_stage_file_registered(payload,journal,index,1,source,kill_copy,&payload);fprintf(stderr,"copy fault returned: %s\n",e?e:"success");_exit(2);}killed(child);
    char partial[160],record_name[112];snprintf(partial,sizeof(partial),"models/sub/.copy-1-%s",index->files[1].sha256);snprintf(record_name,sizeof(record_name),"copy-1-%s",index->files[1].sha256);struct stat s;
    CHECK(!fstatat(payload,partial,&s,AT_SYMLINK_NOFOLLOW)&&s.st_size==65536);int out=-7;
    CHECK(!strcmp(package_remove_run(pool,package,index,cancel,NULL,&out),"cancelled"));CHECK(out==-7);
    CHECK(!flock(lock,LOCK_SH|LOCK_NB));CHECK(!strcmp(package_remove_run(pool,package,index,NULL,NULL,&out),"package_assembly_busy"));CHECK(!flock(lock,LOCK_UN));CHECK(out==-7);
    /* Preflight the entire payload, metadata and package namespace before
     * deleting the registered copy or already-published first file. */
    write_file(payload,"models/sub/foreign","unknown",7);CHECK(package_remove_begin(package,index,NULL,NULL));CHECK(!fstatat(payload,partial,&s,0)&&s.st_size==65536);CHECK(!unlinkat(payload,"models/sub/foreign",0));
    write_file(journal,"unknown","foreign",7);CHECK(package_remove_begin(package,index,NULL,NULL));CHECK(!faccessat(payload,partial,F_OK,0));CHECK(!unlinkat(journal,"unknown",0));
    write_file(package,"foreign","unknown",7);CHECK(package_remove_begin(package,index,NULL,NULL));CHECK(!faccessat(payload,partial,F_OK,0));CHECK(!unlinkat(package,"foreign",0));
    int fd=openat(journal,record_name,O_RDONLY|O_NOFOLLOW);CHECK(fd>=0);CHECK(!fchmod(fd,0600));CHECK(!close(fd));fd=openat(journal,record_name,O_RDWR|O_NOFOLLOW);CHECK(fd>=0);char first;CHECK(pread(fd,&first,1,0)==1);CHECK(pwrite(fd,"X",1,0)==1);
    CHECK(package_remove_begin(package,index,NULL,NULL));CHECK(!faccessat(payload,partial,F_OK,0));CHECK(!faccessat(payload,names[0],F_OK,0));CHECK(pwrite(fd,&first,1,0)==1);CHECK(!fchmod(fd,0400));CHECK(!close(fd));
    child=fork();CHECK(child>=0);if(!child){const char *e=package_remove_begin(package,index,kill_inventory,&package);fprintf(stderr,"inventory fault returned: %s\n",e?e:"success");_exit(2);}killed(child);
    CHECK(faccessat(payload,partial,F_OK,0)<0);CHECK(faccessat(journal,record_name,F_OK,0)<0);CHECK(!faccessat(payload,names[0],F_OK,0));CHECK(!faccessat(package,".removal.record.pending",F_OK,0));
    PackageRemovalRecord *record=malloc(sizeof(*record));CHECK(record);CHECK(!package_removal_record_load_pending(package,index,NULL,NULL,record));
    CHECK(!record->completion.value[1]&&record->files[0].value[1]&&!record->files[1].value[1]&&!record->files[2].value[1]);CHECK(record->directory_count==2);
    CHECK(!package_remove_begin(package,index,NULL,NULL));CHECK(!package_removal_record_load(package,index,NULL,NULL,record));
    /* A sparse inventory cannot masquerade as the strict completed format,
     * even with a recomputed valid checksum. Restore only our owned fixture. */
    fd=openat(package,"removal.record",O_RDONLY|O_NOFOLLOW);CHECK(fd>=0);CHECK(!fchmod(fd,0600));CHECK(!close(fd));
    fd=openat(package,"removal.record",O_RDWR|O_NOFOLLOW);CHECK(fd>=0);CHECK(!fstat(fd,&s));size_t record_size=(size_t)s.st_size;unsigned char *encoded=malloc(record_size);CHECK(encoded);
    CHECK(pread(fd,encoded,record_size,0)==(ssize_t)record_size);memcpy(encoded,"SMRMV001",8);Sha256 hash;sha256_init(&hash);sha256_update(&hash,encoded,record_size-32);sha256_final(&hash,encoded+record_size-32);
    CHECK(pwrite(fd,encoded,record_size,0)==(ssize_t)record_size);CHECK(!strcmp(package_removal_record_load(package,index,NULL,NULL,record),"unsafe_package_removal_record"));
    memcpy(encoded,"SMRMP001",8);sha256_init(&hash);sha256_update(&hash,encoded,record_size-32);sha256_final(&hash,encoded+record_size-32);CHECK(pwrite(fd,encoded,record_size,0)==(ssize_t)record_size);free(encoded);CHECK(!fchmod(fd,0400));CHECK(!close(fd));
    CHECK(!package_removal_record_load(package,index,NULL,NULL,record));
    /* A formerly missing indexed file is not part of this removal inventory. */
    write_file(payload,names[1],bytes,sizeof(bytes));CHECK(package_remove_payload(package,index,NULL,NULL));CHECK(!faccessat(payload,names[0],F_OK,0));CHECK(!unlinkat(payload,names[1],0));
    CHECK(!renameat(payload,names[0],base,"saved"));write_file(payload,names[0],"abc",3);CHECK(package_remove_payload(package,index,NULL,NULL));CHECK(!faccessat(payload,names[0],F_OK,0));CHECK(!unlinkat(payload,names[0],0));CHECK(!renameat(base,"saved",payload,names[0]));
    CHECK(!renameat(payload,"models/sub",base,"saved-dir"));CHECK(!mkdirat(payload,"models/sub",0700));CHECK(package_remove_payload(package,index,NULL,NULL));CHECK(!unlinkat(payload,"models/sub",AT_REMOVEDIR));CHECK(!renameat(base,"saved-dir",payload,"models/sub"));
    /* Rename-based negative fixtures change ctime even when restored. Start a
     * fresh inventory for our own restored fixture before testing crash resume. */
    CHECK(!unlinkat(package,"removal.record",0));CHECK(!package_remove_begin(package,index,NULL,NULL));
    child=fork();CHECK(child>=0);if(!child){const char *e=package_remove_payload(package,index,kill_first,&package);fprintf(stderr,"removal fault returned: %s\n",e?e:"success");_exit(2);}killed(child);
    CHECK(faccessat(payload,names[0],F_OK,0)<0);CHECK(!faccessat(package,"removal.record",F_OK,0));CHECK(!package_reserve(pool,package,index,index->payload_bytes,1,&charge));CHECK(charge.packages==1);
    const char *run_error=package_remove_run(pool,package,index,NULL,NULL,&out);if(run_error)fprintf(stderr,"incomplete run returned: %s\n",run_error);CHECK(!run_error);CHECK(out==1);CHECK(faccessat(package,"payload",F_OK,0)<0&&faccessat(package,"copy-journal",F_OK,0)<0&&faccessat(package,"removal.record",F_OK,0)<0);
    CHECK(!package_remove_run(pool,package,index,NULL,NULL,&out));CHECK(out==0);
    /* A crash immediately after payload-root creation has no files or journal. */
    CHECK(!mkdirat(package,"payload",0700));CHECK(!package_remove_run(pool,package,index,NULL,NULL,&out));CHECK(out==0);CHECK(faccessat(package,"payload",F_OK,0)<0);
    CHECK(!package_reserve(pool,package,index,index->payload_bytes,1,&charge));CHECK(!package_reservation_release_empty(pool,package,index,NULL,NULL,&out));CHECK(out==1);
    /* Settle a killed file-publication interval: both payload links and both
     * known metadata aliases identify exactly the same registered inode. */
    CHECK(!mkdirat(package,"payload",0700));CHECK(!mkdirat(package,"copy-journal",0700));int linked_root=openat(package,"payload",O_RDONLY|O_DIRECTORY),linked_journal=openat(package,"copy-journal",O_RDONLY|O_DIRECTORY);CHECK(linked_root>=0&&linked_journal>=0);
    FullCopy full={linked_root,partial,sizeof(bytes)};CHECK(!strcmp(package_stage_file_registered(linked_root,linked_journal,index,1,source,cancel_full,&full),"cancelled"));
    fd=openat(linked_root,partial,O_RDONLY|O_NOFOLLOW);CHECK(fd>=0);CHECK(!fchmod(fd,0400));CHECK(!close(fd));CHECK(!linkat(linked_root,partial,linked_root,names[1],0));
    char pending_record[128],intent[112],pending_intent[128];snprintf(pending_record,sizeof(pending_record),".pending-%s",record_name);snprintf(intent,sizeof(intent),"intent-1-%s",index->files[1].sha256);snprintf(pending_intent,sizeof(pending_intent),".pending-%s",intent);
    CHECK(!linkat(linked_journal,record_name,linked_journal,pending_record,0));CHECK(!linkat(linked_journal,intent,linked_journal,pending_intent,0));
    CHECK(!package_remove_run(pool,package,index,NULL,NULL,&out));CHECK(out==0);CHECK(!close(linked_root));CHECK(!close(linked_journal));
    /* Completed removal also restarts through the public coordinator after
     * its completion marker is retired but its recorded empty root remains. */
    CHECK(!mkdirat(package,"payload",0700));int completed=openat(package,"payload",O_RDONLY|O_DIRECTORY);CHECK(completed>=0);
    CHECK(!mkdirat(completed,"models",0700));CHECK(!mkdirat(completed,"models/sub",0700));CHECK(!mkdirat(completed,"missing",0700));
    write_file(completed,names[0],"abc",3);write_file(completed,names[1],bytes,sizeof(bytes));write_file(completed,names[2],"abc",3);
    CHECK(!fstat(completed,&s));char marker[256];int length=snprintf(marker,sizeof(marker),"samosa.package.complete.v1\n%s\n%llu %llu\n",index->manifest_sha256,(unsigned long long)s.st_dev,(unsigned long long)s.st_ino);
    write_file(package,"complete",marker,(size_t)length);CHECK(!package_remove_begin(package,index,NULL,NULL));CHECK(!package_remove_payload(package,index,NULL,NULL));
    child=fork();CHECK(child>=0);if(!child)_exit(package_remove_retire(package,index,kill_marker,&package)?2:3);killed(child);
    CHECK(!faccessat(package,"payload",F_OK,0)&&faccessat(package,"complete",F_OK,0)<0);CHECK(!package_remove_run(pool,package,index,NULL,NULL,&out));CHECK(out==0);CHECK(!close(completed));
    CHECK(!close(source));CHECK(!unlinkat(base,"source",0));CHECK(!close(payload));CHECK(!close(journal));free(record);free(index);
    CHECK(!unlinkat(package,"assembly.identity",0));CHECK(!unlinkat(package,"remove.lock",0));CHECK(!close(lock));CHECK(!unlinkat(package,"assemble.lock",0));CHECK(!close(package));CHECK(!unlinkat(base,"package",AT_REMOVEDIR));
    CHECK(!unlinkat(pool,"reserve.lock",0));CHECK(!close(pool));CHECK(!unlinkat(base,"pool",AT_REMOVEDIR));CHECK(!close(base));CHECK(!rmdir(path));
    printf("PASS incomplete package removal: %u checks; mixed partial/published/missing files, durable sparse inventory, SIGKILL recovery, inode refusal and quota retirement\n",checks);return 0;
}
