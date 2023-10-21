#include "vga.h"

typedef volatile u8_t vga_reg8;

#define regaddr(type, i, vga)                                                  \
    ((vga_reg8*)(vga_gfx_regmap[type][i] + (vga)->io_off))

#define arx_reset(v) (void)(*regaddr(VGA_STATX, 1, v))

// Port IO Address - 0x3C0
static u32_t vga_gfx_regmap[][2] = {
    [VGA_ARX] = { 0x0, 0x1 },    [VGA_SRX] = { 0x04, 0x05 },
    [VGA_GRX] = { 0x0E, 0x0F },  [VGA_CRX] = { 0x14, 0x15 },
    [VGA_DACX] = { 0x08, 0x09 }, [VGA_MISCX] = { 0x02, 0x0C },
    [VGA_STATX] = { 0x02, 0x1A }
};

static u32_t
vga_mmio_read(struct vga* v, u32_t type, u32_t index, u32_t mask)
{
    vga_reg8* reg_ix = regaddr(type, VGA_REG_X, v);
    vga_reg8* reg_rd = regaddr(type, VGA_REG_D, v);
    *reg_ix = (u8_t)index;
    return ((u32_t)*reg_rd) & mask;
}

static void
vga_mmio_write(struct vga* v, u32_t type, u32_t index, u32_t val, u32_t mask)
{
    index = index & 0xff;
    val = val & mask;
    vga_reg8* reg_ix = regaddr(type, VGA_REG_X, v);
    if (type == VGA_ARX) {
        // reset ARX filp-flop
        arx_reset(v);
        *reg_ix = ((u8_t)index) | 0x20;
        *reg_ix = val;
        return;
    } else if (type == VGA_MISCX) {
        *reg_ix = val;
        return;
    }

    *((u16_t*)reg_ix) = (u16_t)(((val) << 8) | index);
}

static void
vga_mmio_set_regs(struct vga* v, u32_t type, size_t off, u32_t* seq, size_t len)
{
    vga_reg8* reg_ix = regaddr(type, VGA_REG_X, v);
    if (type == VGA_ARX) {
        arx_reset(v);
        for (size_t i = 0; i < len; i++) {
            *reg_ix = (u8_t)(off + i) | 0x20;
            *reg_ix = seq[i];
        }

    } else {
        for (size_t i = 0; i < len; i++) {
            *((u16_t*)reg_ix) = (u16_t)((seq[i] << 8) | ((off + i) & 0xff));
        }
    }
}

static void
vga_mmio_set_dacp(struct vga* v)
{
#define R(c) (u8_t)(((c) & 0xff0000) >> 16)
#define G(c) (u8_t)(((c) & 0x00ff00) >> 8)
#define B(c) (u8_t)(((c) & 0x0000ff))

    vga_reg8* reg_ix = regaddr(VGA_DACX, VGA_REG_X, v);
    vga_reg8* reg_dat = regaddr(VGA_DACX, VGA_REG_D, v);

    u32_t* color_map = v->lut.colors;

    *reg_ix = 0;
    for (size_t i = 0; i < v->lut.len; i++) {
        u32_t color = color_map[i];
        *reg_dat = R(color);
        *reg_dat = G(color);
        *reg_dat = B(color);
    }
}

struct vga_regops vga_default_mmio_ops = { .read = vga_mmio_read,
                                           .write = vga_mmio_write,
                                           .set_seq = vga_mmio_set_regs,
                                           .set_dac_palette =
                                             vga_mmio_set_dacp };