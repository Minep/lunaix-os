#ifndef __LUNAIX_CODE_H
#define __LUNAIX_CODE_H

#define LXPROCFULL -(1)
#define LXHEAPFULL -(2)
#define LXINVLDPTR -(2)
#define LXOUTOFMEM -(3)
#define LXSEGFAULT -(5)
#define EINVAL -(6)

#define EINTR -(7)

#define EMFILE -8
#define ENOENT -9
#define ENAMETOOLONG -10
#define ENOTDIR -11
#define EEXIST -12
#define EBADF -13
#define ENOTSUP -14
#define ENXIO -15
#define ELOOP -16
#define ENOTEMPTY -17
#define EROFS -18
#define EISDIR -19
#define EBUSY -20
#define EXDEV -21
#define ELOOP -22
#define ENODEV -23
#define ERANGE -24
#define ENOMEM LXOUTOFMEM
#define ENOTDEV -25

#endif /* __LUNAIX_CODE_H */
