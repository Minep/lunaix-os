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
__mkprop_val32(struct fdt_iter* it, struct dt_prop_val* val)
{
    val->u32_val = le(*(u32_t*)&it->prop[1]);
    val->size = le(it->prop->len);
}

static inline void
__mkprop_val64(struct fdt_iter* it, struct dt_prop_val* val)
{
    val->u64_val = le64(*(u64_t*)&it->prop[1]);
    val->size = le(it->prop->len);
}

static inline void
__mkprop_ptr(struct fdt_iter* it, struct dt_prop_val* val)
{
    val->ptr_val = __ptr(&it->prop[1]);
    val->size = le(it->prop->len);
}

static inline u32_t
__prop_getu32(struct fdt_iter* it)
{
    return le(*(u32_t*)&it->prop[1]);
}

bool
parse_stdintr_prop(struct fdt_iter* it, struct dt_intr_node* node);

bool
parse_stdintr_prop(struct fdt_iter* it, struct dt_intr_node* node);

#endif /* __LUNAIX_DEVTREE_H */
