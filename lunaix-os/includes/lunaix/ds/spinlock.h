#ifndef __LUNAIX_SPIN_H
#define __LUNAIX_SPIN_H

#include <asm/atom_ops.h>
#include <lunaix/types.h>

struct spinlock
{
    bool flag;
};

#define DEFINE_SPINLOCK(name)   \
    struct spinlock name = { .flag = false }

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
    atom_clrf(&lock->flag); 
}

static inline bool spin_try_lock(spinlock_t* lock)
{
    if (atom_ldi(&lock->flag)) {
        return false;
    }
    
    atom_setf(&lock->flag);
    return true;
}

static inline void spin_lock(spinlock_t* lock)
{
    while (atom_ldi(&lock->flag));
    atom_setf(&lock->flag);
}

static inline void spin_unlock(spinlock_t* lock)
{
    atom_clrf(&lock->flag);
}

#define DEFINE_SPINLOCK_OPS(type, name, lock_accessor)                            \
    static inline void lock_##name(type obj) { spin_lock(&obj->lock_accessor); }    \
    static inline void unlock_##name(type obj) { spin_unlock(&obj->lock_accessor); }    

#endif /* __LUNAIX_SPIN_H */
