#ifndef SAMOSA_INPUT_IDENTITY_H
#define SAMOSA_INPUT_IDENTITY_H

#include <sys/stat.h>

static inline int inference_same_input(const struct stat *before, const struct stat *now) {
    if (before->st_dev != now->st_dev || before->st_ino != now->st_ino ||
        before->st_mode != now->st_mode || before->st_size != now->st_size)
        return 0;
#ifdef __APPLE__
    return before->st_mtimespec.tv_sec == now->st_mtimespec.tv_sec &&
           before->st_mtimespec.tv_nsec == now->st_mtimespec.tv_nsec &&
           before->st_ctimespec.tv_sec == now->st_ctimespec.tv_sec &&
           before->st_ctimespec.tv_nsec == now->st_ctimespec.tv_nsec;
#else
    return before->st_mtim.tv_sec == now->st_mtim.tv_sec &&
           before->st_mtim.tv_nsec == now->st_mtim.tv_nsec &&
           before->st_ctim.tv_sec == now->st_ctim.tv_sec &&
           before->st_ctim.tv_nsec == now->st_ctim.tv_nsec;
#endif
}

#endif
