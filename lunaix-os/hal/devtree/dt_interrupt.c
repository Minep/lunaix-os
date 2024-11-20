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
__prepare_key(struct dt_intr_mapkey* key, 
              dt_enc_t key_raw, unsigned int keylen)
{
    *key = (struct dt_intr_mapkey) {
        .val = valloc(keylen * sizeof(int)),
        .size = keylen
    };

    memcpy(key->val, key_raw, keylen * sizeof(int));
}

static inline void
__destory_key(struct dt_intr_mapkey* key)
{
    vfree(key->val);
}

static inline unsigned int
__interrupt_keysize(struct dt_node_base* base)
{
    return dt_size_cells(base) + base->intr_c;
}

static void
__mask_key(struct dt_intr_mapkey* k, struct dt_intr_mapkey* mask)
{
    for (unsigned int i = 0; i < k->size; i++)
    {
        k->val[i] &= mask->val[i];
    }
}

static bool
__compare_key(struct dt_intr_mapkey* k1, struct dt_intr_mapkey* k2)
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

static struct dt_node_base*
__get_connected_nexus(struct dt_node_base* base)
{
    struct dt_node_base* current;

    current = base;
    while (current && !current->intr_neuxs) {
        current = current->parent;
    }

    return current;
}

void
dt_resolve_interrupt_map(struct dt_node* node)
{
    struct dt_intr_node* inode;
    struct dt_intr_map* imap;
    struct dt_prop_iter iter;

    struct dt_intr_mapent *ent;

    unsigned int keysize, parent_keysize;
    unsigned int advance;
    dt_phnd_t parent_hnd;
    
    inode = &node->intr;
    if (likely(!inode->map)) {
        return;
    }

    imap = inode->map;
    keysize = __interrupt_keysize(&node->base);
    
    __prepare_key(&imap->key_mask, imap->raw_mask.encoded, keysize);

    dt_decode(&iter, &node->base, &imap->raw, 1);

    advance = 0;
    do 
    {
        advance = keysize;
        ent = valloc(sizeof(*ent));

        __prepare_key(&ent->key, iter.prop_loc, advance);
        __mask_key(&ent->key, &imap->key_mask);

        parent_hnd = dtprop_to_phnd(dtprop_extract(&iter, advance));
        ent->parent = &dt_resolve_phandle(parent_hnd)->base;

        advance++;
        parent_keysize = __interrupt_keysize(ent->parent);

        ent->parent_props.encoded = dtprop_extract(&iter, advance);
        ent->parent_props.size = parent_keysize;

        advance += parent_keysize;

        llist_append(&imap->mapent, &ent->ents);
        
    } while (dtprop_next_n(&iter, advance));

    imap->resolved = true;
}

struct dt_prop_val*
dt_resolve_interrupt(struct dt_node* node)
{
    struct dt_node_base* nexus;
    struct dt_intr_node* i_nexus, *i_node;
    struct dt_intr_mapkey key;
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
    key = (struct dt_intr_mapkey) {
        .val = valloc(keylen * sizeof(int)),
        .size = keylen
    };

    memcpy( key.val, 
            node->reg.encoded, dt_addr_cells(nexus) * sizeof(int));
    
    memcpy(&key.val[dt_addr_cells(nexus)],
            i_node->intr.arr.encoded, nexus->intr_c * sizeof(int));

    __mask_key(&key, &i_nexus->map->key_mask);

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