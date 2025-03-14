#include <hal/devtreem.h>
#include <lunaix/device.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/syslog.h>
#include <lunaix/owloysius.h>
#include <lunaix/status.h>

#include <klibc/string.h>

LOG_MODULE("dtm")

static DECLARE_HASHTABLE(registry,    32);
static struct device_cat* dt_category;

struct figura
{
    char val;
    bool optional;
};

#define hash(def)   ((unsigned int)__ptr(def))

static struct dtm_driver_record*
__locate_record(struct device_def* def)
{
    struct dtm_driver_record *p, *n;

    hashtable_hash_foreach(registry, hash(def), p, n, node)
    {
        if (p->def == def) {
            return p;
        }
    }

    return NULL;
}


static inline void
__get_patternlet(const char* str, unsigned i, size_t len, 
                 struct figura* fig)
{
    fig->optional = (i + 1 < len && str[i + 1] == '?');

    if (i >= len) {
        fig->val = 0;
        return;
    }

    fig->val = str[i];
}

/**
 * A simplified regular expression matcher:
 *      1. '*' matches any substring including empty string
 *      2. '?' mark the prefixed character optional (an epsilon transition)
 */
static bool
__try_match(const char* str, const char* pattern, size_t pat_sz)
{
    unsigned j = 0, i = 0;
    int saved_star = -1, saved_pos = 0;
    size_t str_sz = strlen(str);

    char c;
    struct figura p0, p1;

    while (i < str_sz) {
        c = str[i++];
        __get_patternlet(pattern, j, pat_sz, &p0);
        __get_patternlet(pattern, j + 1, pat_sz, &p1);

        if (p0.val == c) {
            j += 1 + !!p0.optional;
            saved_pos = (int)i;
            continue;
        }

        if (p0.val == '*') {
            saved_pos = i;
            saved_star = (int)j;

            if (p1.optional || p1.val == c) {
                ++j; --i;
            }

            continue;
        }

        if (p0.optional) {
            --i; j += 2;
            continue;
        }

        if (saved_star < 0) {
            return false;
        }

        j = (unsigned)saved_star;
        i = (unsigned)saved_pos;
    }
    
    return j + 1 >= pat_sz;
}

static struct device_meta*
__try_create_categorical(struct dtn_base *p)
{
    if (!p) return NULL;

    struct device_meta* parent = NULL;
    struct device_cat* cat;

    parent = __try_create_categorical(p->parent);
    parent = parent ?: dev_meta(dt_category);

    if (!p->compat.size) {
        return parent;
    }

    if (p->binded_obj) {
        cat = changeling_reveal(p->binded_obj, devcat_morpher);
    }
    else {
        cat = device_addcat(parent, HSTR_VAL(dt_mobj(p)->name));
        p->binded_obj = dev_mobj(cat);
    }

    return dev_meta(cat);
}

static bool
compat_matched(struct dtm_driver_record* rec, struct dtn_base *base)
{
    const char *compat;
    struct dtm_driver_info *p, *n;

    list_for_each(p, n, rec->infos.first, node)
    {
        size_t pat_len = strlen(p->pattern);
        dtprop_strlst_foreach(compat, &base->compat)
        {
            if (__try_match(compat, p->pattern, pat_len)) {
                return true;
            }
        }
    }

    return false;
}

static int
dtm_try_create_from(struct device_def* def)
{
    int err;
    const char *name;
    struct dt_context* dtctx;
    struct dtm_driver_record* rec;
    struct dtn_base *p, *n;
    
    dtctx = dt_main_context();

    rec = __locate_record(def);
    if (!rec) {
        return ENOENT;
    }

    llist_for_each(p, n, &dtctx->nodes, nodes)
    {
        if (!p->compat.size) {
            continue;
        }

        if (!compat_matched(rec, p)) {
            continue;
        }

        __try_create_categorical(p);

        if ((err = def->create(def, dt_mobj(p)))) {
            name = HSTR_VAL(dt_mobj(p)->name);
            WARN("failed to bind devtree node %s, err=%d", name, err);
        }
    }

    return 0;
}

void
dtm_register_entry(struct device_def* def, const char* pattern)
{
    struct dtm_driver_info* info;
    struct dtm_driver_record* record;
    
    info = valloc(sizeof(*info));
    info->pattern = pattern;

    record = __locate_record(def);
    if (!record) {
        record = valloc(sizeof(*record));
        record->def = def;
        list_head_init(&record->infos);

        hashtable_hash_in(registry, &record->node, hash(def));
    }

    list_add(&record->infos, &info->node);

    device_chain_loader(def, dtm_try_create_from);
}

static void
dtm_init()
{
    hashtable_init(registry);

    dt_category = device_addcat(NULL, "tree");
}
owloysius_fetch_init(dtm_init, on_sysconf);