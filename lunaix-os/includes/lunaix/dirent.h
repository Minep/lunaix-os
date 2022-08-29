#ifndef __LUNAIX_DIRENT_H
#define __LUNAIX_DIRENT_H

#define DIRENT_NAME_MAX_LEN 256

#define DT_FILE 0x0
#define DT_DIR 0x1
#define DT_SYMLINK 0x2
#define DT_PIPE 0x2

struct dirent
{
    unsigned int d_type;
    unsigned int d_offset;
    unsigned int d_nlen;
    char d_name[DIRENT_NAME_MAX_LEN];
};

#endif /* __LUNAIX_DIRENT_H */
