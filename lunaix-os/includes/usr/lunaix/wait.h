#ifndef _LUNAIX_UHDR_WAIT_H
#define _LUNAIX_UHDR_WAIT_H

#define PEXITTERM   0x100
#define PEXITSTOP   0x200
#define PEXITSIG    0x400

#define WNOHANG     1
#define WUNTRACED   2

#define PEXITNUM(flag, code)    (flag | (code & 0xff))
#define WEXITSTATUS(wstatus)    ((wstatus & 0xff))
#define WIFSTOPPED(wstatus)     ((wstatus & PEXITSTOP))
#define WIFSIGNALED(wstatus)    ((wstatus & PEXITSIG))

#define WIFEXITED(wstatus)                                                     \
    ((wstatus & PEXITTERM) && ((char)WEXITSTATUS(wstatus) >= 0))

#endif /* _LUNAIX_UHDR_WAIT_H */
