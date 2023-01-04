#ifndef __LUNAIX_SYS_DIRENT_H
#define __LUNAIX_SYS_DIRENT_H

#include <sys/dirent_defs.h>

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

DIR*
opendir(const char* dirp);

int
closedir(DIR* dirp);

struct dirent*
readdir(DIR* dir);

#endif /* __LUNAIX_DIRENT_H */
