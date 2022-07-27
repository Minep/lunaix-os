#ifndef __LUNAIX_DIRENT_H
#define __LUNAIX_DIRENT_H

struct dirent
{
    unsigned int d_type;
    unsigned int d_offset;
    unsigned int d_nlen;
    char* d_name;
};

#endif /* __LUNAIX_DIRENT_H */
