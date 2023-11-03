#include <klibc/string.h>
#include <lunaix/clock.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/spike.h>
#include <lunaix/syslog.h>

#include "kp_records.h"

struct kp_records*
kp_rec_create(int max_recs)
{
    struct kp_records* recs = (struct kp_records*)valloc(KP_RECS_SIZE);

    *recs = (struct kp_records){ .max_recs = max_recs,
                                 .log_lvl = KLOG_DEBUG,
                                 .kp_ent_wp = &recs->kp_ents.ents };

    llist_init_head(&recs->kp_ents.ents);

    return recs;
}

static inline int
kp_recs_full(struct kp_records* recs)
{
    return recs->cur_recs >= recs->max_recs;
}

void
kp_rec_put(struct kp_records* recs, int lvl, char* content, size_t len)
{
    assert(len < 256);

    struct kp_entry* ent;
    if (!kp_recs_full(recs)) {
        assert(recs->kp_ent_wp == &recs->kp_ents.ents);

        ent = (struct kp_entry*)vzalloc(KP_ENT_SIZE);
        llist_append(&recs->kp_ents.ents, &ent->ents);
        recs->cur_recs++;
    } else {
        recs->kp_ent_wp = recs->kp_ent_wp->next;
        ent = container_of(recs->kp_ent_wp, struct kp_entry, ents);
    }

    vfree_safe(ent->content);

    char* _content = valloc(len);
    memcpy(_content, content, len);

    ent->lvl = lvl;
    ent->content = _content;
    ent->time = clock_systime();
}