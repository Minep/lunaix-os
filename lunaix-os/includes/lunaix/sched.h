#ifndef __LUNAIX_SCHEDULER_H
#define __LUNAIX_SCHEDULER_H

#define SCHED_TIME_SLICE 300

#define PROC_TABLE_SIZE 8192
#define MAX_PROCESS (PROC_TABLE_SIZE / sizeof(uintptr_t))

struct scheduler
{
    struct proc_info** _procs;
    int procs_index;
    unsigned int ptable_len;
};

void
sched_init();

void
schedule();

void
sched_yieldk();

#endif /* __LUNAIX_SCHEDULER_H */
