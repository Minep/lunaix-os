#ifndef __LUNAIX_V2_H
#define __LUNAIX_V2_H

#include <asm/aa64_gic.h>

#include "gic-common.h"

#define PAGE_64K        (1 << 16)
#define GIC_FRAME_SIZE  PAGE_64K

union v2_rdbase_map
{
    FIELD_AT(u32_t, ctlr,           0x0000);
    FIELD_AT(u32_t, iidr,           0x0004);
    FIELD_AT(u32_t, typer,          0x0008);
} compact;

union v2_sgibase_map
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
} compact;

// Table 4-1 Distributor register map
union v2_gicd_map
{
    FIELD_AT(u32_t, ctlr,         0x0000        );
    FIELD_AT(u32_t, typer,        0x0004        );
    FIELD_AT(u32_t, iidr,         0x0008        );
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
} compact;

// Table 4-2 CPU interface register map
union v2_gicc_regs
{
    FIELD_AT(u32_t, ctlr,         0x0000        );
    FIELD_AT(u32_t, pmr,          0x0004        );
    FIELD_AT(u32_t, bpr,          0x0008        );
    FIELD_AT(u32_t, iar,          0x000c        );
    FIELD_AT(u32_t, eoir,         0x0010        );
    FIELD_AT(u32_t, rpr,          0x0014        );

    FIELD_AT(u32_t, iidr,         0x00fc        );
    FIELD_AT(u32_t, dir,          0x1000        );
} compact;

union v2_reg_map
{
    ptr_t reg_ptrs;
    union v2_rdbase_map*    red;
    union v2_sgibase_map*   red_sgi;
    union v2_gicd_map*      distr;
    union v2_gicc_map*      icc;
};

struct v2_pe
{
    ptr_t rd_base;
    
    union {
        ptr_t sgi_ptr;
        union v2_rdbase_map* gicr_sgi;
    };

    struct gic_pe* pe;
};

struct v2_distr
{
    union {
        ptr_t dist_ptr;
        union v2_gicd_map* gicd;
    };

    struct gic* gic;
};

#define v2distr(gic)  ((struct v2_distr*)(gic)->impl)
#define v2pe(pe)  ((struct v2_pe*)(pe)->impl)

#define v2distr_class(class)  v2distr(gic_global_context_of(class))
#define v2pe_class(local_class)  v2distr(gic_local_context_of(local_class))

#endif /* __LUNAIX_V2_H */
