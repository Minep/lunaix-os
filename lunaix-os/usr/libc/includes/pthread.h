#ifndef __LUNALIBC_PTHREAD_H
#define __LUNALIBC_PTHREAD_H

#include <lunaix/threads.h>

typedef unsigned int pthread_t;

typedef struct {
    // TODO
} pthread_attr_t;

int 
pthread_create(pthread_t* thread,
                const pthread_attr_t* attr,
                void *(*start_routine)(void*), void* arg);

int 
pthread_detach(pthread_t thread);

void 
pthread_exit(void *value_ptr);

int 
pthread_join(pthread_t thread, void **value_ptr);

int 
pthread_kill(pthread_t thread, int sig);

pthread_t pthread_self(void);



#endif /* __LUNALIBC_PTHREAD_H */
