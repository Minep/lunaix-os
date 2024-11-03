#include <lunaix/changeling.h>
#include <lunaix/mm/valloc.h>

static DEFINE_LLIST(chrysallidis);

void
changeling_init(morph_t* parent, morph_t* chlg, unsigned int id)
{
    chlg->ident = CHLG_ID;
    chlg->sig   = id;
    chlg->ref   = 0;

    if (!parent) {
        llist_append(&chrysallidis, &chlg->sibs);
    } else {
        llist_append(&parent->subs, &chlg->sibs);
    }

    llist_init_head(&chlg->subs);
}

morph_t*
changeling_spawn_anon(morph_t* parent)
{
    morph_t* changeling;

    changeling = valloc(sizeof(morph_t));
    changeling_init(parent, changeling, chlg_anon);
    
    return changeling;
}