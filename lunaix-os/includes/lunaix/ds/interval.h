#ifndef __LUNAIX_DS_INTERVAL_H
#define __LUNAIX_DS_INTERVAL_H

#include "lunaix/compiler.h"
#include <lunaix/types.h>
#include <lunaix/ds/rbtree.h>

struct sub_interval {
    unsigned long start;
    unsigned long end;

    struct rbnode rb;
};

struct interval {
    struct sub_interval* root;
    struct rbnode* root_ref;
    unsigned long min_bound;
    unsigned long max_bound;
};

static force_inline struct sub_interval*
sub_interval(struct rbnode* node)
{
    return container_of(node, struct sub_interval, rb);
}

static force_inline unsigned long
sub_interval_span(struct sub_interval* sinv)
{
    return sinv->end - sinv->start + 1;
}

static force_inline void
interval_init(struct interval* interval, 
              unsigned long min_bound, unsigned long max_bound)
{
    interval->max_bound = max_bound;
    interval->min_bound = min_bound;
}

bool
interval_insert_node(struct interval* interval, 
                     struct sub_interval* node, bool fixed, int align_bits);

/**
 * Insert the given sub interval identified by [start, end] at the 
 * most suitable location somewhere near the `start`. 
 *
 * Return whether the insertion successful or not.
 */
static inline bool
interval_insert_alloc(struct interval* interval, struct sub_interval* node)
{
    return interval_insert_node(interval, node, false, 0);
}


/**
 * Insert the given sub interval identified by [start, end] at the 
 * EXACT location given by `start` 
 *
 * Return whether the insertion successful or not.
 */
static inline bool
interval_insert_fixed(struct interval* interval, struct sub_interval* node)
{
    return interval_insert_node(interval, node, true, 0);
}

/**
 * Insert the given sub interval identified by [start, end] at the 
 * most suitable location somewhere near the `start`. 
 *
 * Return whether the insertion successful or not.
 */
static inline bool
interval_insert_alloc_aligned(struct interval* interval, 
                              struct sub_interval* node, int align_bits)
{
    return interval_insert_node(interval, node, false, align_bits);
}

/**
 * Insert the given sub interval identified by [start, end] at the 
 * EXACT location given by `start` 
 *
 * Return whether the insertion successful or not.
 */
static inline bool
interval_insert_fixed_aligned(struct interval* interval, 
                        struct sub_interval* node, int align_bits)
{
    return interval_insert_node(interval, node, true, align_bits);
}

/**
 * Delete the given sub interval node from the interval.
 *
 */
void
interval_delete(struct interval* interval, struct sub_interval* node);


struct sub_interval*
interval_query(struct interval* interval, unsigned long location);


struct interval_iter 
{
    struct interval* root;
    struct rbnode** pivot_ref;
    unsigned long span_start;
    unsigned long span_end;
};


/**
 * Start range search on the interval.
 *
 * The range search will find all interval that intersect with 
 * the given range
 *
 */
void
interval_search_begin(struct interval* interval, struct interval_iter* it,
                      unsigned long span_start, unsigned long span_end);

/**
 * Get next result from the range search.
 */
struct sub_interval*
interval_search_next(struct interval_iter* iter);

/**
 * Finalise the range search.
 *
 */
void
interval_search_end(struct interval_iter* iter);

#endif
