#include <lunaix/mm/valloc.h>
#include <lunaix/syslog.h>
#include <lunaix/owloysius.h>

#include "devtree.h"

LOG_MODULE("dtb")

static morph_t* devtree_obj_root;
static struct dt_context dtctx;

void 
fdt_itbegin(struct fdt_iter* fdti, struct fdt_header* fdt_hdr)
{
    unsigned int off_struct, off_str;
    struct fdt_token* tok;
    const char* str_blk;

    off_str    = fdt_hdr->off_dt_strings;
    off_struct = fdt_hdr->off_dt_struct;

    tok = offset_t(fdt_hdr, struct fdt_token, off_struct);
    str_blk = offset_t(fdt_hdr, const char, off_str);
    
    *fdti = (struct fdt_iter) {
        .pos = tok,
        .next = tok,
        .str_block = str_blk,
        .state = FDT_STATE_START,
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

    current = fdti->next;
    if (!current) {
        return false;
    }

    do
    {
        if (fdt_nope(current)) {
            continue;
        }

        if (fdt_node(current)) {
            fdti->state = FDT_STATE_NODE;
            fdti->depth++;
            goto done;
        }
        
        if (fdt_node_end(current)) {
            if (!fdti->depth) {
                fdti->state = FDT_STATE_END;
                break;
            }

            fdti->state = FDT_STATE_NODE_EXIT;
            fdti->depth--;
            goto done;
        }

        if (fdt_prop(current)) {
            fdti->state = FDT_STATE_PROP;
            goto done;
        }

        fdti->invalid = true;

        current++;
    } while (!fdti->invalid && fdti->state != FDT_STATE_END);

    return false;
    
done:
    fdti->pos = current;

    ptr_t moves = sizeof(struct fdt_token);

    if (fdti->state == FDT_STATE_PROP) {
        moves = fdti->prop->len;
        moves += sizeof(struct fdt_prop);
    } 
    else if (fdti->state == FDT_STATE_NODE) {
        moves += strlen(fdti->detail->extra_data) + 1;
    }

    moves = ROUNDUP(moves, sizeof(struct fdt_token));
    fdti->next = offset(current, moves);

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
    size_t off = fdt_hdr->off_mem_rsvmap;
    
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

static bool
__parse_stdbase_prop(struct fdt_iter* it, struct dtn_base* node)
{
    struct fdt_prop* prop;

    prop = it->prop;

    if (propeq(it, "compatible")) {
        __mkprop_ptr(it, &node->compat);
    } 

    else if (propeq(it, "phandle")) {
        node->phandle = __prop_getu32(it);
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
        char peek = it->prop->val_str[0];
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
__parse_stdnode_prop(struct fdt_iter* it, struct dtn* node)
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
__parse_stdflags(struct fdt_iter* it, struct dtn_base* node)
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
__parse_other_prop(struct fdt_iter* it, struct dtn_base* node)
{
    struct dtp* prop;
    const char* key;
    unsigned int hash;

    prop = valloc(sizeof(*prop));
    key  = fdtit_prop_key(it);

    prop->key = HSTR(key, strlen(key));
    __mkprop_ptr(it, &prop->val);

    hstr_rehash(&prop->key, HSTR_FULL_HASH);

    prop_table_add(node, prop);
}

static void
__fill_node(struct fdt_iter* it, struct dtn* node)
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

    if (parse_stdintr_prop(it, &node->intr)) {
        return;
    }

    __parse_other_prop(it, &node->base);
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
__expand_extended_intr(struct dt_intr_node* intrupt)
{
    struct dtpropi it;
    struct dtp_val  arr;
    struct dtn *master;
    struct dt_intr_prop* intr_prop;

    if (!intrupt->intr.extended) {
        return;
    }

    arr = intrupt->intr.arr;

    llist_init_head(&intrupt->intr.values);
    
    dtpi_init(&it, &arr);
    
    while(dtpi_has_next(&it)) 
    {
        master = dtpi_next_hnd(&it);

        if (!master) {
            WARN("(intr_extended) malformed phandle");
            continue;
        }

        intr_prop = valloc(sizeof(*intr_prop));
        
        intr_prop->master = &master->intr;
        dtpi_next_val(&it, &intr_prop->val, master->base.intr_c);

        llist_append(&intrupt->intr.values, &intr_prop->props);
    };
}

static void
__resolve_phnd_references()
{
    struct dtn_base *pos, *n;
    struct dtn *node, *parent, *default_parent;
    struct dt_intr_node* intrupt;
    dt_phnd_t phnd;
    
    llist_for_each(pos, n, &dtctx.nodes, nodes)
    {
        node = dtn_from(pos);
        intrupt = &node->intr;

        if (!node->base.intr_c) {
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

        intrupt->parent = &parent->intr;

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
    dtctx.reloacted_dtb = dtb_dropoff;

    if (dtctx.fdt->magic != FDT_MAGIC) {
        ERROR("invalid dtb, unexpected magic: 0x%x, expect: 0x%x", dtctx.fdt->magic, FDT_MAGIC);
        return false;
    }

    size_t str_off = dtctx.fdt->off_dt_strings;
    dtctx.str_block = offset_t(dtb_dropoff, const char, str_off);

    llist_init_head(&dtctx.nodes);
    hashtable_init(dtctx.phnds_table);

    struct fdt_iter it;
    struct fdt_token* tok;
    struct dtn *node, *child;
    
    struct dtn* depth[16] = { NULL };
    int level, nr_nodes = 0;

    node = NULL;
    fdt_itbegin(&it, dtctx.fdt);
    
    while (fdt_itnext(&it)) {

        if (it.depth >= 16) {
            // tree too deep
            ERROR("strange dtb, too deep to dive.");
            return false;
        }

        level = it.depth - 1;
        node = depth[level];

        assert(level >= 0);

        if (it.state == FDT_STATE_NODE)
        {
            assert(!node);

            node = vzalloc(sizeof(struct dtn));
            __init_node_regular(node);
            llist_append(&dtctx.nodes, &node->base.nodes);

            __dt_node_set_name(node, it.detail->extra_data);

            if (level) {
                __set_parent(&depth[level - 1]->base, &node->base);
            }

            nr_nodes++;
            depth[level] = node;
            continue;
        }
        
        if (it.state == FDT_STATE_NODE_EXIT)
        {
            depth[it.depth] = NULL;
            continue;
        }
        
        assert(node);

        if (it.state == FDT_STATE_PROP)
        {
            __fill_node(&it, node);
        }
    }

    if (it.invalid)
    {
        FATAL("invalid dtb. state: %d, token: 0x%x", 
                it.state, it.pos->token);
    }

    fdt_itend(&it);

    dtctx.root = depth[0];

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