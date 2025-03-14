#ifndef __LUNAIX_DEVTREE_INTERNAL_H
#define __LUNAIX_DEVTREE_INTERNAL_H

#include <hal/devtree.h>
#include <klibc/string.h>

static inline bool
propeq(struct fdt_blob* fdt, fdt_loc_t loc, const char* key)
{
    return streq(fdt_prop_key(fdt, loc), key);
}

static inline ptr_t
__prop_val_ptr(struct fdt_prop* prop)
{
    return __ptr(prop) + sizeof(struct fdt_prop);
}

static inline void
__mkprop_ptr(fdt_loc_t loc, struct dtp_val* val)
{
    val->ptr_val = __prop_val_ptr(loc.prop);
    val->size = loc.prop->len;
}

static inline u32_t
__prop_getu32(fdt_loc_t loc)
{
    return loc.prop->val[0];
}

bool
parse_stdintr_prop(struct fdt_blob*, fdt_loc_t, struct dtn_intr*);

#endif /* __LUNAIX_DEVTREE_H */
