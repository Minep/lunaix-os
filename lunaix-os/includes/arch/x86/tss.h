#ifndef __LUNAIX_TSS_H
#define __LUNAIX_TSS_H
#include <lunaix/types.h>

struct x86_tss
{
    u32_t link;
    u32_t esp0;
    uint16_t ss0;
    uint8_t __padding[94];
} __attribute__((packed));

void
tss_update_esp(u32_t esp0);

#endif /* __LUNAIX_TSS_H */
