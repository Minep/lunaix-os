#ifndef __LUNAIX_TSS_H
#define __LUNAIX_TSS_H
#include <stdint.h>

struct x86_tss {
    uint32_t link;
    uint32_t esp0;
    uint16_t ss0;
    uint8_t __padding[94];
} __attribute__((packed));

void tss_update(uint32_t ss0, uint32_t esp0);
 
#endif /* __LUNAIX_TSS_H */
