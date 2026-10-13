#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "artifact.h"
#include "sha256.h"
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <time.h>

static int same_file(const struct stat *a,const struct stat *b) {
    return a->st_dev==b->st_dev && a->st_ino==b->st_ino &&
           a->st_size==b->st_size && a->st_mode==b->st_mode &&
#ifdef __APPLE__
           a->st_mtimespec.tv_sec==b->st_mtimespec.tv_sec &&
           a->st_mtimespec.tv_nsec==b->st_mtimespec.tv_nsec &&
           a->st_ctimespec.tv_sec==b->st_ctimespec.tv_sec &&
           a->st_ctimespec.tv_nsec==b->st_ctimespec.tv_nsec;
#else
           a->st_mtim.tv_sec==b->st_mtim.tv_sec &&
           a->st_mtim.tv_nsec==b->st_mtim.tv_nsec &&
           a->st_ctim.tv_sec==b->st_ctim.tv_sec &&
           a->st_ctim.tv_nsec==b->st_ctim.tv_nsec;
#endif
}

const char *artifact_verify(int fd,uint64_t expected_size,
                            uint64_t ceiling,const char *expected) {
    return artifact_verify_controlled(fd,expected_size,ceiling,expected,NULL,NULL);
}

const char *artifact_verify_controlled(int fd,uint64_t expected_size,
    uint64_t ceiling,const char *expected,
    const char *(*interrupted)(void *),void *context) {
    if(!expected || strlen(expected)!=64 || !expected_size ||
       !ceiling || expected_size>ceiling || expected_size>INT64_MAX)
        return "invalid_artifact_configuration";
    for(size_t i=0;i<64;i++)
        if(!((expected[i]>='0'&&expected[i]<='9') ||
             (expected[i]>='a'&&expected[i]<='f')))
            return "invalid_artifact_configuration";
    struct stat before,after;
    if(fstat(fd,&before))return "artifact_unavailable";
    if(!S_ISREG(before.st_mode) || before.st_size<0)
        return "unsupported_artifact";
    if((uint64_t)before.st_size>ceiling)return "artifact_byte_limit";
    if((uint64_t)before.st_size!=expected_size)return "artifact_size_mismatch";
    unsigned char buffer[65536];Sha256 hash;sha256_init(&hash);
    struct timespec started,now;
    if(clock_gettime(CLOCK_MONOTONIC,&started))return "artifact_read_failed";
    uint64_t offset=0;
    while(offset<expected_size) {
        const char *control=interrupted?interrupted(context):NULL;
        if(control)return control;
        if(clock_gettime(CLOCK_MONOTONIC,&now))return "artifact_read_failed";
        if(now.tv_sec-started.tv_sec>=600)return "artifact_deadline_exceeded";
        size_t want=sizeof(buffer);
        if(expected_size-offset<want)want=(size_t)(expected_size-offset);
        ssize_t n=pread(fd,buffer,want,(off_t)offset);
        if(n<0 && errno==EINTR)continue;
        if(n<0)return "artifact_read_failed";
        if(!n)return "artifact_changed";
        sha256_update(&hash,buffer,(size_t)n);offset+=(uint64_t)n;
    }
    const char *control=interrupted?interrupted(context):NULL;
    if(control)return control;
    if(fstat(fd,&after) || !same_file(&before,&after))return "artifact_changed";
    unsigned char digest[32];char actual[65];
    sha256_final(&hash,digest);sha256_hex(digest,actual);
    return strcmp(actual,expected)?"artifact_checksum_mismatch":NULL;
}
