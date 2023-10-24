#ifndef __LUNAIX_GFXM_H
#define __LUNAIX_GFXM_H

#include <lunaix/device.h>
#include <usr/lunaix/gfx.h>

struct gfxa;

struct disp_profile
{
    struct gfxa_mon mon;

    struct
    {
        color_t* val;
        size_t len;
    } clut;
};

struct gfxa_ops
{
    int (*update_profile)(struct gfxa*);
    /**
     * @brief Read adapter registers
     *
     */
    int (*rreads)(struct gfxa*, u32_t* map, void* rxbuf, size_t map_sz);
    /**
     * @brief Write adapter registers
     *
     */
    int (*rwrites)(struct gfxa*, u32_t* map, void* txbuf, size_t map_sz);
    /**
     * @brief send data to VRAM
     *
     */
    int (*vmcpy)(struct gfxa*, void*, off_t, size_t);
    /**
     * @brief send logical frame buffer to adapter
     *
     */
    int (*lfbcpy)(struct gfxa*, void*, off_t, size_t);
    /**
     * @brief Execute hardware dependent ioctl command
     *
     */
    int (*hwioctl)(struct gfxa*, int, va_list);
};

struct gfxa
{
    struct device* dev;
    struct llist_header gfxas;
    struct hlist_node gfxas_id;
    struct disp_profile disp_info;
    int id;
    void* hw_obj;

    struct gfxa_ops ops;
};

struct gfxa*
gfxm_alloc_adapter(void* hw_obj);

void
gfxm_register(struct gfxa*);

struct gfxa*
gfxm_adapter(int gfxa_id);

int
gfxm_set_lut(struct gfxa*, color_t* lut, size_t len);

#endif /* __LUNAIX_GFXM_H */