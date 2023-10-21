#ifndef __LUNAIX_SYS_IOCTL_DEFS_H
#define __LUNAIX_SYS_IOCTL_DEFS_H

#define IOREQ(cmd, arg_num) ((((cmd) & 0xffff) << 8) | ((arg_num) & 0xff))

#define IOCMD(req) ((req) >> 8)

#define IOARGNUM(req) ((req) & 0xff)

#define DEVIOIDENT IOREQ(-1, 1)

#define TIOCGPGRP IOREQ(1, 0)
#define TIOCSPGRP IOREQ(1, 1)
#define TIOCCLSBUF IOREQ(2, 0)
#define TIOCFLUSH IOREQ(3, 0)
#define TIOCPUSH IOREQ(4, 1)
#define TIOCPOP IOREQ(5, 0)
#define TIOCSCDEV IOREQ(6, 1)
#define TIOCGCDEV IOREQ(7, 0)

#define RTCIO_IUNMSK IOREQ(1, 0)
#define RTCIO_IMSK IOREQ(2, 0)
#define RTCIO_SETFREQ IOREQ(3, 1)
#define RTCIO_GETINFO IOREQ(4, 1)
#define RTCIO_SETDT IOREQ(5, 1)

#define TIMERIO_GETINFO IOREQ(1, 0)

#endif /* __LUNAIX_IOCTL_DEFS_H */
