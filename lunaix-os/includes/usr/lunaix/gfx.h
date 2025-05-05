#ifndef _LUNAIX_UHDR_UGFX_H
#define _LUNAIX_UHDR_UGFX_H

#define GFX_CMDA(type, cmd_id) (((type) << 8) | ((cmd_id) & 0xf))

#define GFX_ADAPTER_INFO 1
#define GFX_ADAPTER_REG_RD 2
#define GFX_ADAPTER_REG_WR 3
#define GFX_ADAPTER_CHMODE 4
#define GFX_ADAPTER_LUT_WR 5
#define GFX_ADAPTER_LUT_RD 6
#define GFX_ADAPTER_VMCPY 7
#define GFX_ADAPTER_LFBCPY 8

typedef unsigned int color_t;

struct gfxa_info
{
    struct
    {
        unsigned int w_px;
        unsigned int h_px;
        unsigned int depth;
        unsigned int freq;
    } disp_mode;

    void* hwdep_info;
};

struct gfxa_mon
{
    unsigned int w_px;
    unsigned int h_px;
    unsigned int depth;
    unsigned int freq;
};

struct gfxa_clut
{
    color_t* val;
    size_t len;
};

#endif /* _LUNAIX_UHDR_UGFX_H */
