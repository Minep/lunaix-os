#ifndef __LUNAIX_I386_ASM_H
#define __LUNAIX_I386_ASM_H

#define KCODE_SEG 0x08
#define KDATA_SEG 0x10
#define UCODE_SEG 0x1B
#define UDATA_SEG 0x23
#define TSS_SEG 0x28

#define tss_esp0_off 4

#ifndef __ASM__
#include <lunaix/types.h>
struct x86_tss
{
  u32_t link;
  u32_t esp0;
  u16_t ss0;
  u8_t __padding[94];
} __attribute__((packed));

void tss_update_esp(u32_t esp0);
#endif

#endif /* __LUNAIX_I386_ASM_H */
