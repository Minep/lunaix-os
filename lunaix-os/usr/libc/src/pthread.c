#include <lunaix/syscall.h>
#include <pthread.h>

static void*
__pthread_routine_wrapper(void *(*start_routine)(void*), void* arg)
{
    void* ret = start_routine(arg);

    do_lunaix_syscall(__SYSCALL_th_exit, ret);

    return ret; // should not reach
}

int 
pthread_create(pthread_t* thread,
                const pthread_attr_t* attr,
                void *(*start_routine)(void*), void* arg)
{
    // FIXME attr currently not used

    struct uthread_info th_info;
    int ret = do_lunaix_syscall(__SYSCALL_th_create, thread, &th_info, __pthread_routine_wrapper, NULL);

    if (ret) {
        return ret;
    }

    // FIXME this is not working for x86_64, need to pass
    //       the parameter by registers, however, this can be
    //       done only in kernel side. Our libc must have
    //       dynamic allocator in order to do achieve this.
    //       we should encapsulate these parameter into struct
    //       and pass it as a single thread param.

    void** th_stack = (void**) th_info.th_stack_top; 
    th_stack[1] = (void*)start_routine;
    th_stack[2] = arg;

    return ret;
}

int 
pthread_detach(pthread_t thread)
{
    return do_lunaix_syscall(__SYSCALL_th_detach, thread);
}

void 
pthread_exit(void *value_ptr)
{
    do_lunaix_syscall(__SYSCALL_th_exit, value_ptr);
}

int 
pthread_join(pthread_t thread, void **value_ptr)
{
    return do_lunaix_syscall(__SYSCALL_th_join, thread, value_ptr);
}

int 
pthread_kill(pthread_t thread, int sig)
{
    return do_lunaix_syscall(__SYSCALL_th_kill, thread, sig);
}

pthread_t 
pthread_self(void)
{
    return do_lunaix_syscall(__SYSCALL_th_self);
}
