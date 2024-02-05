#ifndef __LUNAIX_IOPOLL_H
#define __LUNAIX_IOPOLL_H

#include <lunaix/device.h>
#include <lunaix/ds/llist.h>

#include <usr/lunaix/poll.h>

struct thread;  // <lunaix/process.h>
struct proc_info;  // <lunaix/process.h>
struct v_fd;    // <lunaix/fs.h>

typedef struct llist_header poll_evt_q;

struct poll_opts
{
    struct pollfd** upoll;
    int upoll_num;
    int timeout;
};

struct iopoller
{
    poll_evt_q evt_listener;
    struct v_file* file_ref;
    struct thread* thread;
};

struct iopoll
{
    struct iopoller** pollers;
    int n_poller;
};

static inline void
iopoll_listen_on(struct iopoller* listener, poll_evt_q* source)
{
    llist_append(source, &listener->evt_listener);
}

static inline void
iopoll_init_evt_q(poll_evt_q* source)
{
    llist_init_head(source);
}

void
iopoll_wake_pollers(poll_evt_q*);

void
iopoll_init(struct iopoll*);

void
iopoll_free(struct proc_info*);

int
iopoll_install(struct thread* thread, struct v_fd* fd);

int
iopoll_remove(struct thread*, int);

static inline void
poll_setrevt(struct poll_info* pinfo, int evt)
{
    pinfo->revents = (pinfo->revents & ~evt) | evt;
}

static inline int
poll_checkevt(struct poll_info* pinfo, int evt)
{
    return pinfo->events & evt;
}

#endif /* __LUNAIX_POLL_H */
