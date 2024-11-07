#ifndef __LUNAIX_CHANGELING_H
#define __LUNAIX_CHANGELING_H

#include <lunaix/types.h>
#include <lunaix/ds/llist.h>
#include <lunaix/spike.h>

#define CHLG_ID   (unsigned short)0x4c43

enum changeling_idents
{
    chlg_anon = 0,
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
    
    struct llist_header sibs;
    struct llist_header subs;
};

typedef struct changeling morph_t;


#define morphable_attrs(struct_name, field)     struct_name, field


#define __changeling_morph(parent, chlg, struct_name, field)    \
    ({changeling_init(parent, &(chlg), chlg_##struct_name); &(chlg);})
#define changeling_morph(parent, chlg, morpher)     \
        __changeling_morph(parent, chlg, morpher)


#define __changeling_try_reveal(chlg, struct_name, field)                      \
    ({                                                                         \
        struct struct_name* __r = NULL;                                        \
        if (chlg                                                               \
            && (chlg)->sig == CHLG_ID                                          \
            && (chlg)->ident == chlg_##struct_name                             \
           ) __r = container_of(chlg, struct struct_name, field);              \
        __r;                                                                   \
    })
#define changeling_try_reveal(chlg, morpher)     \
        __changeling_try_reveal(chlg, morpher)


#define __changeling_reveal(chlg, struct_name, field)                          \
    ({                                                                         \
        struct struct_name* __r;                                               \
        __r = __changeling_try_reveal(chlg, struct_name, field);               \
        assert(__r); __r;                                                      \
    })
#define changeling_reveal(chlg, morpher)     \
        __changeling_reveal(chlg, morpher)


#define changeling_for_each(pos, n, parent)     \
        llist_for_each(pos, n, &(parent)->subs, sibs)

static inline morph_t*
changeling_refonce(morph_t* chlg)
{
    return ({ chlg->ref++; chlg; });
}

void
changeling_init(morph_t* parent, morph_t* chlg, unsigned int id);

morph_t*
changeling_spawn_anon(morph_t* parent);

#endif /* __LUNAIX_CHANGELING_H */
