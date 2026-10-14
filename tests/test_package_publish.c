#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "package_publish.h"
#include "platform.h"
#include "sha256.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"publish failure %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static const char *compatibility(int root,const PackageIndex *index,void *context){(void)root;(void)index;return context;}
static const char *cancel(void *p){(void)p;return "cancelled";}
static const char *change_marker(int payload,const PackageIndex *index,void *context){
    (void)payload;(void)index;int root=*(int *)context;CHECK(!unlinkat(root,"complete",0));
    int fd=openat(root,"complete",O_WRONLY|O_CREAT|O_EXCL,0600);CHECK(fd>=0);CHECK(write(fd,"foreign",7)==7);CHECK(!close(fd));return NULL;
}
static void failed(PackagePublication *c,const char *expected){PackageTreeResult out,before;memset(&out,42,sizeof(out));memcpy(&before,&out,sizeof(out));
    const char *e=package_publish(c,&out);if(!e||strcmp(e,expected))fprintf(stderr,"expected %s, received %s\n",expected,e?e:"success");CHECK(e&&!strcmp(e,expected));CHECK(!memcmp(&out,&before,sizeof(out)));}
static const char *publication_death(void *opaque){int root=*(int *)opaque;struct stat s;
    if(!fstatat(root,".complete.pending",&s,AT_SYMLINK_NOFOLLOW))kill(getpid(),SIGKILL);
    return NULL;
}
int main(void){
    char base[]="/tmp/samosa-package-publish.XXXXXX";CHECK(mkdtemp(base));int root=open(base,O_RDONLY|O_DIRECTORY);CHECK(root>=0);
    CHECK(!mkdirat(root,"payload",0700));int payload=openat(root,"payload",O_RDONLY|O_DIRECTORY);CHECK(payload>=0);
    int file=openat(payload,"model",O_WRONLY|O_CREAT|O_EXCL,0600);CHECK(file>=0);CHECK(write(file,"abc",3)==3);CHECK(!close(file));
    char body[1024],pin[65];snprintf(body,sizeof(body),"{\"format\":\"samosa.inference.package.v1\",\"platform\":\"%s\",\"minimum_os\":\"1.0\",\"recipe\":\"fixture-v1\",\"files\":[{\"name\":\"model\",\"role\":\"model\",\"bytes\":3,\"sha256\":\"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad\"}]}",inference_host_platform());
    PackageIndex *index=malloc(sizeof(*index));CHECK(index);hash_bytes(body,strlen(body),pin);CHECK(!package_index_parse(body,strlen(body),pin,index));
    PackagePublication c={.directory=root,.index=index,.compatible=compatibility};PackageTreeResult out;
    CHECK(!strcmp(package_publication_verify(&c,&out),"package_not_published"));
    c.compatible=NULL;failed(&c,"invalid_package_publication");c.compatible=compatibility;
    c.compatibility="unsupported_package_recipe";failed(&c,"unsupported_package_recipe");CHECK(faccessat(root,"complete",F_OK,0)<0);c.compatibility=NULL;
    c.control=cancel;failed(&c,"cancelled");c.control=NULL;
    CHECK(!package_publish(&c,&out));CHECK(out.files==1&&out.payload_bytes==3);
    CHECK(!renameat(root,"complete",root,".complete.pending"));CHECK(!strcmp(package_publication_verify(&c,&out),"package_publication_recovery_required"));CHECK(faccessat(root,"complete",F_OK,0)<0);
    CHECK(!package_publish(&c,&out));CHECK(!package_publication_verify(&c,&out));
    CHECK(!linkat(root,"complete",root,".complete.pending",0));CHECK(!strcmp(package_publication_verify(&c,&out),"package_publication_recovery_required"));
    CHECK(!package_publish(&c,&out));CHECK(!package_publication_verify(&c,&out));CHECK(faccessat(root,".complete.pending",F_OK,0)<0);
    CHECK(!unlinkat(root,"complete",0));c.control=publication_death;c.context=&root;
    pid_t child=fork();CHECK(child>=0);if(!child)_exit(package_publish(&c,&out)?2:3);
    int status;CHECK(waitpid(child,&status,0)==child&&WIFSIGNALED(status)&&WTERMSIG(status)==SIGKILL);c.control=NULL;c.context=NULL;
    CHECK(!strcmp(package_publication_verify(&c,&out),"package_publication_recovery_required"));
    c.compatibility="unsupported_package_recipe";failed(&c,"unsupported_package_recipe");CHECK(faccessat(root,".complete.pending",F_OK,0)==0);CHECK(faccessat(root,"complete",F_OK,0)<0);c.compatibility=NULL;
    CHECK(!package_publish(&c,&out));CHECK(!package_publication_verify(&c,&out));
    struct stat s;CHECK(!fstatat(root,"complete",&s,0)&&s.st_nlink==1&&(s.st_mode&0777)==0400);
    CHECK(!package_publication_verify(&c,&out));CHECK(!package_publish(&c,&out));
    c.compatible=change_marker;c.compatibility=&root;failed(&c,"package_identity_conflict");
    c.compatible=compatibility;c.compatibility=NULL;CHECK(!unlinkat(root,"complete",0));CHECK(!package_publish(&c,&out));
    int lock=openat(root,"publish.lock",O_RDWR);CHECK(lock>=0&&!flock(lock,LOCK_EX|LOCK_NB));failed(&c,"package_publication_busy");CHECK(!close(lock));
    file=openat(payload,"model",O_WRONLY|O_TRUNC);CHECK(file>=0);CHECK(write(file,"abd",3)==3);CHECK(!close(file));failed(&c,"artifact_checksum_mismatch");
    file=openat(payload,"model",O_WRONLY|O_TRUNC);CHECK(file>=0);CHECK(write(file,"abc",3)==3);CHECK(!close(file));CHECK(!package_publication_verify(&c,&out));
    CHECK(!unlinkat(root,"complete",0));file=openat(root,".complete.pending",O_WRONLY|O_CREAT|O_EXCL,0600);CHECK(file>=0);CHECK(write(file,"foreign",7)==7);CHECK(!close(file));
    failed(&c,"package_identity_conflict");CHECK(!fstatat(root,".complete.pending",&s,0)&&s.st_size==7);CHECK(!unlinkat(root,".complete.pending",0));
    CHECK(!package_publish(&c,&out));CHECK(!unlinkat(root,"complete",0));CHECK(!symlinkat("payload/model",root,"complete"));failed(&c,"unsafe_package_publication");CHECK(!unlinkat(root,"complete",0));
    CHECK(!unlinkat(root,"publish.lock",0));CHECK(!unlinkat(payload,"model",0));CHECK(!close(payload));CHECK(!unlinkat(root,"payload",AT_REMOVEDIR));CHECK(!close(root));CHECK(!rmdir(base));free(index);
    printf("PASS package publication: %u checks; gated complete records, compatibility, integrity, lease, cancellation and foreign-state preservation; no activation claim\n",checks);return 0;
}
