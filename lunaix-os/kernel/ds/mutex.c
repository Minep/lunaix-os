#include "asm/atom_ops.h"
#include "lunaix/compiler.h"
#include <lunaix/ds/mutex.h>
#include <lunaix/process.h>
#include <lunaix/kpreempt.h>

#define __do_lock(mutext)                             \
    ({                                                \
        atom_inci(&mutex->lk);                        \
	if (__current)	                              \
            atom_sti(&mutex->owner, __current->pid);  \
    })

static inline bool must_inline
__mutex_check_owner(mutex_t* mutex)
{
    return !__current || mutex->owner == __current->pid;
}

static inline void must_inline
__mutex_lock_pure(mutex_t* mutex)
{
    while (atom_ldi(&mutex->lk)) {
        preempt_current();
    }
    
    __do_lock(mutex);
}

static inline void must_inline
__mutex_lock(mutex_t* mutex)
{
    not_preemptable(__mutex_lock_pure(mutex));
}

static inline void must_inline
__mutex_unlock(mutex_t* mutex)
{
    not_preemptable({
        if (__mutex_check_owner(mutex)) {
	    atom_deci(&mutex->lk);
	}
    });
}

void
mutex_lock(mutex_t* mutex)
{
    __mutex_lock(mutex);
}

void
mutex_unlock(mutex_t* mutex)
{
    __mutex_unlock(mutex);
}

void
mutex_unlock_for(mutex_t* mutex, pid_t pid)
{
    not_preemptable({
        if (mutex->owner == pid && atom_ldi(&mutex->lk)) {
            atom_deci(&mutex->lk);
        }
    });
}

void
mutex_lock_nested(mutex_t* mutex)
{
    not_preemptable({
        if (atom_ldi(&mutex->lk) && __mutex_check_owner(mutex)) {
            atom_inci(&mutex->lk);
	}
        else {
            __mutex_lock_pure(mutex);
        }
    });
}

void
mutex_unlock_nested(mutex_t* mutex)
{
    not_preemptable({
        if (atom_ldi(&mutex->lk) && __mutex_check_owner(mutex)) {
            atom_deci(&mutex->lk);
	}
    });
}

