#include <lunaix/clock.h>
#include <lunaix/device.h>
#include <lunaix/fs.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/process.h>
#include <lunaix/sched.h>
#include <lunaix/spike.h>
#include <lunaix/syscall.h>
#include <lunaix/syscall_utils.h>

#define MAX_POLLER_COUNT 16

static inline void
current_rmiopoll(int pld)
{
    iopoll_remove(__current->pid, &__current->pollctx, pld);
}

static struct iopoller*
iopoll_getpoller(struct iopoll* ctx, int pld)
{
    if (pld < 0 || pld >= MAX_POLLER_COUNT) {
        return NULL;
    }

    return ctx->pollers[pld];
}

static inline int
__do_poll(struct poll_info* pinfo, int pld)
{
    struct iopoller* poller = iopoll_getpoller(&__current->pollctx, pld);
    if (!poller) {
        return 0;
    }

    struct device* dev;
    int evt = 0;

    if ((dev = resolve_device(poller->file_ref->inode->data))) {
        dev->ops.poll(dev);
    } else {
        // TODO handle generic file
        /*
            N.B. In Linux, polling on any of the non-device mapped file cause
           immediate return of poller, in other words, the I/O signal on file is
           always active. Which make it no use on monitoring any file
           modifications. However, polling for such modifications
           must go through inotify_* API. Which is not that elegant as it breaks
           the nice consistency that *poll(2) should have. Let see want can we
           do in Lunaix.
        */
    }

    if (evt < 0) {
        poll_setrevt(pinfo, _POLLERR);
        goto has_err;
    }

    if ((evt = poll_checkevt(pinfo, evt))) {
        poll_setrevt(pinfo, evt);
        goto has_event;
    }

    return 0;

has_err:
    if ((pinfo->flags & _POLLEE_RM_ON_ERR)) {
        current_rmiopoll(pld);
        return 1;
    }

has_event:
    if (!(pinfo->flags & _POLLEE_ALWAYS)) {
        current_rmiopoll(pld);
    }

    return 1;
}

static int
__do_poll_round(struct poll_info* pinfos, int ninfo)
{
    int nc = 0;
    struct v_fd* fd_s;
    struct device* dev;
    for (int i = 0; i < ninfo; i++) {
        struct poll_info* pinfo = &pinfos[i];
        int pld = pinfo->pld;

        if (__do_poll(pinfo, pld)) {
            nc++;
        }
    }

    return nc;
}

static int
__do_poll_all(struct poll_info* pinfo)
{
    for (int i = 0; i < MAX_POLLER_COUNT; i++) {
        if (!__do_poll(pinfo, i)) {
            continue;
        }

        pinfo->pld = i;
        return 1;
    }

    return 0;
}

#define fd2dev(fd) resolve_device((fd)->file->inode->data)

static int
__alloc_pld()
{
    for (size_t i = 0; i < MAX_POLLER_COUNT; i++) {
        if (!__current->pollctx.pollers[i]) {
            return i;
        }
    }

    return -1;
}

static int
__append_pollers(int* ds, int npoller)
{
    int err = 0, nc = 0;
    struct v_fd* fd_s;
    for (int i = 0; i < npoller; i++) {
        int* fd = &ds[i];
        if ((err = vfs_getfd(*fd, &fd_s))) {
            *fd = err;
            nc++;
            continue;
        }

        int pld = iopoll_install(__current->pid, &__current->pollctx, fd_s);
        if (pld < 0) {
            nc++;
        }

        *fd = pld;
    }

    return nc;
}

static void
__wait_until_event()
{
    block_current_thread();
    sched_yieldk();
}

void
iopoll_init(struct iopoll* ctx)
{
    ctx->pollers = vzalloc(sizeof(ptr_t) * MAX_POLLER_COUNT);
    ctx->n_poller = 0;
}

void
iopoll_free(pid_t pid, struct iopoll* ctx)
{
    for (int i = 0; i < MAX_POLLER_COUNT; i++) {
        struct iopoller* poller = ctx->pollers[i];
        if (poller) {
            vfs_pclose(poller->file_ref, pid);
            llist_delete(&poller->evt_listener);
            vfree(poller);
        }
    }
    
    vfree(ctx->pollers);
}

void
iopoll_wake_pollers(poll_evt_q* pollers_q)
{
    struct iopoller *pos, *n;
    llist_for_each(pos, n, pollers_q, evt_listener)
    {
        struct proc_info* proc = get_process(pos->pid);
        if (proc_hanged(proc)) {
            resume_process(proc);
        }

        assert(!proc_terminated(proc));
    }
}

int
iopoll_remove(pid_t pid, struct iopoll* ctx, int pld)
{
    struct iopoller* poller = ctx->pollers[pld];
    if (!poller) {
        return ENOENT;
    }

    // FIXME vfs locking model need to rethink in the presence of threads
    vfs_pclose(poller->file_ref, pid);
    vfree(poller);
    ctx->pollers[pld] = NULL;
    ctx->n_poller--;

    return 0;
}

int
iopoll_install(pid_t pid, struct iopoll* pollctx, struct v_fd* fd)
{
    int pld = __alloc_pld();
    if (pld < 0) {
        return EMFILE;
    }

    struct iopoller* iop = valloc(sizeof(struct iopoller));
    *iop = (struct iopoller){
        .file_ref = fd->file,
        .pid = pid,
    };

    vfs_ref_file(fd->file);
    __current->pollctx.pollers[pld] = iop;
    __current->pollctx.n_poller++;

    struct device* dev;
    if ((dev = fd2dev(fd))) {
        iopoll_listen_on(iop, &dev->pollers);
    } else {
        // TODO handle generic file
    }

    return pld;
}

__DEFINE_LXSYSCALL2(int, pollctl, int, action, va_list, va)
{
    int retcode = 0;
    switch (action) {
        case _SPOLL_ADD: {
            int* ds = va_arg(va, int*);
            int nds = va_arg(va, int);
            retcode = __append_pollers(ds, nds);
        } break;
        case _SPOLL_RM: {
            int pld = va_arg(va, int);
            retcode = iopoll_remove(__current->pid, &__current->pollctx, pld);
        } break;
        case _SPOLL_WAIT: {
            struct poll_info* pinfos = va_arg(va, struct poll_info*);
            int npinfos = va_arg(va, int);
            int timeout = va_arg(va, int);

            time_t t1 = clock_systime() + timeout;
            while (!(retcode == __do_poll_round(pinfos, npinfos))) {
                if (timeout >= 0 && t1 < clock_systime()) {
                    break;
                }
                __wait_until_event();
            }
        } break;
        case _SPOLL_WAIT_ANY: {
            struct poll_info* pinfo = va_arg(va, struct poll_info*);
            int timeout = va_arg(va, int);

            time_t t1 = clock_systime() + timeout;
            while (!(retcode == __do_poll_all(pinfo))) {
                if (timeout >= 0 && t1 < clock_systime()) {
                    break;
                }
                __wait_until_event();
            }
        } break;
        default:
            retcode = EINVAL;
            break;
    }

    return DO_STATUS(retcode);
}