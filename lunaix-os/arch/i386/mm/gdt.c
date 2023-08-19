#include <lunaix/types.h>
#include <sys/x86_isa.h>

#define SD_TYPE(x) (x << 8)
#define SD_CODE_DATA(x) (x << 12)
#define SD_DPL(x) (x << 13)
#define SD_PRESENT(x) (x << 15)
#define SD_AVL(x) (x << 20)
#define SD_64BITS(x) (x << 21)
#define SD_32BITS(x) (x << 22)
#define SD_4K_GRAN(x) (x << 23)

#define SEG_LIM_L(x) (x & 0x0ffff)
#define SEG_LIM_H(x) (x & 0xf0000)
#define SEG_BASE_L(x) ((x & 0x0000ffff) << 16)
#define SEG_BASE_M(x) ((x & 0x00ff0000) >> 16)
#define SEG_BASE_H(x) (x & 0xff000000)

#define SEG_DATA_RD 0x00        // Read-Only
#define SEG_DATA_RDA 0x01       // Read-Only, accessed
#define SEG_DATA_RDWR 0x02      // Read/Write
#define SEG_DATA_RDWRA 0x03     // Read/Write, accessed
#define SEG_DATA_RDEXPD 0x04    // Read-Only, expand-down
#define SEG_DATA_RDEXPDA 0x05   // Read-Only, expand-down, accessed
#define SEG_DATA_RDWREXPD 0x06  // Read/Write, expand-down
#define SEG_DATA_RDWREXPDA 0x07 // Read/Write, expand-down, accessed
#define SEG_CODE_EX 0x08        // Execute-Only
#define SEG_CODE_EXA 0x09       // Execute-Only, accessed
#define SEG_CODE_EXRD 0x0A      // Execute/Read
#define SEG_CODE_EXRDA 0x0B     // Execute/Read, accessed
#define SEG_CODE_EXC 0x0C       // Execute-Only, conforming
#define SEG_CODE_EXCA 0x0D      // Execute-Only, conforming, accessed
#define SEG_CODE_EXRDC 0x0E     // Execute/Read, conforming
#define SEG_CODE_EXRDCA 0x0F    // Execute/Read, conforming, accessed

#define SEG_R0_CODE                                                            \
    SD_TYPE(SEG_CODE_EXRD) | SD_CODE_DATA(1) | SD_DPL(0) | SD_PRESENT(1) |     \
      SD_AVL(0) | SD_64BITS(0) | SD_32BITS(1) | SD_4K_GRAN(1)

#define SEG_R0_DATA                                                            \
    SD_TYPE(SEG_DATA_RDWR) | SD_CODE_DATA(1) | SD_DPL(0) | SD_PRESENT(1) |     \
      SD_AVL(0) | SD_64BITS(0) | SD_32BITS(1) | SD_4K_GRAN(1)

#define SEG_R3_CODE                                                            \
    SD_TYPE(SEG_CODE_EXRD) | SD_CODE_DATA(1) | SD_DPL(3) | SD_PRESENT(1) |     \
      SD_AVL(0) | SD_64BITS(0) | SD_32BITS(1) | SD_4K_GRAN(1)

#define SEG_R3_DATA                                                            \
    SD_TYPE(SEG_DATA_RDWR) | SD_CODE_DATA(1) | SD_DPL(3) | SD_PRESENT(1) |     \
      SD_AVL(0) | SD_64BITS(0) | SD_32BITS(1) | SD_4K_GRAN(1)

#define SEG_TSS SD_TYPE(9) | SD_DPL(0) | SD_PRESENT(1)

#define GDT_ENTRY 6

u64_t _gdt[GDT_ENTRY];
u16_t _gdt_limit = sizeof(_gdt) - 1;

void
_set_gdt_entry(u32_t index, u32_t base, u32_t limit, u32_t flags)
{
    _gdt[index] =
      SEG_BASE_H(base) | flags | SEG_LIM_H(limit) | SEG_BASE_M(base);
    _gdt[index] <<= 32;
    _gdt[index] |= SEG_BASE_L(base) | SEG_LIM_L(limit);
}

extern struct x86_tss _tss;

void
_init_gdt()
{
    _set_gdt_entry(0, 0, 0, 0);
    _set_gdt_entry(1, 0, 0xfffff, SEG_R0_CODE);
    _set_gdt_entry(2, 0, 0xfffff, SEG_R0_DATA);
    _set_gdt_entry(3, 0, 0xfffff, SEG_R3_CODE);
    _set_gdt_entry(4, 0, 0xfffff, SEG_R3_DATA);
    _set_gdt_entry(5, (u32_t)&_tss, sizeof(struct x86_tss) - 1, SEG_TSS);
}