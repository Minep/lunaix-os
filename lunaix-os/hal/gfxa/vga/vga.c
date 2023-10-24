/**
 * @file ga_vtm.c
 * @author Lunaixsky (lunaxisky@qq.com)
 * @brief video graphic adapter - text mode with simpiled ansi color code
 * @version 0.1
 * @date 2023-10-06
 *
 * @copyright Copyright (c) 2023
 *
 */

#include <lunaix/compiler.h>
#include <lunaix/device.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/spike.h>
#include <lunaix/status.h>

#include <sys/cpu.h>

#include "vga.h"

#define ALL_FIELDS -1

struct vga*
vga_new_state(ptr_t reg_base, ptr_t fb, ptr_t alloced_fbsz)
{
    struct vga* state = valloc(sizeof(struct vga));
    *state =
      (struct vga){ .fb = fb, .fb_sz = alloced_fbsz, .io_off = reg_base };

    return state;
}

void
vga_config_rect(struct vga* state,
                size_t width,
                size_t height,
                size_t freq,
                int d9)
{
    size_t pel_sz = 8 + !!d9;

    state->crt.h_cclk = width / pel_sz;
    state->crt.v_cclk = height;
    state->crt.freq = freq;

    state->crt.pel_dot = pel_sz;
}

#define RETRACE_GAP 4

static void
vga_crt_update(struct vga* state)
{
#define R_BIT9(x, l) ((((x) & (1 << 9)) >> 9) << (l))
#define R_BIT8(x, l) ((((x) & (1 << 8)) >> 8) << (l))
    struct vga_regops* reg = &state->reg_ops;

    u32_t hdisp = state->crt.h_cclk;
    u32_t vdisp = state->crt.v_cclk;
    u32_t hdisp_end = hdisp - 1;
    u32_t vdisp_end = vdisp - 1;

    // VGA spec said so
    u32_t htotal = hdisp - 5;
    u32_t vtotal = vdisp - 5;

    u32_t hsync_end = (hdisp + RETRACE_GAP) & 0b11111;
    u32_t vsync_end = (vdisp + RETRACE_GAP) & 0b01111;

    reg->write(state, VGA_CRX, VGA_CR11, 0, 1 << 7);
    reg->write(state, VGA_CRX, VGA_CR03, 1 << 7, 1 << 7);

    // set values of first 8 registers. Note, we prefer no H_BLANK region
    u32_t crt_regs[VGA_CRCOUNT] = {
        [VGA_CR00] = htotal, [VGA_CR01] = hdisp - 1,
        [VGA_CR02] = 0xff,   [VGA_CR03] = (1 << 7) | 31,
        [VGA_CR04] = hdisp,  [VGA_CR05] = hsync_end,
        [VGA_CR06] = vtotal
    };

    // overflow: bit 9/8 of vtotal
    crt_regs[VGA_CR07] = R_BIT9(vtotal, 5) | R_BIT8(vtotal, 0);
    // overflow: bit 9/8 of vdisp_end
    crt_regs[VGA_CR07] |= R_BIT9(vdisp_end, 6) | R_BIT8(vdisp_end, 1);
    // overflow: bit 9/8 of vsync_start
    crt_regs[VGA_CR07] |= R_BIT9(vdisp, 7) | R_BIT8(vdisp, 2);

    crt_regs[VGA_CR10] = vdisp; // vsync_start

    // V_SYNC_END, Disable Vertical INTR (GD5446)
    crt_regs[VGA_CR11] = vsync_end | (1 << 5);
    crt_regs[VGA_CR12] = vdisp_end; // vdisp_end

    // no V_BLANK region (v blank start/end = 0x3ff)
    crt_regs[VGA_CR15] = 0xff;
    crt_regs[VGA_CR16] = 0xff;
    crt_regs[VGA_CR09] |= (1 << 5);
    crt_regs[VGA_CR07] |= (1 << 3);

    // no split line (0x3FF)
    crt_regs[VGA_CR18] = 0xff;
    crt_regs[VGA_CR07] |= (1 << 4);
    crt_regs[VGA_CR09] |= (1 << 6);

    // disable text cursor by default
    crt_regs[VGA_CR0A] = (1 << 5);
    // CRB: text cusor end, 0x00
    // CRE-CRF: text cusor location, cleared, 0x00

    // CRC-CRD: screen start address(lo/hi): 0x00, 0x00
    //  i.e., screen content start at the beginning of framebuffer

    if (!(state->options & VGA_MODE_GFX)) {
        // AN Mode, each char heights 16 scan lines
        crt_regs[VGA_CR09] |= 0xf;
    } else {
        // GFX Mode, individual scanline, no repeat
    }

    // framebuffer: 16bit granule
    u32_t divsor = 2 * 2;
    if (state->crt.depth == 256) {
        // if 8 bit color is used, 2 pixels per granule
        divsor *= 2;
    } else if ((state->options & VGA_MODE_GFX)) {
        // otherwise (gfx mode), 4 pixels per granule
        divsor *= 4;
    } // AN mode, one granule per character
    crt_regs[VGA_CR13] = (hdisp * state->crt.pel_dot) / divsor;

    // Timing En, Word Mode, Address Warp,
    crt_regs[VGA_CR17] = (0b10100011);

    reg->set_seq(state, VGA_CRX, 0, crt_regs, VGA_CRCOUNT);
}

void
vga_update_palette(struct vga* state, u32_t* lut, size_t len)
{
    u32_t index[16];
    u32_t clr_len = MIN(len, 16);
    for (size_t i = 0; i < clr_len; i++) {
        index[i] = i;
    }

    state->reg_ops.set_dac_palette(state, lut, len);
    state->reg_ops.set_seq(state, VGA_ARX, 0, index, clr_len);
}

int
vga_reload_config(struct vga* state)
{
    cpu_disable_interrupt();

    int err = 0;
    struct vga_regops* reg = &state->reg_ops;

    u32_t color = state->crt.depth;
    if (!(color == 2 || color == 4 || color == 16 || color == 256)) {
        err = EINVAL;
        goto done;
    }

    if (!(state->options & VGA_MODE_GFX) && color > 256) {
        err = EINVAL;
        goto done;
    }

    // estimate actual fb size
    u32_t dpc_sq = state->crt.pel_dot;
    u32_t total_px = state->crt.h_cclk * state->crt.v_cclk;
    if ((state->options & VGA_MODE_GFX)) {
        total_px *= dpc_sq;
        total_px = total_px / (1 + (color < 256));
    } else {
        total_px = total_px * 2;
    }

    if (state->fb_sz && state->fb_sz < total_px) {
        err = EINVAL;
    }

    state->fb_sz = total_px;

    // RAM Enable, I/OAS
    u32_t clk_f = state->crt.h_cclk * state->crt.v_cclk * state->crt.freq;
    u32_t misc = 0b11;
    clk_f = (clk_f * dpc_sq) / 1000000;

    if (!(clk_f && clk_f <= 28)) {
        err = EINVAL;
    }

    // require 28 MHz clock
    if (clk_f > 25) {
        misc |= 0b01 << 2;
    }
    // 25 MHz clock: 0b00 << 2

    reg->write(state, VGA_SRX, VGA_SR00, 0x0, ALL_FIELDS);
    reg->write(state, VGA_MISCX, 0, misc, 0b1111);

    // SEQN_CLK: shift every CCLK, DCLK passthrough, 8/9 DCLK
    u32_t d89 = (state->crt.pel_dot != 9);
    reg->write(state, VGA_SRX, VGA_SR01, d89, ALL_FIELDS);

    // GR0-4
    u32_t gr_regs[] = { 0, 0, 0, 0, 0 };
    reg->set_seq(state, VGA_GRX, 0, gr_regs, 5);

    // Setup CRT
    vga_crt_update(state);

    if ((state->options & VGA_MODE_GFX)) { // GFX MODE
        // GFX mode, check if 8 bit color depth is required
        u32_t c256 = (color == 256);
        reg->write(state, VGA_ARX, VGA_AR10, 1 | (c256 << 6), ALL_FIELDS);

        // All 4 maps enabled, chain 4 mode
        reg->write(state, VGA_SRX, VGA_SR02, 0b1111, ALL_FIELDS);

        // Chain-4, Odd/Even, Extended Memory
        reg->write(state, VGA_SRX, VGA_SR04, 0b1010, ALL_FIELDS);

        // check 256 color mode, RdM0, WrM0
        reg->write(
          state, VGA_GRX, VGA_GR05, (c256 << 6) | (1 << 4), ALL_FIELDS);

        // Legacy GFX FB (for compatibility): 0xb8000, GFX mode
        reg->write(state, VGA_GRX, VGA_GR06, 0b1111, ALL_FIELDS);

    } else { // AN MOODE
        // Only map 0,1 enabled, (ascii and attribute)
        reg->write(state, VGA_SRX, VGA_SR02, 0b0011, ALL_FIELDS);

        // Character (AN) mode
        reg->write(state, VGA_ARX, VGA_AR10, 0, ALL_FIELDS);

        // Extended Memory
        reg->write(state, VGA_SRX, VGA_SR04, 0b0010, ALL_FIELDS);

        // Shift Reg, RdM0, WrM0
        reg->write(state, VGA_GRX, VGA_GR05, (1 << 5), ALL_FIELDS);

        // Legacy CGA FB: 0xB800, AN mode
        reg->write(state, VGA_GRX, VGA_GR06, 0b1100, 0b1111);
    }

    // all planes not involved in color compare
    reg->write(state, VGA_GRX, VGA_GR07, 0, ALL_FIELDS);
    // No rotate, no bit ops with latched px
    reg->write(state, VGA_GRX, VGA_GR03, 0, ALL_FIELDS);

    // Load default bit mask
    reg->write(state, VGA_GRX, VGA_GR08, 0xff, ALL_FIELDS);

    reg->write(state, VGA_ARX, VGA_AR12, 0xf, ALL_FIELDS);
    reg->write(state, VGA_ARX, VGA_AR11, 0b100, ALL_FIELDS);
    reg->write(state, VGA_ARX, VGA_AR13, 0, ALL_FIELDS);
    reg->write(state, VGA_ARX, VGA_AR14, 0, ALL_FIELDS);

    reg->write(state, VGA_SRX, VGA_SR00, 0x3, ALL_FIELDS);

done:
    cpu_enable_interrupt();
    return err;
}
