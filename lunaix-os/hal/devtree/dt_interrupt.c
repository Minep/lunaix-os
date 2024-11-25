#include "devtree.h"

#include <lunaix/mm/valloc.h>

#include <klibc/string.h>

static struct dt_intr_map*
__get_map(struct dt_intr_node* node)
{
    if (!node->map) {
        node->map = valloc(sizeof(struct dt_intr_map));
        
        node->map->resolved = false;
        llist_init_head(&node->map->mapent);

        INTR_TO_DTNODE(node)->base.intr_neuxs = true;
    }

    return node->map;
}

static void
__prepare_key(struct dt_speckey* key, 
              dt_enc_t key_raw, unsigned int keylen)
{
    *key = (struct dt_speckey) {
        .val = valloc(keylen * sizeof(int)),
        .size = keylen
    };

    memcpy(key->val, key_raw, keylen * sizeof(int));
}

static inline void
__destory_key(struct dt_speckey* key)
{
    vfree(key->val);
}

static inline unsigned int
__interrupt_keysize(struct dtn_base* base)
{
    return base->addr_c + base->intr_c;
}

void
dt_speckey_mask(struct dt_speckey* k, struct dt_speckey* mask)
{
    for (unsigned int i = 0; i < k->size; i++)
    {
        k->val[i] &= mask->val[i];
    }
}

static bool
__compare_key(struct dt_speckey* k1, struct dt_speckey* k2)
{
    if (k1->size != k2->size) {
        return false;
    }

    for (unsigned int i = 0; i < k1->size; i++)
    {
        if (k1->val[i] != k2->val[i]) {
            return false;
        }    
    }
    
    return true;
}

static struct dtn_base*
__get_connected_nexus(struct dtn_base* base)
{
    struct dtn_base* current;

    current = base;
    while (current && !current->intr_neuxs) {
        current = current->parent;
    }

    return current;
}

void
dt_resolve_interrupt_map(struct dtn* node)
{
    struct dt_intr_map* imap;
    struct dtpropi it;
    struct dtp_val val;
    struct dt_intr_mapent *ent;

    unsigned int keysize, parent_keysize;
    
    imap = node->intr.map;
    if (likely(!imap)) {
        return;
    }

    keysize = __interrupt_keysize(&node->base);
    __prepare_key(&imap->key_mask, imap->raw_mask.encoded, keysize);

    dtpi_init(&it, &imap->raw);

    while (dtpi_has_next(&it)) 
    {
        ent = valloc(sizeof(*ent));

        dtpi_next_val(&it, &val, keysize);
        __prepare_key(&ent->key, val.encoded, keysize);

        ent->parent = &dtpi_next_hnd(&it)->base;

        parent_keysize = __interrupt_keysize(ent->parent);
        dtpi_next_val(&it, &ent->parent_props, parent_keysize);

        llist_append(&imap->mapent, &ent->ents);        
    };

    imap->resolved = true;
}

struct dtp_val*
dt_resolve_interrupt(struct dtn* node)
{
    struct dtn_base* nexus;
    struct dt_intr_node* i_nexus, *i_node;
    struct dt_speckey key;
    unsigned int keylen;

    if (!node->intr.intr.valid) {
        return NULL;
    }

    nexus = __get_connected_nexus(&node->base);
    i_nexus = &BASE_TO_DTNODE(nexus)->intr;
    i_node  = &node->intr;

    if (!nexus) {
        return &i_node->intr.arr;
    }

    keylen = __interrupt_keysize(nexus);
    key = (struct dt_speckey) {
        .val = valloc(keylen * sizeof(int)),
        .size = keylen
    };

    memcpy( key.val, 
            node->reg.encoded, nexus->addr_c * sizeof(int));
    
    memcpy(&key.val[nexus->addr_c],
            i_node->intr.arr.encoded, nexus->intr_c * sizeof(int));

    dt_speckey_mask(&key, &i_nexus->map->key_mask);

    struct dt_intr_mapent *pos, *n;

    llist_for_each(pos, n, &i_nexus->map->mapent, ents) {
        if (__compare_key(&pos->key, &key))
        {
            __destory_key(&key);
            return &pos->parent_props;
        }
    }

    __destory_key(&key);
    return NULL;
}

bool
parse_stdintr_prop(struct fdt_iter* it, struct dt_intr_node* node)
{
    struct dt_intr_map* map;

    if (propeq(it, "interrupt-map")) {
        map = __get_map(node);
        __mkprop_ptr(it, &map->raw);
    }

    else if (propeq(it, "interrupt-map-mask")) {
        map = __get_map(node);
        __mkprop_ptr(it, &map->raw_mask);
    }

    else if (propeq(it, "interrupt-parent")) {
        node->parent_hnd = __prop_getu32(it);
    }

    else if (propeq(it, "interrupt-extended")) {
        node->intr.extended = true;
        __mkprop_ptr(it, &node->intr.arr);
    }

    else if (!node->intr.extended && propeq(it, "interrupts")) {
        node->intr.valid = true;
        __mkprop_ptr(it, &node->intr.arr);
    }

    else {
        return false;
    }

    return true;
}