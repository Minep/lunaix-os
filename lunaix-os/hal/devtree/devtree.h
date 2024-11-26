#ifndef __LUNAIX_DEVTREE_INTERNAL_H
#define __LUNAIX_DEVTREE_INTERNAL_H

#include <hal/devtree.h>

#include <klibc/string.h>

static inline bool
propeq(struct fdt_iter* it, const char* key)
{
    return streq(fdtit_prop_key(it), key);
}

static inline void
__mkprop_ptr(struct fdt_iter* it, struct dtp_val* val)
{
    val->ptr_val = __ptr(it->prop->val);
    val->size = it->prop->len;
}

static inline u32_t
__prop_getu32(struct fdt_iter* it)
{
    return it->prop->val[0];
}

bool
parse_stdintr_prop(struct fdt_iter* it, struct dtn_intr* node);

#endif /* __LUNAIX_DEVTREE_H */
