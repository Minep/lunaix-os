#ifndef _LUNAIX_UHDR_SYS_IOCTL_H
#define _LUNAIX_UHDR_SYS_IOCTL_H

#define IOREQ(cmd, arg_num) ((((cmd) & 0xffff) << 8) | ((arg_num) & 0xff))

#define IOCMD(req) ((req) >> 8)

#define IOARGNUM(req) ((req) & 0xff)

#define DEVIOIDENT IOREQ(-1, 1)

#define TIOCGPGRP IOREQ(1, 0)
#define TIOCSPGRP IOREQ(1, 1)
#define TIOCCLSBUF IOREQ(2, 0)
#define TIOCFLUSH IOREQ(3, 0)

#define RTCIO_IUNMSK IOREQ(1, 0)
#define RTCIO_IMSK IOREQ(2, 0)
#define RTCIO_SETFREQ IOREQ(3, 1)
#define RTCIO_GETINFO IOREQ(4, 1)
#define RTCIO_SETDT IOREQ(5, 1)

#define TIMERIO_GETINFO IOREQ(1, 0)

#endif /* _LUNAIX_UHDR_IOCTL_H */
