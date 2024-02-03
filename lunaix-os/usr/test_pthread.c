#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

static void* 
__print_and_sleep_randsec(void* value)
{
    pthread_t tid = pthread_self();
    printf("thread %d: gets number %d\n", tid, (int)value);
    
    int fd = open("/dev/rand", O_RDONLY | O_DIRECT);
    if (fd < 0) {
        printf("thread %d: failed to get random source: %d\n", tid, errno);
        return NULL;
    }

    unsigned char rand = 0;
    if (read(fd, &rand, sizeof(rand)) != 1) {
        printf("thread %d: failed to get random number: %d\n", tid, errno);
        close(fd);
        return NULL;
    }

    rand = rand % 30;

    printf("thread %d: going to sleep %ds\n", tid, rand);
    sleep(rand);
    close(fd);
    return NULL;
}

static void* 
__print_and_sleep(void* value)
{
    pthread_t tid = pthread_self();
    printf("thread %d: gets number %d\n", tid, (int)value);

    sleep(1);
    return NULL;
}

int __counter_shared = 0;

static void* 
__inc_number(void* value)
{
    for (int i = 0; i < 10000; i++)
    {
        __counter_shared++;
    }
    return NULL;
}

/*
    Test cases
*/

static void
pthread_test_rand_sleep(int param)
{
    int err;
    pthread_t created;
    for (int i = 0; i < param; i++)
    {
        err = pthread_create(&created, NULL, __print_and_sleep_randsec, (void*)i);
        if (err) {
            printf("unable to create thread: %d\n", err);
        }

        if((err = pthread_detach(created))) {
            printf("failed to detach: %d\n", err);
        }
    }

    // wait for max 30 seconds
    sleep(30);
}

static void
pthread_test_join(int param)
{
    int err;
    pthread_t created;
    void* v;
    for (int i = 0; i < param; i++)
    {
        err = pthread_create(&created, NULL, __print_and_sleep, (void*)i);
        if (err) {
            printf("unable to create thread: %d\n", err);
        }

        pthread_join(created, &v);
    }
}


static void
pthread_test_shared_race(int param)
{
    __counter_shared = 0;

    int err;
    pthread_t created;
    for (int i = 0; i < param; i++)
    {
        err = pthread_create(&created, NULL, __inc_number, (void*)i);
        if (err) {
            printf("unable to create thread: %d\n", err);
        }

        if((err = pthread_detach(created))) {
            printf("failed to detach: %d\n", err);
        }
    }

    printf("counter val: %d", __counter_shared);
}

#define run_test(testn, note, ...)                  \
    do {                                            \
        printf("** [%s] test start\n", note);       \
        pthread_test_##testn(__VA_ARGS__);      \
        printf("** [%s] test completed\n");         \
    } while (0)

int main()
{
    run_test(rand_sleep, "rand_sleep_thread5", 5);
    run_test(rand_sleep, "rand_sleep_thread10", 10);
    run_test(rand_sleep, "rand_sleep_thread50", 50);

    run_test(join, "join5", 5);
    run_test(join, "join20", 20);
    
    run_test(shared_race, "shared_race40", 40);

    // TODO test pthread + signal
}