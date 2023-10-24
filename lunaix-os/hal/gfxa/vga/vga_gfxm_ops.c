#include "vga.h"
#include <hal/gfxm.h>
#include <klibc/string.h>

static int
__vga_gfxa_update_profile(struct gfxa* gfxa)
{
    struct vga* v = (struct vga*)gfxa->hw_obj;
    struct disp_profile* profile = &gfxa->disp_info;

    v->crt.depth = profile->mon.depth;
    v->options = VGA_MODE_GFX;

    vga_config_rect(
      v, profile->mon.w_px, profile->mon.h_px, profile->mon.freq, 0);

    int err = vga_reload_config(v);

    if (!err && profile->clut.val) {
        vga_update_palette(v, profile->clut.val, profile->clut.len);
    }

    return err;
}

static int
__vga_gfxa_rreads(struct gfxa* gfxa, u32_t* map, void* rxbuf, size_t map_sz)
{
    return 0;
}

static int
__vga_gfxa_rwrites(struct gfxa* gfxa, u32_t* map, void* txbuf, size_t map_sz)
{
    return 0;
}

static int
__vga_gfxa_vmcpy(struct gfxa* gfxa, void* buf, off_t off, size_t sz)
{
    struct vga* v = (struct vga*)gfxa->hw_obj;

    if (off + sz > v->fb_sz) {
        return 0;
    }

    ptr_t vram_start = v->fb + off;
    memcpy((void*)vram_start, buf, sz);

    return sz;
}

static int
__vga_gfxa_lfbcpy(struct gfxa* gfxa, void* buf, off_t off, size_t sz)
{
    return __vga_gfxa_vmcpy(gfxa, buf, off, sz);
}

static int
__vga_gfxa_hwioctl(struct gfxa* gfxa, int req, va_list args)
{
    // TODO
    return 0;
}

struct gfxa_ops vga_gfxa_ops = { .update_profile = __vga_gfxa_update_profile,
                                 .rreads = __vga_gfxa_rreads,
                                 .rwrites = __vga_gfxa_rwrites,
                                 .vmcpy = __vga_gfxa_vmcpy,
                                 .lfbcpy = __vga_gfxa_lfbcpy,
                                 .hwioctl = __vga_gfxa_hwioctl };