#ifndef __LUNAIX_CHANGELING_H
#define __LUNAIX_CHANGELING_H

#include <lunaix/types.h>
#include <lunaix/ds/llist.h>
#include <lunaix/spike.h>
#include <lunaix/ds/hstr.h>

#define CHLG_ID   (unsigned short)0x4c43

#define morphable(struct_name)              chlg_##struct_name
#define __morpher_id(struct_name, field)    morphable(struct_name)
#define morpher_id(morpher)                 __morpher_id(morpher)

enum changeling_idents
{
    morphable(anon) = 0,
    #include <listings/changeling.lst>
};

/**
 * changeling - a proud changeling of her majesty :)
 *              changeling reginae superbum
 */
struct changeling 
{
    struct {
        union {
            struct
            {
                unsigned short sig;
                unsigned short ident;
            };

            unsigned int magic;
        };
    };

    int ref;
    unsigned int uid;
    
    struct changeling *parent;
    struct llist_header sibs;
    struct llist_header subs;
    struct hstr name;
};

typedef struct changeling morph_t;

#define morphable_attrs(struct_name, field)     struct_name, field
#define morphed_ptr(ptr_like)                   ((morph_t*)__ptr(ptr_like))
#define morpher_uid(mobj)                       ((mobj)->uid)
#define morpher_name(mobj)                      ((mobj)->name.value)

#define __changeling_morph(parent, chlg, name, struct_name, field)             \
    ({                                                                         \
        changeling_init(parent, &(chlg), chlg_##struct_name, name);            \
        &(chlg);                                                               \
    })

#define __morph_type_of(chlg, struct_name, field)                              \
        ((chlg)->ident == chlg_##struct_name)

#define __changeling_cast(chlg, struct_name, field)                            \
        container_of(chlg, struct struct_name, field)

#define __changeling_try_reveal(chlg, struct_name, field)                      \
    ({                                                                         \
        struct struct_name* __r = NULL;                                        \
        if (__changeling_of(chlg, struct_name, field))                         \
            __r = __changeling_cast(chlg, struct_name, field);                 \
        __r;                                                                   \
    })

#define __changeling_reveal(chlg, struct_name, field)                          \
    ({                                                                         \
        struct struct_name* __r;                                               \
        __r = __changeling_try_reveal(chlg, struct_name, field);               \
        assert(__r); __r;                                                      \
    })

#define __changeling_of(chlg, struct_name, field)                              \
        (chlg && (chlg)->sig == CHLG_ID                                        \
              && __morph_type_of(chlg, struct_name, field))


#define changeling_morph(parent, chlg, name, morpher)                          \
            __changeling_morph(parent, chlg, name, morpher)


#define changeling_morph_anon(parent, chlg, morpher)                           \
            __changeling_morph(parent, chlg, NULL, morpher)


#define is_changeling(maybe_chlg)                                              \
            ((maybe_chlg) && morphed_ptr(maybe_chlg)->sig == CHLG_ID)


#define morph_type_of(chlg, morpher)                                           \
            __morph_type_of(chlg, morpher)

#define changeling_of(chlg, morpher)                                           \
            __changeling_of(chlg, morpher)

#define changeling_try_reveal(chlg, morpher)                                   \
            __changeling_try_reveal(chlg, morpher)

#define changeling_reveal(chlg, morpher)                                       \
            __changeling_reveal(chlg, morpher)

#define changeling_for_each(pos, n, parent)                                    \
            llist_for_each(pos, n, &(parent)->subs, sibs)

static inline morph_t*
changeling_ref(morph_t* chlg)
{
    return ({ chlg->ref++; chlg; });
}

static inline morph_t*
changeling_unref(morph_t* chlg)
{
    assert(chlg->ref > 0);
    return ({ chlg->ref--; chlg; });
}

static inline void
changeling_detach(morph_t* obj)
{
    if (llist_empty(&obj->sibs)) {
        return;
    }

    changeling_unref(obj);
    llist_delete(&obj->sibs);
}

static inline morph_t*
changeling_attach(morph_t* parent, morph_t* obj)
{
    changeling_detach(obj);

    llist_append(&parent->subs, &obj->sibs);
    obj->parent = parent;

    return changeling_ref(obj);
}

static inline void
changeling_isolate(morph_t* obj)
{
    changeling_detach(obj);
    assert(obj->ref == 0);
}

void
changeling_init(morph_t* parent, morph_t* chlg, 
                unsigned int id, const char* name);

morph_t*
changeling_spawn(morph_t* parent, const char* name);

morph_t*
changeling_find(morph_t* parent, struct hstr* str);

morph_t*
changeling_get_at(morph_t* parent, int index);

morph_t*
changeling_setname(morph_t* chlg, const char* name);

#endif /* __LUNAIX_CHANGELING_H */
