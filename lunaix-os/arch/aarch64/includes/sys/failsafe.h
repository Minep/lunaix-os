#ifndef __LUNAIX_ARCH_FAILSAFE_H
#define __LUNAIX_ARCH_FAILSAFE_H

#define STACK_SANITY            0xbeefc0de

#ifndef __ASM__

#include <lunaix/types.h>

static inline bool
check_bootstack_sanity()
{
    extern unsigned int __kinit_stack_end[];

    return ( __kinit_stack_end[0] 
           | __kinit_stack_end[1]
           | __kinit_stack_end[2]
           | __kinit_stack_end[3]) == STACK_SANITY;
}

static inline void must_inline noret
failsafe_diagnostic() {
    // TODO
    unreachable;
}

#endif

#endif /* __LUNAIX_FAILSAFE_H */
