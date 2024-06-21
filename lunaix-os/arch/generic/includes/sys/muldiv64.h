#ifndef __LUNAIX_ARCH_MULDIV64_H
#define __LUNAIX_ARCH_MULDIV64_H


#include <lunaix/spike.h>
#include <lunaix/types.h>

u64_t
udiv64(u64_t n, unsigned int base);

unsigned int
umod64(u64_t n, unsigned int base);


#endif /* __LUNAIX_ARCH_MULDIV64_H */
