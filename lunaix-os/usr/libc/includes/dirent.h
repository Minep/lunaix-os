#ifndef __LUNALIBC_SYS_DIRENT_H
#define __LUNALIBC_SYS_DIRENT_H

#include <lunaix/dirent_defs.h>

typedef struct
{
    int dirfd;
    struct lx_dirent _lxd;
} DIR;

struct dirent
{
    unsigned char d_type;
    char d_name[256];
};

extern DIR*
opendir(const char* dirp);

extern int
closedir(DIR* dirp);

extern struct dirent*
readdir(DIR* dir);

extern int
sys_readdir(int fd, struct lx_dirent* dirent);

#endif /* __LUNALIBC_DIRENT_H */
