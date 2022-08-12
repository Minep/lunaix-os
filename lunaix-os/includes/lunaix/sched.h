#ifndef __LUNAIX_SCHEDULER_H
#define __LUNAIX_SCHEDULER_H

#define SCHED_TIME_SLICE 300

struct scheduler
{
    struct proc_info* _procs;
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
