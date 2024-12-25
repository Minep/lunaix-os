#ifndef __LUNAIX_V3_H
#define __LUNAIX_V3_H

#include <asm/aa64_gic.h>

#include "gic-common.h"

#define PAGE_64K        (1 << 16)
#define GIC_FRAME_SIZE  PAGE_64K

union v3_rdbase_map
{
    FIELD_AT(u32_t, ctlr,           0x0000);
    FIELD_AT(u32_t, iidr,           0x0004);
    FIELD_AT(u64_t, typer,          0x0008);

    // LPI
    FIELD_AT(u64_t, setlpir,        0x0040);
    FIELD_AT(u64_t, clrlpir,        0x0048);

    FIELD_AT(u64_t, propbaser,      0x0070);
    FIELD_AT(u64_t, pendbaser,      0x0078);
    FIELD_AT(u64_t, invlpir,        0x00a0);
    FIELD_AT(u64_t, invllr,         0x00b0);
    FIELD_AT(u64_t, syncr,          0x00c0);
} compact;

union v3_sgibase_map
{
    FIELD_AT(u32_t, igroupr0,       0x0080);
    ARRAY_AT(u32_t, igroupr_e,      0x0084, 0x0088);

    FIELD_AT(u32_t, isenabler0,     0x0100);
    ARRAY_AT(u32_t, isenabler_e,    0x0104, 0x0108);
    
    FIELD_AT(u32_t, icenabler0,     0x0180);
    ARRAY_AT(u32_t, icenabler_e,    0x0184, 0x0188);

    FIELD_AT(u32_t, ispendr0,       0x0200);
    ARRAY_AT(u32_t, ispendr_e,      0x0204, 0x0208);

    FIELD_AT(u32_t, icpendr0,       0x280);
    ARRAY_AT(u32_t, icpendr_e,      0x284,  0x0288);

    FIELD_AT(u32_t, isactiver0,     0x300);
    ARRAY_AT(u32_t, isactiver_e,    0x304,  0x0308);

    FIELD_AT(u32_t, icactiver0,     0x380);
    ARRAY_AT(u32_t, icactiver_e,    0x384,  0x0388);

    ARRAY_AT(u32_t, ipriorityr,     0x0400, 0x041c);
    ARRAY_AT(u32_t, ipriorityr_e,   0x0420, 0x045c);

    FIELD_AT(u32_t, icfgr0,         0x0c00);
    FIELD_AT(u32_t, icfgr1,         0x0c04);
    ARRAY_AT(u32_t, icfgr_e,        0x0c08, 0x0c14);

    FIELD_AT(u32_t, igrpmodr,       0x0d00);
    ARRAY_AT(u32_t, igrpmodr_e,     0x0d04, 0x0d08);

    FIELD_AT(u32_t, inmir,          0x0f80);
    ARRAY_AT(u32_t, inmir_e,        0x0f84, 0x0ffc);
} compact;

union v3_gicd_map
{
    FIELD_AT(u32_t, ctlr,         0x0000        );
    FIELD_AT(u32_t, typer,        0x0004        );
    FIELD_AT(u32_t, iidr,         0x0008        );

    FIELD_AT(u32_t, setspi_nsr,   0x0040        );
    FIELD_AT(u32_t, clrspi_nsr,   0x0048        );
    FIELD_AT(u32_t, setspi_sr,    0x0050        );
    FIELD_AT(u32_t, clrspi_sr,    0x0058        );

    // SPI
    ARRAY_AT(u32_t, igroupr,      0x0084, 0x00fc);
    ARRAY_AT(u32_t, isenabler,    0x0100, 0x017c);
    ARRAY_AT(u32_t, icenabler,    0x0180, 0x01fc);
    ARRAY_AT(u32_t, ispendr,      0x0200, 0x027c);
    ARRAY_AT(u32_t, icpendr,      0x0280, 0x02fc);
    ARRAY_AT(u32_t, isactiver,    0x0300, 0x037c);
    ARRAY_AT(u32_t, icactiver,    0x0380, 0x03fc);
    ARRAY_AT(u32_t, ipriorityr,   0x0400, 0x07f8);
    ARRAY_AT(u32_t, itargetsr,    0x0800, 0x0bf8);
    ARRAY_AT(u32_t, icfgr,        0x0c00, 0x0cfc);
    ARRAY_AT(u32_t, igrpmodr,     0x0d00, 0x0d7c);
    ARRAY_AT(u32_t, inmir,        0x0f80, 0x0ffc);

    // eSPI
    ARRAY_AT(u32_t, igroupr_e,    0x1000, 0x107c);
    ARRAY_AT(u32_t, isenabler_e,  0x1200, 0x127c);
    ARRAY_AT(u32_t, icenabler_e,  0x1400, 0x147c);
    ARRAY_AT(u32_t, ispendr_e,    0x1600, 0x167c);
    ARRAY_AT(u32_t, icpendr_e,    0x1800, 0x187c);
    ARRAY_AT(u32_t, isactiver_e,  0x1a00, 0x1a7c);
    ARRAY_AT(u32_t, icactiver_e,  0x1c00, 0x1c7c);
    ARRAY_AT(u32_t, ipriorityr_e, 0x2000, 0x23fc);
    ARRAY_AT(u32_t, icfgr_e,      0x3000, 0x30fc);
    ARRAY_AT(u32_t, igrpmodr_e,   0x3400, 0x347c);
    ARRAY_AT(u32_t, inmir_e,      0x3b00, 0x3b7c);

    ARRAY_AT(u64_t, irouter,      0x6100, 0x7fd8);
    ARRAY_AT(u64_t, irouter_e,    0x8000, 0x9ffc);
} compact;

union v3_gicc_regs
{
    FIELD_AT(u32_t, ctlr,         0x0000        );
    FIELD_AT(u32_t, pmr,          0x0004        );
    FIELD_AT(u32_t, bpr,          0x0008        );
    FIELD_AT(u32_t, iar,          0x000c        );
    FIELD_AT(u32_t, eoir,         0x0010        );
    FIELD_AT(u32_t, ppr,          0x0014        );

    FIELD_AT(u32_t, iidr,         0x00fc        );
    FIELD_AT(u32_t, dir,          0x1000        );
} compact;

union v3_its_regs
{
    FIELD_AT(u32_t, ctlr,         0x0000        );
    FIELD_AT(u32_t, iidr,         0x0004        );
    FIELD_AT(u32_t, typer,        0x0008        );
    FIELD_AT(u64_t, cbaser,       0x0080        );
    FIELD_AT(u64_t, cwriter,      0x0088        );
    FIELD_AT(u64_t, creadr,       0x0090        );
    ARRAY_AT(u64_t, basern,       0x0100, 0x0138);
} compact;

union v3_its_translation
{
    FIELD_AT(u32_t, translater,   0x0040        );
} compact;

union v3_reg_map
{
    ptr_t reg_ptrs;
    union v3_rdbase_map*    red;
    union v3_sgibase_map*   red_sgi;
    union v3_gicd_map*      distr;
    union v3_gicc_map*      icc;
};

struct v3_pe
{
    ptr_t rd_base;
    
    union {
        ptr_t red_ptr;
        union v3_rdbase_map* gicr;
    };

    union {
        ptr_t sgi_ptr;
        union v3_rdbase_map* gicr_sgi;
    };

    union {
        struct {
            bool direct_lpi:1;
        };
        int flags;
    };

    union {
        ptr_t lpi_tab_ptr;
        u8_t* lpi_tab;
    };

    struct gic_pe* pe;
};

struct v3_itt_alloc
{
    ptr_t page;
    u8_t free_map[PAGE_SIZE / 256 / 8];
};

struct v3_its_cmd
{
    union {
        struct {
            u64_t dw0;
            u64_t dw1;
            u64_t dw2;
            u64_t dw3;
        };
        u64_t dws[4];
    };
} compact;

struct v3_its
{
    union {
        ptr_t base_ptr;
        union v3_its_regs* gits;
    };

    union {
        ptr_t trns_phys_ptr;
        union v3_its_translation* gtrn;
    };

    struct v3_itt_alloc itt_alloc;

    size_t cmdq_size;
    struct v3_its_cmd *cmdq;

    struct gic* gic;
};


struct v3_distr
{
    union {
        ptr_t dist_ptr;
        union v3_gicd_map* gicd;
    };

    struct gic* gic;
};

#define v3distr(gic)  ((struct v3_distr*)(gic)->impl)
#define v3pe(pe)  ((struct v3_pe*)(pe)->impl)

#define v3distr_class(class)  v3distr(gic_global_context_of(class))
#define v3pe_class(local_class)  v3distr(gic_local_context_of(local_class))

#endif /* __LUNAIX_V3_H */
