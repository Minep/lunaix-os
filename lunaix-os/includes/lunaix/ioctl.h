#ifndef __LUNAIX_IOCTL_H
#define __LUNAIX_IOCTL_H

#include <lunaix/syscall.h>

#define IOREQ(cmd, arg_num) ((((cmd)&0xffff) << 8) | ((arg_num)&0xff))

#define IOCMD(req) ((req) >> 8)

#define IOARGNUM(req) ((req)&0xff)

#define TIOCGPGRP IOREQ(1, 0)
#define TIOCSPGRP IOREQ(1, 1)
#define TIOCCLSBUF IOREQ(2, 0)
#define TIOCFLUSH IOREQ(3, 0)

__LXSYSCALL2_VARG(int, ioctl, int, fd, int, req);

#endif /* __LUNAIX_IOCTL_H */
