#ifndef __LUNAIX_SYS_TYPES_H
#define __LUNAIX_SYS_TYPES_H

#undef NULL
#define NULL (void*)0
#define NULLPTR 0

#define PEXITTERM 0x100
#define PEXITSTOP 0x200
#define PEXITSIG 0x400

#define PEXITNUM(flag, code) (flag | (code & 0xff))

#define WNOHANG 1
#define WUNTRACED 2
#define WEXITSTATUS(wstatus) ((wstatus & 0xff))
#define WIFSTOPPED(wstatus) ((wstatus & PEXITSTOP))
#define WIFEXITED(wstatus)                                                     \
    ((wstatus & PEXITTERM) && ((char)WEXITSTATUS(wstatus) >= 0))

#define WIFSIGNALED(wstatus) ((wstatus & PEXITSIG))

typedef signed long ssize_t;

typedef int pid_t;
typedef int tid_t;

typedef __SIZE_TYPE__ size_t;

typedef __SIZE_TYPE__ off_t;

typedef unsigned int ino_t;

typedef struct dev_t
{
    unsigned int meta;
    unsigned int unique;
    unsigned int index;
} dev_t;

#endif /* __LUNAIX_TYPES_H */
