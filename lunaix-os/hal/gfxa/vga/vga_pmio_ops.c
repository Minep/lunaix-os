#include "vga.h"

static u32_t vga_gfx_regmap[][2] = {
    [VGA_ARX] = { 0x3C0, 0x3C1 },  [VGA_SRX] = { 0x3C4, 0x3C5 },
    [VGA_GRX] = { 0x3CE, 0x3CF },  [VGA_CRX] = { 0x3D4, 0x3D5 },
    [VGA_DACX] = { 0x3C8, 0x3C9 }, [VGA_MISCX] = { 0x3CC, 0x3C2 }
};

// TODO