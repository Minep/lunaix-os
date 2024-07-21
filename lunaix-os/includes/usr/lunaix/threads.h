#ifndef __LUNAIX_USR_THREADS_H
#define __LUNAIX_USR_THREADS_H

#include "types.h"

struct uthread_param {
    void* th_handler;
    void* arg1;
};

#endif /* __LUNAIX_USR_THREADS_H */
