#ifndef __LUNAIX_SCHEDULER_H
#define __LUNAIX_SCHEDULER_H

#include <lunaix/compiler.h>
#include <lunaix/process.h>

#define SCHED_TIME_SLICE 300

#define PROC_TABLE_SIZE 8192
#define MAX_PROCESS (PROC_TABLE_SIZE / sizeof(ptr_t))

struct scheduler
{
    struct proc_info** procs;
    struct llist_header* threads;
    struct llist_header* proc_list;
    struct llist_header sleepers;

    int procs_index;
    int ptable_len;
    int ttable_len;
};

void
sched_init();

void noret
schedule();

void
sched_pass();

void noret
run(struct thread* thread);

void
cleanup_detached_threads();

#endif /* __LUNAIX_SCHEDULER_H */
