#ifndef __LUNAIX_SWITCH_H
#define __LUNAIX_SWITCH_H

#define SWITCH_MODE_NORMAL   0
#define SWITCH_MODE_FAST     1
#define SWITCH_MODE_GIVEUP   2

#ifndef __ASM__

#include <lunaix/types.h>

struct signpost_result
{
    int mode;
    ptr_t stack;
};

/**
 * @brief Decide how current thread should perform the context switch
 *        back to it's previously saved context. 
 * 
 *        Return a user stack pointer perform a temporary fast redirected 
 *        context switch. 
 *        No redirection is required if such pointer is null.
 * 
 *        This function might never return if the decision is made to give up
 *        this switching.
 * 
 *        NOTE: This function might have side effects, it can only be
 *         called within the twilight zone of context restore. (after entering
 *         do_switch and before returning from exception)
 * 
 * @return ptr_t
 */
ptr_t
switch_signposting();

static inline void
redirect_switch(struct signpost_result* res, ptr_t stack)
{
    res->mode  = SWITCH_MODE_FAST;
    res->stack = stack;
}

static inline void
continue_switch(struct signpost_result* res)
{
    res->mode  = SWITCH_MODE_NORMAL;
    res->stack = 0;
}

static inline void
giveup_switch(struct signpost_result* res)
{
    res->mode  = SWITCH_MODE_GIVEUP;
    res->stack = 0;
}

#endif
#endif /* __LUNAIX_SWITCH_H */
