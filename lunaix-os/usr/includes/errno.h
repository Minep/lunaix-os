#ifndef __LUNAIX_SYS_ERRNO_H
#define __LUNAIX_SYS_ERRNO_H

#define EINVAL -(6)
#define EINTR -(7)
#define EMFILE -8
#define ENOENT -9
#define ENAMETOOLONG -10
#define ENOTDIR -11
#define EEXIST -12
#define EBADF -13
#define ENOTSUP -14
#define EIO -15
#define ELOOP -16
#define ENOTEMPTY -17
#define EROFS -18
#define EISDIR -19
#define EBUSY -20
#define EXDEV -21
#define ENODEV -22
#define ERANGE -23
#define ENOMEM -(3)
#define ENOTDEV -24
#define EOVERFLOW -25
#define ENOTBLK -26
#define ENOEXEC -27
#define E2BIG -28

int
geterrno();

#define errno (geterrno())

#endif /* __LUNAIX_ERRNO_H */
