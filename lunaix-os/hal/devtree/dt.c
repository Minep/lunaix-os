#include <lunaix/mm/valloc.h>
#include <lunaix/syslog.h>
#include <lunaix/owloysius.h>

#include "devtree.h"

LOG_MODULE("dtb")

static morph_t* devtree_obj_root;
static struct dt_context dtctx;

void
fdt_load(struct fdt_blob* fdt, ptr_t base)
{
    struct fdt_header* hdr;

    fdt->fdt_base = base;

    hdr = fdt->header;
    if (hdr->magic != FDT_MAGIC) {
        FATAL("invalid dtb, unexpected magic: 0x%x, expect: 0x%x", 
                hdr->magic, FDT_MAGIC);
    }

    fdt->plat_rsvd_base = base + hdr->off_mem_rsvmap;
    fdt->str_block_base = base + hdr->off_dt_strings;
    fdt->root.ptr       = base + hdr->off_dt_struct;
}

bool
fdt_next_boot_rsvdmem(struct fdt_blob* fdt, fdt_loc_t* loc, 
                      struct dt_memory_node* mem)
{
    fdt_loc_t current;

    current = *loc;

    if (!current.rsvd_ent->addr && !current.rsvd_ent->addr) {
        return false;
    }

    mem->base = current.rsvd_ent->addr;
    mem->size = current.rsvd_ent->size;
    mem->type = FDT_MEM_RSVD;

    current.rsvd_ent++;
    *loc = current;

    return true;
}

fdt_loc_t
fdt_next_token(fdt_loc_t loc, int* delta_depth)
{
    int d = 0;

    do {
        if (fdt_node(loc.token)) {
            d++;
            loc.ptr += strlen(loc.node->name) + 1;
            loc.ptr  = ROUNDUP(loc.ptr, sizeof(int));
        }
        else if (fdt_node_end(loc.token)) {
            d--;
        }
        else if (fdt_prop(loc.token)) {
            loc.ptr   += loc.prop->len + 2 * sizeof(int);
            loc.ptr    = ROUNDUP(loc.ptr, sizeof(int));
        }

        loc.token++;
    } while (fdt_nope(loc.token));

    *delta_depth = d;
    return loc;
}

bool
fdt_next_sibling(fdt_loc_t loc, fdt_loc_t* loc_out)
{
    int depth = 0, new_depth = 0;

    do {
        loc = fdt_next_token(loc, &new_depth);
        depth += new_depth;
    } while (depth > 0);

    *loc_out = loc;
    return !fdt_node_end(loc.token);
}

fdt_loc_t
fdt_descend_into(fdt_loc_t loc) 
{
    fdt_loc_t new_loc;
    int depth = 0;

    new_loc = fdt_next_token(loc, &depth);

    return depth != 1 ? loc : new_loc;
}

bool
fdt_find_prop(const struct fdt_blob* fdt, fdt_loc_t loc, 
              const char* name, struct dtp_val* val)
{
    char* prop_name;

    loc = fdt_descend_into(loc);

    do
    {
        if (!fdt_prop(loc.token)) {
            continue;
        }

        prop_name = fdt_prop_key(fdt, loc);
        
        if (!streq(prop_name, name)) {
            continue;
        }

        dtp_val_set(val, (dt_enc_t)loc.prop->val, loc.prop->len);
        return true;
    } while (fdt_next_sibling(loc, &loc));

    return false;
}

bool
fdt_memscan_begin(struct fdt_memscan* mscan, const struct fdt_blob* fdt)
{
    struct dtp_val val;
    fdt_loc_t loc;

    loc  = fdt->root;
    loc  = fdt_descend_into(loc);

    if (fdt_find_prop(fdt, loc, "#address-cells", &val))
    {
        mscan->root_addr_c = val.ref->u32_val;
    }

    if (fdt_find_prop(fdt, loc, "#size-cells", &val))
    {
        mscan->root_size_c = val.ref->u32_val;
    }

    mscan->loc = loc;
}

#define get_size(mscan, val)    \
    (mscan->root_size_c == 1 ? (val)->ref->u32_val : (val)->ref->u64_val)

#define get_addr(mscan, val)    \
    (mscan->root_addr_c == 1 ? (val)->ref->u32_val : (val)->ref->u64_val)

bool
fdt_itnext_memory(struct fdt_memscan* mscan, struct fdt_blob* fdt)
{
    char* prop_name;

    struct dtp_val val, reg_val;
    fdt_loc_t loc;
    struct dtpropi dtpi;
    bool has_reg = false;

    loc  = mscan->loc;

restart:
    while (fdt_next_sibling(loc, &loc))
    {
        if (!fdt_node(loc.token))
            continue;

        if (mscan->node_type != FDT_MEM_FREE) {
            goto found;
        }

        if (streq(loc.node->name, "reserved-memory")) {
            // dived into /reserved-memory, walking for childrens
            mscan->node_type = FDT_MEM_RSVD;
            loc = fdt_descend_into(loc);
            continue;
        }

        if (!fdt_find_prop(fdt, loc, "device_type", &val))
            continue;

        if (!streq(val.str_val, "memory"))
            continue;

        goto found;
    }

    // emerged from /reserved-memory, resume walking for /memory
    if (mscan->node_type != FDT_MEM_FREE) {
        mscan->node_type = FDT_MEM_FREE;
        goto restart;
    }

    return false;

found:

    dtpi_init_empty(&mscan->regit);

    mscan->loc = loc;
    has_reg = fdt_find_prop(fdt, loc, "reg", &val);
    if (mscan->node_type == FDT_MEM_RSVD) {
        goto do_rsvd_child;
    }

    if (!has_reg)
    {
        WARN("malformed memory node");
        goto restart;
    }

    dtpi_init(&mscan->regit, &val);

    return true;

do_rsvd_child:
    if (has_reg)
    {
        mscan->node_type = FDT_MEM_RSVD_DYNAMIC;

        dtpi_init(&mscan->regit, &val);
        return true;
    }

    if (!fdt_find_prop(fdt, loc, "size", &val)) {
        WARN("malformed reserved memory child node");
        goto restart;
    }

    mscan->node_type = FDT_MEM_RSVD;
    mscan->node_attr.total_size = get_size(mscan, &val);

    if (fdt_find_prop(fdt, loc, "no-map", &val)) {
        mscan->node_attr.nomap = true;
    }

    if (fdt_find_prop(fdt, loc, "reusable", &val)) {
        mscan->node_attr.reusable = true;
    }

    if (fdt_find_prop(fdt, loc, "alignment", &val)) {
        mscan->node_attr.alignment = get_size(mscan, &val);
    }

    if (fdt_find_prop(fdt, loc, "alloc-ranges", &val)) {
        dtpi_init(&mscan->regit, &val);
    }

    return true;
}

bool
fdt_memscan_nextrange(struct fdt_memscan* mscan, struct dt_memory_node* mem)
{
    struct dtp_val val;

    if (dtpi_is_empty(&mscan->regit)) {
        return false;
    }

    if (dtpi_next_val(&mscan->regit, &val, mscan->root_addr_c)) {
        mem->base = get_addr(mscan, &val);
    }

    if (dtpi_next_val(&mscan->regit, &val, mscan->root_size_c)) {
        mem->base = get_size(mscan, &val);
    }

    mem->type = mscan->node_type;
    
    if (mem->type == FDT_MEM_RSVD_DYNAMIC) {
        mem->dyn_alloc_attr = mscan->node_attr;
    }

    return dtpi_has_next(&mscan->regit);
}

static bool
__parse_stdbase_prop(struct fdt_blob* fdt, fdt_loc_t loc, 
                     struct dtn_base* node)
{
    if (propeq(fdt, loc, "compatible")) {
        __mkprop_ptr(loc, &node->compat);
    } 

    else if (propeq(fdt, loc, "phandle")) {
        node->phandle = __prop_getu32(loc);
    }
    
    else if (propeq(fdt, loc, "#address-cells")) {
        node->addr_c = (char)__prop_getu32(loc);
    } 
    
    else if (propeq(fdt, loc, "#size-cells")) {
        node->sz_c = (char)__prop_getu32(loc);
    } 
    
    else if (propeq(fdt, loc, "#interrupt-cells")) {
        node->intr_c = (char)__prop_getu32(loc);
    } 
    
    else if (propeq(fdt, loc, "status")) {
        char peek = loc.prop->val_str[0];
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
__parse_stdnode_prop(struct fdt_blob* fdt, fdt_loc_t loc, struct dtn* node)
{
    if (propeq(fdt, loc, "reg")) {
        __mkprop_ptr(loc, &node->reg);
    }

    else if (propeq(fdt, loc, "ranges")) {
        __mkprop_ptr(loc, &node->ranges);
    }

    else if (propeq(fdt, loc, "dma-ranges")) {
        __mkprop_ptr(loc, &node->dma_ranges);
    }

    else {
        return false;
    }

    return true;
}

static bool
__parse_stdflags(struct fdt_blob* fdt, fdt_loc_t loc, struct dtn_base* node)
{
    if (propeq(fdt, loc, "dma-coherent")) {
        node->dma_coherent = true;
    }

    else if (propeq(fdt, loc, "dma-noncoherent")) {
        node->dma_ncoherent = true;
    }

    else if (propeq(fdt, loc, "interrupt-controller")) {
        node->intr_controll = true;
    }

    else {
        return false;
    }

    return true;
}

static inline void
__dt_node_set_name(struct dtn* node, const char* name)
{
    changeling_setname(&node->mobj, name);
}

static inline void
__init_prop_table(struct dtn_base* node)
{
    struct dtp_table* propt;

    propt = valloc(sizeof(*propt));
    hashtable_init(propt->_op_bucket);

    node->props = propt;
}

#define prop_table_add(node, prop)                                             \
            hashtable_hash_in( (node)->props->_op_bucket,                      \
                              &(prop)->ht, (prop)->key.hash);

static void
__parse_other_prop(struct fdt_blob* fdt, fdt_loc_t loc, struct dtn_base* node)
{
    struct dtp* prop;
    const char* key;
    unsigned int hash;

    prop = valloc(sizeof(*prop));
    key  = fdt_prop_key(fdt, loc);

    prop->key = HSTR(key, strlen(key));
    __mkprop_ptr(loc, &prop->val);

    hstr_rehash(&prop->key, HSTR_FULL_HASH);

    prop_table_add(node, prop);
}

static void
__fill_node(struct fdt_blob* fdt, fdt_loc_t loc, struct dtn* node)
{
    if (__parse_stdflags(fdt, loc, &node->base)) {
        return;
    }

    if (__parse_stdbase_prop(fdt, loc, &node->base)) {
        return;
    }

    if (__parse_stdnode_prop(fdt, loc, node)) {
        return;
    }

    if (parse_stdintr_prop(fdt, loc, &node->intr)) {
        return;
    }

    __parse_other_prop(fdt, loc, &node->base);
}

static inline void
__set_parent(struct dtn_base* parent, struct dtn_base* node)
{
    morph_t* parent_obj;
    
    parent_obj   = devtree_obj_root;
    node->parent = parent;
    
    if (parent) {
        node->addr_c = parent->addr_c;
        node->sz_c = parent->sz_c;
        node->intr_c = parent->intr_c;
        parent_obj = dt_mobj(parent);
    }

    changeling_attach(parent_obj, dt_mobj(node));
}

static inline void
__init_node_regular(struct dtn* node)
{
    __init_prop_table(&node->base);
    changeling_morph_anon(NULL, node->mobj, dt_morpher);
 
    node->intr.parent_hnd = PHND_NULL;
}

static void
__expand_extended_intr(struct dtn_intr* intrupt)
{
    struct dtpropi it;
    struct dtp_val  arr;
    struct dtn *domain;
    struct dtspec_intr* ispec;
    int nr_intrs = 0;

    if (!intrupt->extended) {
        nr_intrs  = intrupt->raw_ispecs.size / sizeof(u32_t);
        nr_intrs /= intrupt->parent->base.intr_c; 
        goto done;
    }

    arr = intrupt->raw_ispecs;

    llist_init_head(&intrupt->ext_ispecs);
    
    dtpi_init(&it, &arr);

    while(dtpi_has_next(&it)) 
    {
        domain = dtpi_next_hnd(&it);

        if (!domain) {
            WARN("(intr_extended) malformed phandle");
            continue;
        }

        ispec = valloc(sizeof(*ispec));
        
        ispec->domain = domain;
        dtpi_next_val(&it, &ispec->val, domain->base.intr_c);

        llist_append(&intrupt->ext_ispecs, &ispec->ispecs);
        nr_intrs++;
    };

done:
    intrupt->nr_intrs = nr_intrs;
}

static void
__resolve_phnd_references()
{
    struct dtn_base *pos, *n;
    struct dtn *node, *parent, *default_parent;
    struct dtn_intr* intrupt;
    dt_phnd_t phnd;
    
    llist_for_each(pos, n, &dtctx.nodes, nodes)
    {
        node = dtn_from(pos);
        intrupt = &node->intr;

        if (intrupt->parent_hnd == PHND_NULL) {
            continue;
        }

        phnd = intrupt->parent_hnd;
        default_parent = (struct dtn*)node->base.parent;
        parent = default_parent;

        if (phnd != PHND_NULL) {
            parent = dt_resolve_phandle(phnd);
        }

        if (!parent) {
            WARN("dtb: (phnd_resolve) malformed phandle: %d", phnd);
            parent = default_parent;
        }

        intrupt->parent = parent;

        __expand_extended_intr(intrupt);
    }
}

static void
__resolve_inter_map()
{
    struct dtn_base *pos, *n;

    llist_for_each(pos, n, &dtctx.nodes, nodes)
    {
        dt_resolve_interrupt_map(dtn_from(pos));
    }
}

bool
dt_load(ptr_t dtb_dropoff)
{
    llist_init_head(&dtctx.nodes);
    hashtable_init(dtctx.phnds_table);

    struct fdt_blob *fdt;
    struct dtn      *node,
                    *stack[16] = { NULL };
    
    int depth = 0, delta = 0, nr_nodes = 0;
    fdt_loc_t  loc, next_loc;

    fdt = &dtctx.fdt;
    fdt_load(&dtctx.fdt, dtb_dropoff);

    loc = fdt->root;
    
    while (!fdt_eof(loc.token)) 
    {
        next_loc = fdt_next_token(loc, &delta);

        if (depth >= 16) {
            // tree too deep
            ERROR("strange dtb, too deep to dive.");
            return false;
        }

        assert(depth >= 0);
        node = stack[depth];

        if (fdt_node(loc.token))
        {
            assert(!node);

            node = vzalloc(sizeof(struct dtn));
            __init_node_regular(node);
            llist_append(&dtctx.nodes, &node->base.nodes);

            __dt_node_set_name(node, loc.node->name);

            if (depth) {
                __set_parent(&stack[depth - 1]->base, &node->base);
            }

            nr_nodes++;
            stack[depth] = node;
        }

        else if (depth > 1 && fdt_node_end(loc.token))
        {
            stack[depth - 1] = NULL;
        }

        else if (fdt_prop(loc.token))
        {
            node = stack[depth - 1];

            assert(depth && node);
            __fill_node(fdt, loc, node);
        }

        depth += delta;
        loc = next_loc;
    }

    dtctx.root = stack[0];

    __resolve_phnd_references();
    __resolve_inter_map();

    INFO("%d nodes loaded.", nr_nodes);

    return true;
}

struct dtn*
dt_resolve_phandle(dt_phnd_t phandle)
{
    struct dtn_base *pos, *n;
    llist_for_each(pos, n, &dtctx.nodes, nodes)
    {
        if (pos->phandle == phandle) {
            return (struct dtn*)pos;
        }
    }

    return NULL;
}

static bool
__byname_predicate(struct dtn_iter* iter, struct dtn_base* node)
{
    int i = 0;
    const char* be_matched = HSTR_VAL(node->mobj.name);
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
dt_begin_find_byname(struct dtn_iter* iter, 
              struct dtn* node, const char* name)
{
    dt_begin_find(iter, node, __byname_predicate, name);
}

void
dt_begin_find(struct dtn_iter* iter, struct dtn* node, 
              node_predicate_t pred, void* closure)
{
    node = node ? : (struct dtn*)dtctx.root;

    iter->head = &node->base;
    iter->matched = NULL;
    iter->closure = closure;
    iter->pred = pred;

    morph_t *pos, *n;
    struct dtn_base* base;
    changeling_for_each(pos, n, &node->mobj)
    {
        base = &changeling_reveal(pos, dt_morpher)->base;
        if (pred(iter, base)) {
            iter->matched = base;
            break;
        }
    }
}

bool
dt_find_next(struct dtn_iter* iter,
             struct dtn_base** matched)
{
    if (!dt_found_any(iter)) {
        return false;
    }

    struct dtn *node;
    morph_t *pos, *head;

    head = dt_mobj(iter->head);
    pos  = dt_mobj(iter->matched);
    *matched = iter->matched;

    while (&pos->sibs != &head->subs)
    {
        pos = list_next(pos, morph_t, sibs);
        node = changeling_reveal(pos, dt_morpher);

        if (!iter->pred(iter, &node->base)) {
            continue;
        }

        iter->matched = &node->base;
        return true;
    }

    return false;
}

struct dtp_val*
dt_getprop(struct dtn_base* base, const char* name)
{
    struct hstr hashed_name;
    struct dtp *pos, *n;
    unsigned int hash;

    hashed_name = HSTR(name, strlen(name));
    hstr_rehash(&hashed_name, HSTR_FULL_HASH);
    hash = hashed_name.hash;

    hashtable_hash_foreach(base->props->_op_bucket, hash, pos, n, ht)
    {
        if (HSTR_EQ(&pos->key, &hashed_name)) {
            return &pos->val;
        }
    }

    return NULL;
}

void
dtpx_compile_proplet(struct dtprop_def* proplet)
{
    int i;
    unsigned int acc = 0;
    
    for (i = 0; proplet[i].type && i < 10; ++i)
    {
        proplet[i].acc_sz = acc;
        acc += proplet[i].cell;
    }

    if (proplet[i - 1].type && i == 10) {
        FATAL("invalid proplet: no terminator detected");
    }

    proplet[i].acc_sz = acc;
}

void
dtpx_prepare_with(struct dtpropx* propx, struct dtp_val* prop,
                  struct dtprop_def* proplet)
{
    int i;
    bool has_str = false;
    
    for (i = 0; proplet[i].type; ++i);

    propx->proplet = proplet;
    propx->proplet_len = i;
    propx->proplet_sz = proplet[i].acc_sz;
    propx->raw = prop;
    propx->row_loc = 0;
}

bool
dtpx_goto_row(struct dtpropx* propx, int row)
{
    off_t loc;

    loc  = propx->proplet_sz;
    loc *= row;

    if (loc * sizeof(u32_t) >= propx->raw->size) {
        return false;
    }

    propx->row_loc = loc;
    return true;
}

bool
dtpx_next_row(struct dtpropx* propx)
{
    off_t loc;

    loc  = propx->row_loc;
    loc += propx->proplet_sz;

    if (loc * sizeof(u32_t) >= propx->raw->size) {
        return false;
    }

    propx->row_loc = loc;
    return true;
}

bool
dtpx_extract_at(struct dtpropx* propx, 
                struct dtprop_xval* val, int col)
{
    struct dtprop_def* def;
    union dtp_baseval* raw;
    dt_enc_t enc;

    if (unlikely(col >= propx->proplet_len)) {
        return false;
    }

    def = &propx->proplet[col];
    enc = &propx->raw->encoded[propx->row_loc + def->acc_sz];
    raw = (union dtp_baseval*)enc;

    val->archetype = def;

    switch (def->type)
    {
        case DTP_U32:
            val->u32 = raw->u32_val;
            break;

        case DTP_U64:
            val->u64 = raw->u64_val;
            break;

        case DTP_PHANDLE:
        {
            ptr_t hnd = raw->phandle;
            val->phandle = dt_resolve_phandle(hnd);
        } break;

        case DTP_COMPX:
            val->composite = enc;
            break;
        
        default:
            break;
    }

    return true;
}

bool
dtpx_extract_loc(struct dtpropx* propx, 
                 struct dtprop_xval* val, int row, int col)
{
    ptr_t loc = propx->row_loc;

    if (!dtpx_goto_row(propx, row))
        return false;

    
    bool r = dtpx_extract_at(propx, val, col);
    propx->row_loc = loc;
    return r;
}

bool
dtpx_extract_row(struct dtpropx* propx, struct dtprop_xval* vals, int len)
{
    assert(len == propx->proplet_len);

    for (int i = 0; i < len; i++)
    {
        if (!dtpx_extract_at(propx, &vals[i], i)) {
            return false;
        }
    }
    
    return true;
}

struct dt_context*
dt_main_context()
{
    return &dtctx;
}

static void
__init_devtree()
{
    devtree_obj_root = changeling_spawn(NULL, NULL);
}
owloysius_fetch_init(__init_devtree, on_sysconf);