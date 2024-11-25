#include "devtree.h"
#include <lunaix/mm/valloc.h>

#define key_mkwr(key)   \
            (struct dtspec_key*)(key)
#define call_op(dtn, ops, name)    \
            ((ops)->name ? (ops)->name(dtn) : NULL)

struct dtspec_map*
dtspec_create(struct dtn* node, struct dtp_val* map,
              const struct dtspec_create_ops* ops)
{
    struct dtpropi it;
    struct dtspec_map* spec;
    struct dtspec_mapent* ent;
    struct dtp_val val, *field;
    int keysz, p_keysz;

    assert(ops->child_keysz);
    assert(ops->parent_keysz);

    keysz = ops->child_keysz(node);
    spec = vzalloc(sizeof(*spec));

    field = call_op(node, ops, get_mask);
    if (field) {
        dtp_speckey(key_mkwr(&spec->mask), field);
    }

    field = call_op(node, ops, get_passthru);
    if (field) {
        dtp_speckey(key_mkwr(&spec->pass_thru), field);
    }
    
    llist_init_head(&spec->ents);

    dtpi_init(&it, map);
    while (dtpi_has_next(&it))
    {
        ent = vzalloc(sizeof(*ent));

        dtpi_next_val(&it, &val, keysz);
        dtp_speckey(key_mkwr(&ent->child_spec), &val);

        ent->parent = dtpi_next_hnd(&it);
        p_keysz = ops->parent_keysz(ent->parent);

        dtpi_next_val(&it, &val, p_keysz);
        dtp_speckey(key_mkwr(&ent->parent_spec), &val);

        changeling_ref(dt_mobj(ent->parent));
        llist_append(&spec->ents, &ent->ents);
    }

    return spec;
}

static bool
__try_match(struct dtspec_key* given, struct dtspec_key* against)
{
    for (int i = 0; i < given->size; i++)
    {
        if (given->val[i] != against->val[i])
            return false;
    }
    
    return true;
}

struct dtspec_mapent*
dtspec_lookup(struct dtspec_map* map, struct dtspec_key* key)
{
    struct dtspec_mapent *pos, *n;
    struct dtspec_key scratch = {};

    dtspec_cpykey(&scratch, key);
    dtspec_applymask(map, &scratch);

    llist_for_each(pos, n, &map->ents, ents)
    {
        if (__try_match(&scratch, &pos->child_spec))
        {
            dtspec_freekey(&scratch);
            return pos;
        }
    }

    dtspec_freekey(&scratch);
    return NULL;
}

void
dtspec_applymask(struct dtspec_map* map, struct dtspec_key* key)
{
    for (int i = 0; i < map->mask.size; i++)
    {
        key->val[i] &= map->mask.val[i];
    }
}

void
dtspec_free(struct dtspec_map* map)
{
    struct dtspec_mapent *pos, *n;
    
    llist_for_each(pos, n, &map->ents, ents)
    {
        changeling_unref(dt_mobj(pos->parent));
        vfree(pos);
    }

    vfree(map);
}

void
dtspec_cpykey(struct dtspec_key* dest, struct dtspec_key* src)
{    
    if (dtspec_nullkey(src)) {
        return;
    }

    int sz = sizeof(int) * src->size;

    dest->val = valloc(sz);
    dest->size = src->size;
    memcpy(dest->val, src->val, sz);
}

void
dtspec_freekey(struct dtspec_key* key)
{
    if (dtspec_nullkey(key)) {
        return;
    }

    vfree(key->val);
    key->size = 0;
}