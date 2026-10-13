#ifndef SAMOSA_PRIVATE_KEY_H
#define SAMOSA_PRIVATE_KEY_H
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
static inline void private_key_clear(char key[130]){
    volatile unsigned char *p=(volatile unsigned char *)key;for(size_t i=0;i<130;i++)p[i]=0;
}
static inline int private_key_read(const char *path,char key[130]){
    private_key_clear(key);
    if(!path||path[0]!='/'||strlen(path)>4095)return 0;
    int fd=open(path,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);if(fd<0)return 0;
    struct stat before,after,named;
    int valid=!fstat(fd,&before)&&S_ISREG(before.st_mode)&&before.st_uid==geteuid()&&!(before.st_mode&077)&&before.st_nlink==1&&before.st_size>0&&before.st_size<=129;
    size_t used=0;if(valid)while(used<(size_t)before.st_size){ssize_t n=pread(fd,key+used,(size_t)before.st_size-used,(off_t)used);
        if(n<0&&errno==EINTR)continue;
        if(n<=0){valid=0;break;}used+=(size_t)n;}
    valid=valid&&!fstat(fd,&after)&&!lstat(path,&named)&&S_ISREG(named.st_mode)&&after.st_uid==geteuid()&&!(after.st_mode&077)&&after.st_nlink==1&&
        before.st_dev==after.st_dev&&before.st_ino==after.st_ino&&before.st_size==after.st_size&&before.st_mtime==after.st_mtime&&before.st_ctime==after.st_ctime&&
        named.st_dev==after.st_dev&&named.st_ino==after.st_ino;
#ifdef __APPLE__
    valid=valid&&before.st_mtimespec.tv_nsec==after.st_mtimespec.tv_nsec&&before.st_ctimespec.tv_nsec==after.st_ctimespec.tv_nsec;
#else
    valid=valid&&before.st_mtim.tv_nsec==after.st_mtim.tv_nsec&&before.st_ctim.tv_nsec==after.st_ctim.tv_nsec;
#endif
    close(fd);if(valid&&used&&key[used-1]=='\n')used--;
    valid=valid&&used>0&&used<=128;
    if(valid)for(size_t i=0;i<used;i++)if(!key[i]||(!isalnum((unsigned char)key[i])&&!strchr("-_.",key[i]))){valid=0;break;}
    if(!valid){private_key_clear(key);return 0;}key[used]=0;return 1;
}
#endif
