#ifndef __LUNAIX_VGA_H
#define __LUNAIX_VGA_H

#include <lunaix/types.h>

/* -- Attribute Controller -- */

#define VGA_ARX 0
// Attribute Mode Control Register
#define VGA_AR10 0x10

// Overscan Color Register
#define VGA_AR11 0x11

// Color Plane Enable Register
#define VGA_AR12 0x12

// Horizontal Pixel Panning Register
#define VGA_AR13 0x13

// Color Select Register
#define VGA_AR14 0x14

/* -- Sequencer -- */

#define VGA_SRX 1
// Reset Register
#define VGA_SR00 0x00

// Clocking Mode Register
#define VGA_SR01 0x01

// Map Mask Register
#define VGA_SR02 0x02

// Character Map Select Register
#define VGA_SR03 0x03

// Sequencer Memory Mode Register
#define VGA_SR04 0x04

/* -- GFX Controller -- */

#define VGA_GRX 2
// Set/Reset Register
#define VGA_GR00 0x00

// Enable Set/Reset Register
#define VGA_GR01 0x01

// Color Compare Register
#define VGA_GR02 0x02

// Data Rotate Register
#define VGA_GR03 0x03

// Read Map Select Register
#define VGA_GR04 0x04

// Graphics Mode Register
#define VGA_GR05 0x05

// Miscellaneous Graphics Register
#define VGA_GR06 0x06

// Color Don't Care Register
#define VGA_GR07 0x07

// Bit Mask Register
#define VGA_GR08 0x08

/* -- CRT Controller -- */

#define VGA_CRX 3

// Horizontal Total Register
#define VGA_CR00 0x00

// End Horizontal Display Register
#define VGA_CR01 0x01

// Start Horizontal Blanking Register
#define VGA_CR02 0x02

// End Horizontal Blanking Register
#define VGA_CR03 0x03

// Start Horizontal Retrace Register
#define VGA_CR04 0x04

// End Horizontal Retrace Register
#define VGA_CR05 0x05

// Vertical Total Register
#define VGA_CR06 0x06

// Overflow Register
#define VGA_CR07 0x07

// Preset Row Scan Register
#define VGA_CR08 0x08

// Maximum Scan Line Register
#define VGA_CR09 0x09

// Cursor Start Register
#define VGA_CR0A 0x0A

// Cursor End Register
#define VGA_CR0B 0x0B

// Start Address High Register
#define VGA_CR0C 0x0C

// Start Address Low Register
#define VGA_CR0D 0x0D

// Cursor Location High Register
#define VGA_CR0E 0x0E

// Cursor Location Low Register
#define VGA_CR0F 0x0F

// Vertical Retrace Start Register
#define VGA_CR10 0x10

// Vertical Retrace End Register
#define VGA_CR11 0x11

// Vertical Display End Register
#define VGA_CR12 0x12

// Offset Register
#define VGA_CR13 0x13

// Underline Location Register
#define VGA_CR14 0x14

// Start Vertical Blanking Register
#define VGA_CR15 0x15

// End Vertical Blanking
#define VGA_CR16 0x16

// CRTC Mode Control Register
#define VGA_CR17 0x17

// Line Compare Register
#define VGA_CR18 0x18
#define VGA_CRCOUNT 0x19

/* -- Digital-Analogue Converter -- */

// DAC register
#define VGA_DACX 4

// DAC Data Register
#define VGA_DAC1 0x3C9

// DAC State Register
#define VGA_DAC2 0x3C7

#define VGA_MISCX 5
#define VGA_STATX 6

#define VGA_REG_X 0
#define VGA_REG_D 1

#define VGA_MODE_GFX (0b0001)

struct vga;

struct vga_regops
{
    u32_t (*read)(struct vga*, u32_t type, u32_t index, u32_t mask);
    void (*write)(struct vga*, u32_t type, u32_t index, u32_t val, u32_t mask);
    void (
      *set_seq)(struct vga*, u32_t type, size_t off, u32_t* seq, size_t len);
    void (*set_dac_palette)(struct vga*, u32_t*, size_t);
};

struct vga
{
    ptr_t io_off;
    ptr_t fb;
    size_t fb_sz;
    u32_t options;

    struct
    {
        size_t h_cclk;  // horizontal char clock count;
        size_t v_cclk;  // vertical char clock count;
        size_t pel_dot; // pixel per dot clock
        size_t freq;
        size_t fb_off;
        size_t depth;
    } crt;

    struct vga_regops reg_ops;
};

/**
 * @brief Create a new VGA state
 *
 * @param reg_base base address for register accessing
 * @param fb address to frame buffer
 * @param fb_sz size of usable frame buffer
 * @return struct vga* VGA state
 */
struct vga*
vga_new_state(ptr_t reg_base, ptr_t fb, ptr_t fb_sz);

/**
 * @brief Config VGA active displaying region dimension (resolution)
 *
 * @param width width in pixel counts
 * @param hight height in pixel counts
 * @param freq frame update frequency
 * @param d9 use D9 character clock
 */
void
vga_config_rect(struct vga*, size_t width, size_t hight, size_t freq, int d9);

int
vga_reload_config(struct vga*);

void
vga_update_palette(struct vga* state, u32_t* lut, size_t len);

#endif /* __LUNAIX_VGA_H */
