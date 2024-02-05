#ifndef __LUNAIX_USR_THREADS_H
#define __LUNAIX_USR_THREADS_H

#include "types.h"

struct uthread_info {
    void* th_stack_top;
    size_t th_stack_sz;
};

#endif /* __LUNAIX_USR_THREADS_H */
