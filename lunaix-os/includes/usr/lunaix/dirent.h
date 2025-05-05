#ifndef _LUNAIX_UHDR_SYS_DIRENT_DEFS_H
#define _LUNAIX_UHDR_SYS_DIRENT_DEFS_H

#define DIRENT_NAME_MAX_LEN 256

#define DT_FILE 0x0
#define DT_DIR 0x1
#define DT_SYMLINK 0x2
#define DT_PIPE 0x3

struct lx_dirent
{
    unsigned int d_type;
    unsigned int d_offset;
    unsigned int d_nlen;
    char d_name[DIRENT_NAME_MAX_LEN];
};

#endif /* _LUNAIX_UHDR_DIRENT_DEFS_H */
