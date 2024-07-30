#ifndef __LUNAIX_FAILSAFE_H
#define __LUNAIX_FAILSAFE_H

#define STACK_SANITY            0xbeefc0de

#ifndef __ASM__

#include <lunaix/types.h>

static inline bool
check_bootstack_sanity()
{
    return true;
}

static inline void must_inline noret
failsafe_diagnostic() {
    unreachable;
}

#endif

#endif /* __LUNAIX_FAILSAFE_H */
