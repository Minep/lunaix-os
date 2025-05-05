#ifndef _LUNAIX_UHDR_SYS_TYPES_H
#define _LUNAIX_UHDR_SYS_TYPES_H

typedef int             __lunaix_pid_t;
typedef int             __lunaix_tid_t;
typedef int             __lunaix_uid_t;
typedef int             __lunaix_gid_t;
typedef unsigned long   __lunaix_size_t;
typedef signed long     __lunaix_ssize_t;
typedef unsigned int    __lunaix_ino_t;

typedef struct
{
    unsigned int meta;
    unsigned int unique;
    unsigned int index;
} __lunaix_dev_t;

#endif /* _LUNAIX_UHDR_TYPES_H */
