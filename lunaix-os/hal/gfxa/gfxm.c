#include <lunaix/compiler.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/spike.h>
#include <lunaix/status.h>

#include <hal/gfxm.h>

#include <klibc/string.h>

DEFINE_LLIST(gfxa_flat);
DECLARE_HASHTABLE(gfxa_idset, 8);

struct device_cat* gfxa_devcat = NULL;
static int id = 0;

static struct devclass gfxa_class = DEVCLASS(GENERIC, DISP, GFXA);

static int
__gfxa_cmd_router(struct device* dev, u32_t req, va_list args)
{
    struct gfxa* gfxa = (struct gfxa*)dev->underlay;
    struct disp_profile* disp = &gfxa->disp_info;

    switch (req) {
        case GFX_ADAPTER_INFO: {
            struct gfxa_info* info = va_arg(args, struct gfxa_info*);
            *info =
              (struct gfxa_info){ .disp_mode = { .w_px = disp->mon.w_px,
                                                 .h_px = disp->mon.h_px,
                                                 .freq = disp->mon.freq,
                                                 .depth = disp->mon.depth } };
            gfxa->ops.hwioctl(gfxa, req, args);
            break;
        };
        case GFX_ADAPTER_CHMODE: {
            struct gfxa_mon* mon_info = va_arg(args, struct gfxa_mon*);
            gfxa->disp_info.mon = *mon_info;

            return gfxa->ops.update_profile(gfxa);
        };
        case GFX_ADAPTER_VMCPY:
        case GFX_ADAPTER_LFBCPY: {
            void* buf = va_arg(args, void*);
            off_t off = va_arg(args, off_t);
            size_t len = va_arg(args, size_t);
            if (req == GFX_ADAPTER_VMCPY) {
                return gfxa->ops.vmcpy(gfxa, buf, off, len);
            }
            return gfxa->ops.lfbcpy(gfxa, buf, off, len);
        };
        case GFX_ADAPTER_REG_WR:
        case GFX_ADAPTER_REG_RD: {
            u32_t* reg_addrmap = va_arg(args, u32_t*);
            void* reg_val = va_arg(args, void*);
            size_t reg_count = va_arg(args, size_t);

            if (req == GFX_ADAPTER_LUT_RD) {
                return gfxa->ops.rreads(gfxa, reg_addrmap, reg_val, reg_count);
            }
            return gfxa->ops.rwrites(gfxa, reg_addrmap, reg_val, reg_count);
        };
        case GFX_ADAPTER_LUT_RD: {
            struct gfxa_clut* clut = va_arg(args, struct gfxa_clut*);
            if (!clut->val || !clut->len) {
                return disp->clut.len;
            }

            size_t clut_len = MIN(clut->len, disp->clut.len);
            memcpy(clut->val, disp->clut.val, clut_len * sizeof(color_t));
            return clut_len;
        };
        case GFX_ADAPTER_LUT_WR: {
            struct gfxa_clut* clut = va_arg(args, struct gfxa_clut*);
            int err = gfxm_set_lut(gfxa, clut->val, clut->len);
            if (!err) {
                err = gfxa->ops.update_profile(gfxa);
            }
            return err;
        };
        default:
            return gfxa->ops.hwioctl(gfxa, req, args);
    }

    return 0;
}

struct gfxa*
gfxm_alloc_adapter(void* hw_obj)
{
    if (unlikely(!gfxa_devcat)) {
        gfxa_devcat = device_addcat(NULL, "gfx");
    }

    struct gfxa* gfxa = valloc(sizeof(struct gfxa));
    struct device* gfxa_dev = device_allocsys(dev_meta(gfxa_devcat), gfxa);

    *gfxa = (struct gfxa){ .dev = gfxa_dev, .hw_obj = hw_obj };

    gfxa_dev->ops.exec_cmd = __gfxa_cmd_router;

    return gfxa;
}

void
gfxm_register(struct gfxa* gfxa)
{
    gfxa->id = gfxa_class.variant++;

    llist_append(&gfxa_flat, &gfxa->gfxas);
    hashtable_hash_in(gfxa_idset, &gfxa->gfxas_id, gfxa->id);

    register_device(gfxa->dev, &gfxa_class, "gfxa%d", gfxa->id);
}

struct gfxa*
gfxm_adapter(int gfxa_id)
{
    struct gfxa *pos, *n;
    hashtable_hash_foreach(gfxa_idset, gfxa_id, pos, n, gfxas_id)
    {
        if (pos->id == gfxa_id) {
            return pos;
        }
    }

    return NULL;
}

int
gfxm_set_lut(struct gfxa* gfxa, u32_t* lut, size_t len)
{
    // It is generally not feasible for graphic adapter with >256 colors still
    // using color lut
    if (unlikely(len > 256)) {
        return EINVAL;
    }

    struct disp_profile* disp = &gfxa->disp_info;
    size_t lutsz = len * sizeof(*lut);

    if (disp->clut.len != len) {
        vfree_safe(disp->clut.val);
        disp->clut.val = valloc(lutsz);
        disp->clut.len = len;
    }

    memcpy(disp->clut.val, lut, lutsz);

    return 0;
}