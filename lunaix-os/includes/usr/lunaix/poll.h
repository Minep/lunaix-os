#ifndef _LUNAIX_UHDR_UPOLL_H
#define _LUNAIX_UHDR_UPOLL_H

struct poll_info
{
    int pld;
    short events;
    short revents;
    int flags;
};

#define _POLLIN (1)
#define _POLLPRI (1 << 1)
#define _POLLOUT (1 << 2)
#define _POLLRDHUP (1 << 3)
#define _POLLERR (1 << 4)
#define _POLLHUP (1 << 5)
#define _POLLNVAL (1 << 6)

#define _SPOLL_ADD 0
#define _SPOLL_RM 1
#define _SPOLL_WAIT 2
#define _SPOLL_WAIT_ANY 3

#define _POLLEE_ALWAYS 1
#define _POLLEE_RM_ON_ERR (1 << 1)

#endif /* _LUNAIX_UHDR_UPOLL_H */
