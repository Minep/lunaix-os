#include <lunaix/mm/valloc.h>
#include <lunaix/syslog.h>

#include <klibc/string.h>

#include "devtree.h"

LOG_MODULE("dtb")

static struct dt_context dtctx;

void 
fdt_itbegin(struct fdt_iter* fdti, struct fdt_header* fdt_hdr)
{
    unsigned int off_struct, off_str;
    struct fdt_token* tok;
    const char* str_blk;

    off_str    = le(fdt_hdr->off_dt_strings);
    off_struct = le(fdt_hdr->off_dt_struct);

    tok = offset_t(fdt_hdr, struct fdt_token, off_struct);
    str_blk = offset_t(fdt_hdr, const char, off_str);
    
    *fdti = (struct fdt_iter) {
        .pos = tok,
        .str_block = str_blk
    };
}

void 
fdt_itend(struct fdt_iter* fdti)
{
    fdti->pos = NULL;
}

bool
fdt_itnext(struct fdt_iter* fdti)
{
    struct fdt_token *current;
    struct fdt_prop  *prop;

    current = fdti->pos;
    if (!current) {
        return false;
    }

    do
    {
        if (fdt_nope(current)) {
            continue;
        }

        if (fdt_prop(current)) {
            prop    = (struct fdt_prop*) current;
            current = offset(current, prop->len);
            continue;
        }
        
        if (fdt_node_end(current)) {
            fdti->depth--;
            continue;
        }
        
        // node begin

        fdti->depth++;
        if (fdti->depth == 1) {
            // enter root node
            break;
        }

        while (!fdt_prop(current) && !fdt_node_end(current)) {
            current++;
        }

        if (fdt_prop(current)) {
            break;
        }
        
        current++;
        
    } while (fdt_nope(current) && fdti->depth > 0);

    return fdti->depth > 0;
}

bool 
fdt_itnext_at(struct fdt_iter* fdti, int level)
{
    while (fdti->depth != level && fdt_itnext(fdti));
    
    return fdti->depth == level;
}

void
fdt_memrsvd_itbegin(struct fdt_memrsvd_iter* rsvdi, 
                    struct fdt_header* fdt_hdr)
{
    size_t off = le(fdt_hdr->off_mem_rsvmap);
    
    rsvdi->block = 
        offset_t(fdt_hdr, typeof(*rsvdi->block), off);
    
    rsvdi->block = &rsvdi->block[-1];
}

bool
fdt_memrsvd_itnext(struct fdt_memrsvd_iter* rsvdi)
{
    struct fdt_memrsvd_ent* ent;

    ent = rsvdi->block;
    if (!ent) {
        return false;
    }

    rsvdi->block++;

    return ent->addr || ent->size;
}

void
fdt_memrsvd_itend(struct fdt_memrsvd_iter* rsvdi)
{
    rsvdi->block = NULL;
}

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

static bool
__parse_stdbase_prop(struct fdt_iter* it, struct dt_node_base* node)
{
    struct fdt_prop* prop;

    prop = it->prop;

    if (propeq(it, "compatible")) {
        __mkprop_ptr(it, &node->compat);
    } 
    
    else if (propeq(it, "model")) {
        node->model = (const char*)&prop[1];
    } 

    else if (propeq(it, "phandle")) {
        node->phandle = __prop_getu32(it);
        hashtable_hash_in(dtctx.phnds_table, 
                          &node->phnd_link, node->phandle);
    }
    
    else if (propeq(it, "#address-cells")) {
        node->addr_c = (char)__prop_getu32(it);
    } 
    
    else if (propeq(it, "#size-cells")) {
        node->sz_c = (char)__prop_getu32(it);
    } 
    
    else if (propeq(it, "#interrupt-cells")) {
        node->intr_c = (char)__prop_getu32(it);
    } 
    
    else if (propeq(it, "status")) {
        char peek = *(char*)&it->prop[1];
        if (peek == 'o') {
            node->status = STATUS_OK;
        }
        else if (peek == 'r') {
            node->status = STATUS_RSVD;
        }
        else if (peek == 'd') {
            node->status = STATUS_DISABLE;
        }
        else if (peek == 'f') {
            node->status = STATUS_FAIL;
        }
    }

    else {
        return false;
    }

    return true;
}

static bool
__parse_stdnode_prop(struct fdt_iter* it, struct dt_node* node)
{
    if (propeq(it, "reg")) {
        __mkprop_ptr(it, &node->reg);
    }

    else if (propeq(it, "virtual-reg")) {
        __mkprop_ptr(it, &node->vreg);
    }

    else if (propeq(it, "ranges")) {
        __mkprop_ptr(it, &node->ranges);
    }

    else if (propeq(it, "dma-ranges")) {
        __mkprop_ptr(it, &node->dma_ranges);
    }

    else {
        return false;
    }

    return true;
}

static bool
__parse_stdflags(struct fdt_iter* it, struct dt_node_base* node)
{
    if (propeq(it, "dma-coherent")) {
        node->dma_coherent = true;
    }

    else if (propeq(it, "dma-noncoherent")) {
        node->dma_ncoherent = true;
    }

    else if (propeq(it, "interrupt-controller")) {
        node->intr_controll = true;
    }

    else {
        return false;
    }

    return true;
}

static void
__parse_other_prop(struct fdt_iter* it, struct dt_node_base* node)
{
    struct dt_prop* prop;
    const char* key;
    unsigned int hash;

    prop = valloc(sizeof(*prop));
    key  = fdtit_prop_key(it);

    prop->key = HSTR(key, strlen(key));
    __mkprop_ptr(it, &prop->val);

    hstr_rehash(&prop->key, HSTR_FULL_HASH);
    hash = prop->key.hash;

    hashtable_hash_in(node->_op_bucket, &prop->ht, hash);
}

static void
__fill_node(struct fdt_iter* it, struct dt_node* node)
{
    if (__parse_stdflags(it, &node->base)) {
        return;
    }

    if (__parse_stdbase_prop(it, &node->base)) {
        return;
    }

    if (__parse_stdnode_prop(it, node)) {
        return;
    }

    if (__parse_stdintr_prop(it, &node->intr)) {
        return;
    }

    __parse_other_prop(it, &node->base);
}

static void
__fill_root(struct fdt_iter* it, struct dt_root* node)
{
    if (__parse_stdflags(it, &node->base)) {
        return;
    }
    
    if (__parse_stdbase_prop(it, &node->base)) {
        return;
    }

    struct fdt_prop* prop;

    prop = it->prop;
    if (propeq(it, "serial-number")) {
        node->serial = (const char*)&prop[1];
    }

    else if (propeq(it, "chassis-type")) {
        node->chassis = (const char*)&prop[1];
    }

    __parse_other_prop(it, &node->base);
}

static inline void
__init_node(struct dt_node_base* node)
{
    hashtable_init(node->_op_bucket);
    llist_init_head(&node->children);

    if (node->parent)
        node->_std = node->parent->_std;
}

static inline void
__init_node_regular(struct dt_node* node)
{
    __init_node(&node->base);
    node->intr.parent_hnd = PHND_NULL;
}

static void
__expand_extended_intr(struct dt_intr_node* intrupt)
{
    struct dt_prop_iter it;
    struct dt_prop_val  arr;
    struct dt_node *node;
    struct dt_node *master;
    struct dt_intr_prop* intr_prop;

    if (!intrupt->intr.extended) {
        return;
    }

    arr = intrupt->intr.arr;
    node = INTR_TO_DTNODE(intrupt);

    llist_init_head(&intrupt->intr.values);
    
    dt_decode(&it, &node->base, &arr, 1);
    
    dt_phnd_t phnd;
    do {
        phnd   = dtprop_to_u32(it.prop_loc);
        master = dt_resolve_phandle(phnd);

        if (!master) {
            WARN("dtb: (intr_extended) malformed phandle: %d", phnd);
            continue;
        }

        intr_prop = valloc(sizeof(*intr_prop));
        
        intr_prop->master = &master->intr;
        intr_prop->val = (struct dt_prop_val) {
            .encoded = it.prop_loc_next,
            .size    = master->base.intr_c
        };

        llist_append(&intrupt->intr.values, &intr_prop->props);
        dtprop_next_n(&it, intr_prop->val.size);
        
    } while(dtprop_next(&it));
}

static void
__resolve_phnd_references()
{
    struct dt_node_base *pos, *n;
    struct dt_node *node, *parent, *default_parent;
    struct dt_intr_node* intrupt;
    dt_phnd_t phnd;
    
    llist_for_each(pos, n, &dtctx.nodes, nodes)
    {
        node = BASE_TO_DTNODE(pos);
        intrupt = &node->intr;

        if (!node->base.intr_c) {
            continue;
        }

        phnd = intrupt->parent_hnd;
        default_parent = (struct dt_node*)node->base.parent;
        parent = default_parent;

        if (phnd != PHND_NULL) {
            parent = dt_resolve_phandle(phnd);
        }

        if (!parent) {
            WARN("dtb: (phnd_resolve) malformed phandle: %d", phnd);
            parent = default_parent;
        }

        intrupt->parent = &parent->intr;

        __expand_extended_intr(intrupt);
    }
}

static void
__resolve_inter_map()
{
    struct dt_node_base *pos, *n;

    llist_for_each(pos, n, &dtctx.nodes, nodes)
    {
        resolve_interrupt_map(BASE_TO_DTNODE(pos));
    }
}

bool
dt_load(ptr_t dtb_dropoff)
{
    dtctx.reloacted_dtb = dtb_dropoff;

    if (dtctx.fdt->magic != FDT_MAGIC) {
        ERROR("invalid dtb, unexpected magic: 0x%x", dtctx.fdt->magic);
        return false;
    }

    size_t str_off = le(dtctx.fdt->size_dt_strings);
    dtctx.str_block = offset_t(dtb_dropoff, const char, str_off);

    llist_init_head(&dtctx.nodes);
    hashtable_init(dtctx.phnds_table);

    struct fdt_iter it;
    struct fdt_token* tok;
    struct dt_node_base *node, *prev;
    
    struct dt_node_base* depth[16];
    bool is_root_level, filled;

    node = NULL;
    depth[0] = NULL;
    fdt_itbegin(&it, dtctx.fdt);
    
    while (fdt_itnext(&it)) {
        is_root_level = it.depth == 1;

        if (it.depth >= 16) {
            // tree too deep
            ERROR("strange dtb, too deep to dive.");
            return false;
        }

        depth[it.depth] = NULL;
        node = depth[it.depth - 1];

        if (!node) {
            // need new node
            if (unlikely(is_root_level)) {
                node = vzalloc(sizeof(struct dt_root));
                __init_node(node);
            }
            else {
                node = vzalloc(sizeof(struct dt_node));
                prev = depth[it.depth - 2];
                node->parent = prev;

                __init_node_regular((struct dt_node*)node);
                llist_append(&prev->children, &node->siblings);

                llist_append(&dtctx.nodes, &node->nodes);
            }

            node->name = (const char*)&it.pos[1];
        }

        if (unlikely(is_root_level)) {
            __fill_root(&it, (struct dt_root*)node);
        }
        else {
            __fill_node(&it, (struct dt_node*)node);
        }
    }

    fdt_itend(&it);

    dtctx.root = (struct dt_root*)depth[0];

    __resolve_phnd_references();
    __resolve_inter_map();

    INFO("device tree loaded");

    return true;
}

struct dt_node*
dt_resolve_phandle(dt_phnd_t phandle)
{
    struct dt_node_base *pos, *n;
    hashtable_hash_foreach(dtctx.phnds_table, phandle, pos, n, phnd_link)
    {
        if (pos->phandle == phandle) {
            return (struct dt_node*)pos;
        }
    }

    return NULL;
}

static bool
__byname_predicate(struct dt_node_iter* iter, struct dt_node_base* node)
{
    int i = 0;
    const char* be_matched = node->name;
    const char* name = (const char*)iter->closure;

    while (be_matched[i] && name[i])
    {
        if (be_matched[i] != name[i]) {
            return false;
        }

        i++;
    }

    return true;
}

void
dt_begin_find_byname(struct dt_node_iter* iter, 
              struct dt_node* node, const char* name)
{
    dt_begin_find(iter, node, __byname_predicate, name);
}

void
dt_begin_find(struct dt_node_iter* iter, struct dt_node* node, 
              node_predicate_t pred, void* closure)
{
    node = node ? : (struct dt_node*)dtctx.root;

    iter->head = &node->base;
    iter->matched = NULL;
    iter->closure = closure;
    iter->pred = pred;

    struct dt_node_base *pos, *n;
    llist_for_each(pos, n, &node->base.children, siblings)
    {
        if (pred(iter, pos)) {
            iter->matched = pos;
            break;
        }
    }
}

bool
dt_find_next(struct dt_node_iter* iter,
             struct dt_node_base** matched)
{
    if (!dt_found_any(iter)) {
        return false;
    }

    struct dt_node_base *pos, *head;

    head = iter->head;
    pos = iter->matched;
    *matched = pos;

    while (&pos->siblings != &head->children)
    {
        pos = list_next(pos, struct dt_node_base, siblings);

        if (!iter->pred(iter, pos)) {
            continue;
        }

        iter->matched = pos;
        return true;
    }

    return false;
}

struct dt_prop_val*
dt_getprop(struct dt_node_base* base, const char* name)
{
    struct hstr hashed_name;
    struct dt_prop *pos, *n;
    unsigned int hash;

    hashed_name = HSTR(name, strlen(name));
    hstr_rehash(&hashed_name, HSTR_FULL_HASH);
    hash = hashed_name.hash;

    hashtable_hash_foreach(base->_op_bucket, hash, pos, n, ht)
    {
        if (HSTR_EQ(&pos->key, &hashed_name)) {
            return &pos->val;
        }
    }

    return NULL;
}