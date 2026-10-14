#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "package_removal_record.h"
#include "sha256.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"removal record failure %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static const char *cancel(void *unused){(void)unused;return "cancelled";}
static const char *pending_death(void *opaque){int directory=*(int *)opaque;struct stat s;
    if(!fstatat(directory,".removal.record.pending",&s,AT_SYMLINK_NOFOLLOW)&&(s.st_mode&0777)==0400)kill(getpid(),SIGKILL);return NULL;
}
int main(void){
    char path[]="/tmp/samosa-removal-record-XXXXXX";CHECK(mkdtemp(path));int package=open(path,O_RDONLY|O_DIRECTORY);CHECK(package>=0);CHECK(!mkdirat(package,"payload",0700));int payload=openat(package,"payload",O_RDONLY|O_DIRECTORY);CHECK(payload>=0);
    CHECK(!mkdirat(payload,"models",0700));int fd=openat(payload,"models/model",O_WRONLY|O_CREAT|O_EXCL,0600);CHECK(fd>=0);CHECK(write(fd,"abc",3)==3);CHECK(!close(fd));
    PackageIndex *index=calloc(1,sizeof(*index));PackageTreeSnapshot *snapshot=calloc(1,sizeof(*snapshot));PackageRemovalRecord *record=calloc(1,sizeof(*record)),*loaded=malloc(sizeof(*loaded)),*before=malloc(sizeof(*before));CHECK(index&&snapshot&&record&&loaded&&before);
    index->count=1;index->payload_bytes=3;memset(index->manifest_sha256,'a',64);strcpy(index->files[0].name,"models/model");index->files[0].role=PACKAGE_MODEL;index->files[0].bytes=3;hash_bytes("abc",3,index->files[0].sha256);
    CHECK(!package_tree_snapshot(payload,index,NULL,NULL,snapshot));fd=openat(package,"complete",O_WRONLY|O_CREAT|O_EXCL,0600);CHECK(fd>=0);CHECK(write(fd,"completion-placeholder",22)==22);CHECK(!close(fd));
    struct stat root_stat,completion;CHECK(!fstat(package,&root_stat));CHECK(!fstatat(package,"complete",&completion,AT_SYMLINK_NOFOLLOW));CHECK(!package_removal_record_capture(index,snapshot,&root_stat,&completion,record));
    memset(loaded,0xa5,sizeof(*loaded));memcpy(before,loaded,sizeof(*loaded));CHECK(!strcmp(package_removal_record_load(package,index,NULL,NULL,loaded),"package_removal_record_absent"));CHECK(!memcmp(loaded,before,sizeof(*loaded)));
    CHECK(!strcmp(package_removal_record_store(package,index,record,cancel,NULL),"cancelled"));CHECK(faccessat(package,"removal.record",F_OK,0)<0);
    pid_t child=fork();CHECK(child>=0);if(!child)_exit(package_removal_record_store(package,index,record,pending_death,&package)?2:3);
    int status;CHECK(waitpid(child,&status,0)==child&&WIFSIGNALED(status)&&WTERMSIG(status)==SIGKILL);CHECK(faccessat(package,"removal.record",F_OK,0)<0);
    CHECK(!strcmp(package_removal_record_store(package,index,record,cancel,NULL),"cancelled"));CHECK(faccessat(package,".removal.record.pending",F_OK,0)==0);
    int damaged=openat(package,".removal.record.pending",O_RDONLY|O_NOFOLLOW);CHECK(damaged>=0);CHECK(!fchmod(damaged,0600));CHECK(!close(damaged));damaged=openat(package,".removal.record.pending",O_RDWR|O_NOFOLLOW);CHECK(damaged>=0);char original;CHECK(pread(damaged,&original,1,0)==1);CHECK(pwrite(damaged,"X",1,0)==1);
    CHECK(!strcmp(package_removal_record_store(package,index,record,NULL,NULL),"unsafe_package_removal_record"));CHECK(faccessat(package,".removal.record.pending",F_OK,0)==0);CHECK(pwrite(damaged,&original,1,0)==1);CHECK(!close(damaged));
    CHECK(!package_removal_record_load_pending(package,index,NULL,NULL,loaded));CHECK(!memcmp(record,loaded,sizeof(*record)));
    CHECK(!package_removal_record_store(package,index,loaded,NULL,NULL));CHECK(!package_removal_record_load(package,index,NULL,NULL,loaded));CHECK(!memcmp(record,loaded,sizeof(*record)));
    CHECK(!linkat(package,"removal.record",package,".removal.record.pending",0));CHECK(!strcmp(package_removal_record_load(package,index,NULL,NULL,loaded),"unsafe_package_removal_record"));
    CHECK(!package_removal_record_load_pending(package,index,NULL,NULL,loaded));CHECK(!memcmp(record,loaded,sizeof(*record)));
    CHECK(!package_removal_record_store(package,index,record,NULL,NULL));CHECK(faccessat(package,".removal.record.pending",F_OK,0)<0);CHECK(!package_removal_record_load(package,index,NULL,NULL,loaded));
    memcpy(before,loaded,sizeof(*loaded));CHECK(!strcmp(package_removal_record_load(package,index,cancel,NULL,loaded),"cancelled"));CHECK(!memcmp(loaded,before,sizeof(*loaded)));
    index->manifest_sha256[0]='b';CHECK(!strcmp(package_removal_record_load(package,index,NULL,NULL,loaded),"unsafe_package_removal_record"));CHECK(!memcmp(loaded,before,sizeof(*loaded)));index->manifest_sha256[0]='a';
    CHECK(!strcmp(package_removal_record_store(package,index,record,NULL,NULL),"package_removal_record_conflict"));
    fd=openat(package,"removal.record",O_RDONLY|O_NOFOLLOW);CHECK(fd>=0);struct stat s;CHECK(!fstat(fd,&s)&&(s.st_mode&0777)==0400&&s.st_nlink==1);CHECK(!fchmod(fd,0600));CHECK(!close(fd));
    fd=openat(package,"removal.record",O_RDWR|O_NOFOLLOW);CHECK(fd>=0);char first;CHECK(pread(fd,&first,1,0)==1);CHECK(pwrite(fd,"X",1,0)==1);CHECK(!strcmp(package_removal_record_load(package,index,NULL,NULL,loaded),"unsafe_package_removal_record"));CHECK(!memcmp(loaded,before,sizeof(*loaded)));CHECK(pwrite(fd,&first,1,0)==1);CHECK(!close(fd));
    CHECK(!linkat(package,"removal.record",package,"alias",0));CHECK(!strcmp(package_removal_record_load(package,index,NULL,NULL,loaded),"unsafe_package_removal_record"));
    CHECK(!renameat(package,"removal.record",package,".removal.record.pending"));CHECK(!strcmp(package_removal_record_load_pending(package,index,NULL,NULL,loaded),"unsafe_package_removal_record"));
    CHECK(!strcmp(package_removal_record_store(package,index,record,NULL,NULL),"unsafe_package_removal_record"));CHECK(faccessat(package,".removal.record.pending",F_OK,0)==0);CHECK(!renameat(package,".removal.record.pending",package,"removal.record"));
    CHECK(!unlinkat(package,"alias",0));CHECK(!package_removal_record_load(package,index,NULL,NULL,loaded));
    /* Valid checksums cannot hide impossible counts or truncated node tables. */
    fd=openat(package,"removal.record",O_RDWR|O_NOFOLLOW);CHECK(fd>=0);unsigned char *body=malloc((size_t)s.st_size);CHECK(body);CHECK(pread(fd,body,(size_t)s.st_size,0)==s.st_size);
    unsigned char saved=body[72];body[72]=255;Sha256 hash;sha256_init(&hash);sha256_update(&hash,body,(size_t)s.st_size-32);sha256_final(&hash,body+s.st_size-32);CHECK(pwrite(fd,body,(size_t)s.st_size,0)==s.st_size);
    CHECK(!strcmp(package_removal_record_load(package,index,NULL,NULL,loaded),"unsafe_package_removal_record"));CHECK(!memcmp(loaded,record,sizeof(*loaded)));
    body[72]=saved;sha256_init(&hash);sha256_update(&hash,body,(size_t)s.st_size-32);sha256_final(&hash,body+s.st_size-32);CHECK(pwrite(fd,body,(size_t)s.st_size,0)==s.st_size);
    CHECK(!ftruncate(fd,359));CHECK(!strcmp(package_removal_record_load(package,index,NULL,NULL,loaded),"unsafe_package_removal_record"));CHECK(!memcmp(loaded,record,sizeof(*loaded)));CHECK(!close(fd));free(body);
    /* Inventory capture does not silently omit a required ancestor. */
    snapshot->directory_count=0;CHECK(!strcmp(package_removal_record_capture(index,snapshot,&root_stat,&completion,loaded),"invalid_package_removal_record"));CHECK(!memcmp(loaded,record,sizeof(*loaded)));
    CHECK(!unlinkat(package,"removal.record",0));CHECK(!unlinkat(package,"complete",0));CHECK(!unlinkat(payload,"models/model",0));CHECK(!unlinkat(payload,"models",AT_REMOVEDIR));CHECK(!close(payload));CHECK(!unlinkat(package,"payload",AT_REMOVEDIR));CHECK(!close(package));CHECK(!rmdir(path));
    free(index);free(snapshot);free(record);free(loaded);free(before);printf("PASS removal record: %u checks; portable inventory, durable no-replacement, index/root binding, corruption, aliases and controls\n",checks);return 0;
}
