#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "artifact.h"
#include "sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

static unsigned checks;
typedef struct {unsigned calls,stop;int fd,mutate;} Control;
static const char *controlled(void *context) {
    Control *c=context;
    c->calls++;
    if(c->mutate&&c->calls==2&&pwrite(c->fd,"z",1,0)!=1)return "fixture_write_failed";
    return c->stop&&c->calls==c->stop?"operation_cancelled":NULL;
}
#define CHECK(x) do {checks++;if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void vector(const char *bytes,const char *expected) {
    char digest[65];hash_bytes(bytes,strlen(bytes),digest);CHECK(!strcmp(digest,expected));
}
int main(void) {
    vector("","e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    vector("abc","ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    vector("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
           "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    char path[]="/tmp/samosa-artifact-XXXXXX";int fd=mkstemp(path);CHECK(fd>=0);
    CHECK(write(fd,"abc",3)==3);const char *sha="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    CHECK(!artifact_verify(fd,3,3,sha));CHECK(lseek(fd,0,SEEK_CUR)==3);
    CHECK(!strcmp(artifact_verify(fd,2,3,sha),"artifact_size_mismatch"));
    CHECK(!strcmp(artifact_verify(fd,3,2,sha),"invalid_artifact_configuration"));
    CHECK(!strcmp(artifact_verify(fd,2,2,sha),"artifact_byte_limit"));
    CHECK(!strcmp(artifact_verify(fd,3,3,"bad"),"invalid_artifact_configuration"));
    char wrong[65];memset(wrong,'0',64);wrong[64]=0;
    CHECK(!strcmp(artifact_verify(fd,3,3,wrong),"artifact_checksum_mismatch"));
    CHECK(pwrite(fd,"x",1,0)==1);
    CHECK(!strcmp(artifact_verify(fd,3,3,sha),"artifact_checksum_mismatch"));
    CHECK(!strcmp(artifact_verify(-1,3,3,sha),"artifact_unavailable"));
    int pipes[2];CHECK(!pipe(pipes));
    CHECK(!strcmp(artifact_verify(pipes[0],3,3,sha),"unsupported_artifact"));
    CHECK(!close(pipes[0]));CHECK(!close(pipes[1]));
    CHECK(!ftruncate(fd,0));CHECK(lseek(fd,0,SEEK_SET)==0);
    char block[1000];memset(block,'a',sizeof(block));
    for(int i=0;i<1000;i++)CHECK(write(fd,block,sizeof(block))==(ssize_t)sizeof(block));
    CHECK(!artifact_verify(fd,1000000,1000000,"cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"));
    const char *large_sha="cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0";
    Control control={.stop=1};
    CHECK(!strcmp(artifact_verify_controlled(fd,1000000,1000000,large_sha,controlled,&control),"operation_cancelled"));
    CHECK(control.calls==1);
    control=(Control){.stop=3};
    CHECK(!strcmp(artifact_verify_controlled(fd,1000000,1000000,large_sha,controlled,&control),"operation_cancelled"));
    CHECK(control.calls==3);
    control=(Control){0};
    CHECK(!artifact_verify_controlled(fd,1000000,1000000,large_sha,controlled,&control));
    CHECK(control.calls==17); /* Sixteen bounded reads and the completion check. */
    control=(Control){.stop=17};
    CHECK(!strcmp(artifact_verify_controlled(fd,1000000,1000000,large_sha,controlled,&control),"operation_cancelled"));
    control=(Control){.fd=fd,.mutate=1};
    CHECK(!strcmp(artifact_verify_controlled(fd,1000000,1000000,large_sha,controlled,&control),"artifact_changed"));
    CHECK(!close(fd));CHECK(!unlink(path));
    printf("PASS streamed artifacts: %u checks; SHA vectors, fixed buffer, size/checksum/refusal\n",checks);
    return 0;
}
