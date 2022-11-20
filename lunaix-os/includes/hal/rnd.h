#ifndef __LUNAIX_RND_H
#define __LUNAIX_RND_H

#include <lunaix/types.h>

static inline void
rnd_fill(void* data, size_t len)
{
    asm volatile("1:\n"
                 "rdrand %%eax\n"
                 "movl %%eax, (%0)\n"
                 "addl $4, %%eax\n"
                 "subl $4, %1\n"
                 "jnz 1b" ::"r"((ptr_t)data),
                 "r"((len & ~0x3))
                 : "%al");
}

int
rnd_is_supported();

#endif /* __LUNAIX_RND_H */
