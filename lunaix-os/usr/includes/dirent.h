#ifndef __LUNAIX_SYS_DIRENT_H
#define __LUNAIX_SYS_DIRENT_H

#include <sys/dirent_defs.h>

typedef struct
{
    int dirfd;
    int prev_res;
} DIR;

struct dirent
{
    unsigned char d_type;
    char d_name[256];
};

DIR*
opendir(const char* dir);

struct dirent*
readdir(DIR* dir);

#endif /* __LUNAIX_DIRENT_H */
