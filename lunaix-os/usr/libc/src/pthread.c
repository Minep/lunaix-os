#include <lunaix/syscall.h>
#include <pthread.h>

int 
pthread_create(pthread_t* thread,
                const pthread_attr_t* attr,
                void *(*start_routine)(void*), void* arg)
{
    // FIXME attr currently not used
    int ret;
    struct uthread_param th_param;

    th_param.th_handler = start_routine;
    th_param.arg1 = arg;

    extern void th_trampoline();
    ret = do_lunaix_syscall(__SYSCALL_th_create, thread, 
                            &th_param, th_trampoline);
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
