#include <klibc/string.h>
#include <lunaix/clock.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/spike.h>
#include <lunaix/syslog.h>
#include <lunaix/compiler.h>
#include <lunaix/mm/emrgmem.h>

#include "kp_records.h"

struct kp_records*
kprec_create(int max_recs)
{
    struct kp_records* recs = (struct kp_records*)valloc(KP_RECS_SIZE);

    *recs = (struct kp_records){ .max_recs = max_recs,
                                 .log_lvl = KLOG_DEBUG,
                                 .kp_ent_wp = &recs->kp_ents.ents };

    llist_init_head(&recs->kp_ents.ents);

    return recs;
}

static inline int
kprecs_full(struct kp_records* recs)
{
    return recs->cur_recs >= recs->max_recs;
}

static inline bool
__require_emrgmem(int level) 
{
    return level == KLOG_FATAL;
}

static inline void*
__alloc(int level, size_t size)
{
    if (likely(!__require_emrgmem(level)))
        return vzalloc(size);

    return emalloc(size);
}

static inline void
__free(int level, void* data)
{
    if (unlikely(__require_emrgmem(level)))
        vfree_safe(data);
    else 
        emfree(data);
}

struct kp_entry*
kprec_put(struct kp_records* recs, int lvl, const char* content, size_t len)
{
    assert(len < 256);

    struct kp_entry* ent;
    char* _content;

    if (!kprecs_full(recs)) {
        assert(recs->kp_ent_wp == &recs->kp_ents.ents);

        ent = (struct kp_entry*)__alloc(lvl, KP_ENT_SIZE);
        llist_append(&recs->kp_ents.ents, &ent->ents);
        recs->cur_recs++;
    } else {
        recs->kp_ent_wp = recs->kp_ent_wp->next;
        ent = container_of(recs->kp_ent_wp, struct kp_entry, ents);       
        __free(ent->lvl, ent->content);
    }
    
    _content = __alloc(lvl, len);
    if (!_content || !ent)
        return NULL;

    memcpy(_content, content, len);

    ent->lvl = lvl;
    ent->content = _content;
    ent->time = clock_systime();
    ent->len = len;

    return ent;
}
