#ifndef __LUNAIX_SPIN_H
#define __LUNAIX_SPIN_H

#include <lunaix/types.h>

struct spinlock
{
    volatile bool flag;
};

typedef struct spinlock spinlock_t;

/*
    TODO we might use our own construct for atomic ops
         But we will do itlater, currently this whole 
         kernel is on a single long thread of fate, 
         there won't be any hardware concurrent access 
         happened here.
*/

static inline void
spinlock_init(spinlock_t* lock)
{
    lock->flag = false;
}

static inline bool spinlock_try_acquire(spinlock_t* lock)
{
    if (lock->flag){
        return false;
    }

    return (lock->flag = true);
}

static inline void spinlock_acquire(spinlock_t* lock)
{
    while (lock->flag);
    lock->flag = true;
}

static inline void spinlock_release(spinlock_t* lock)
{
    lock->flag = false;
}

#define DEFINE_SPINLOCK_OPS(type, lock_accessor)                            \
    static inline void lock(type obj) { spinlock_acquire(&obj->lock_accessor); }    \
    static inline void unlock(type obj) { spinlock_release(&obj->lock_accessor); }    

#endif /* __LUNAIX_SPIN_H */
