#ifndef __LUNAIX_KP_RECORDS_H
#define __LUNAIX_KP_RECORDS_H

#include <lunaix/ds/llist.h>
#include <lunaix/time.h>

struct kp_entry
{
    struct llist_header ents;
    int lvl;
    time_t time;
    char* content;
};
#define KP_ENT_SIZE sizeof(struct kp_entry)

struct kp_records
{
    struct kp_entry kp_ents;

    struct llist_header* kp_ent_wp;

    int max_recs;
    int cur_recs;
    int log_lvl;
};
#define KP_RECS_SIZE sizeof(struct kp_records)

struct kp_records*
kprec_create(int max_recs);

struct kp_entry*
kprec_put(struct kp_records*, int lvl, char* content, size_t len);

#endif /* __LUNAIX_KP_RECORDS_H */
