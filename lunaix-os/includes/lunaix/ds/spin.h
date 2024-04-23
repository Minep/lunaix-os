#ifndef __LUNAIX_SPIN_H
#define __LUNAIX_SPIN_H

#include <lunaix/types.h>

struct spinlock
{
    volatile bool flag;
};

/*
    TODO we might use our own construct for atomic ops
         But we will do itlater, currently this whole 
         kernel is on a single long thread of fate, 
         there won't be any hardware concurrent access 
         happened here.
*/

static inline bool spin_try_lock(struct spinlock* lock)
{
    if (lock->flag){
        return false;
    }

    return (lock->flag = true);
}

static inline void spin_lock(struct spinlock* lock)
{
    while (lock->flag);
    lock->flag = true;
}

static inline void spin_unlock(struct spinlock* lock)
{
    lock->flag = false;
}

#endif /* __LUNAIX_SPIN_H */
