#include <lunaix/changeling.h>
#include <lunaix/mm/valloc.h>

#include <klibc/string.h>

static DEFINE_LLIST(chrysallidis);
static unsigned int current_id = 0;

void
changeling_init(morph_t* parent, morph_t* chlg, 
                unsigned int id, const char* name)
{
    chlg->sig     = CHLG_ID;
    chlg->ident   = id;
    chlg->ref     = 1;
    chlg->uid     = current_id++;
    
    changeling_setname(chlg, name);

    if (!parent) {
        llist_append(&chrysallidis, &chlg->sibs);
    } else {
        llist_append(&parent->subs, &chlg->sibs);
    }

    llist_init_head(&chlg->subs);
}

morph_t*
changeling_spawn(morph_t* parent, const char *name)
{
    morph_t* changeling;

    changeling = valloc(sizeof(morph_t));
    changeling_init(parent, changeling, chlg_anon, name);
    
    return changeling;
}

morph_t*
changeling_find(morph_t* parent, struct hstr* str)
{
    morph_t *p, *n;
    llist_for_each(p, n, &parent->subs, sibs)
    {
        if (HSTR_EQ(&p->name, str)) {
            return p;
        }
    }

    return NULL;
}

morph_t*
changeling_get_at(morph_t* parent, int index)
{
    morph_t *p, *n;
    llist_for_each(p, n, &parent->subs, sibs)
    {
        if (!(index--)) {
            return p;
        }
    }

    return NULL;
}

morph_t*
changeling_setname(morph_t* chlg, const char* name)
{
    if (name) {
        chlg->name = HSTR(name, strlen(name));
        hstr_rehash(&chlg->name, HSTR_FULL_HASH);
    } else {
        chlg->name = HHSTR(NULL, 0, 0);
    }

    return chlg;
}
