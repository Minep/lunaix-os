#ifndef __LUNAIX_DIRENT_H
#define __LUNAIX_DIRENT_H

#define DIRENT_NAME_MAX_LEN 256

struct dirent
{
    unsigned int d_type;
    unsigned int d_offset;
    unsigned int d_nlen;
    char d_name[DIRENT_NAME_MAX_LEN];
};

#endif /* __LUNAIX_DIRENT_H */
