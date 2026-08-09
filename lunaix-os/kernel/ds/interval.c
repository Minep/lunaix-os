#include "lunaix/compiler.h"
#include "lunaix/ds/rbtree.h"
#include "lunaix/types.h"
#include <lunaix/ds/interval.h>

struct search_param
{
    struct {
        struct sub_interval* node;
        struct rbnode** slot;
    } result;

    unsigned long hint_start;
    unsigned long size;
    bool retry_variant;
};

#define RESTART_DEFAULT 0
#define RESTART_LEFT 1
#define RESTART_RIGHT 2
#define RESTART_END 3

static bool 
__locate_slot(struct search_param* slot, struct rbnode* root, 
              unsigned curr, unsigned long min, unsigned long max)
{
    struct sub_interval* root_iv;
    bool found;
    int state;

    if (!root) {
        return true;
    }

    root_iv = sub_interval(root);
    state = RESTART_DEFAULT;
    found = false;

    if (unlikely(curr < min || curr + slot->size >= max))
        return false;

restart:
    if (curr < root_iv->start) {
        found = __locate_slot(slot, root->left, curr, min, root_iv->start);
        if (found) {
            slot->result.slot = &root->left;
            slot->result.node = root_iv;
            slot->hint_start = curr;
            goto done;
        }
    }
    
    if (curr > root_iv->end) {
        found = __locate_slot(slot, root->right, curr, root_iv->end, max);
        if (found) {
            slot->result.slot = &root->left;
            slot->result.node = root_iv;
            slot->hint_start = curr;
            goto done;
        }
    }

    if (unlikely(!slot->retry_variant))
        goto done;

    state++;

    if (state == RESTART_RIGHT) {
        curr = (root_iv->end + max + 1) / 2;
        goto restart;
    }

    if (state == RESTART_LEFT) {
        curr = (min + root_iv->start + 1) / 2;
        goto restart;
    }

done:
    return found;
}

static struct sub_interval*
__query_for_hit(struct rbnode* root, unsigned long loc)
{
    struct sub_interval* node;

    if (!root)
        return NULL;

    node = sub_interval(root);

    if (loc < node->start) 
        return __query_for_hit(node->rb.left, loc);
    
    if (loc > node->end) 
        return __query_for_hit(node->rb.right, loc);

    return node;
}

static inline bool
__search_for_slot(struct interval* interval, struct search_param* param)
{
    return __locate_slot(param, &interval->root->rb,
                         param->hint_start,
                         interval->min_bound, interval->max_bound);
}

static inline void
__rebalance(struct interval* interval, 
            struct sub_interval* parent, struct sub_interval* node)
{
    struct rbnode* maybe_newroot;

    maybe_newroot = 
        rbtree_fixup_insert(&interval->root->rb, &parent->rb, &node->rb);

    interval->root = sub_interval(maybe_newroot);
}


static inline bool
__try_insert(struct interval* interval, struct sub_interval* node, bool fixed)
{
    struct search_param param;
    bool found;

    param.hint_start = node->start;
    param.size = sub_interval_span(node);
    param.retry_variant = !fixed;

    found = __search_for_slot(interval, &param);
    if (!found) {
        return false;
    }

    node->start = param.hint_start;
    node->end = param.hint_start + param.size - 1;

    *param.result.slot = &node->rb;

    __rebalance(interval, param.result.node, node);

    return true;
}

bool
interval_alloc_insert(struct interval* interval, struct sub_interval* node) 
{
    return __try_insert(interval, node, false);
}

bool
interval_insert(struct interval* interval, struct sub_interval* node) 
{
    return __try_insert(interval, node, true);
}

void
interval_delete(struct interval* interval, struct sub_interval* node) 
{
    struct rbnode* maybe_newroot;

    maybe_newroot = rbtree_delete(&interval->root->rb, &node->rb);

    interval->root = sub_interval(maybe_newroot);
}

struct sub_interval*
interval_query(struct interval* interval, unsigned long location) 
{
    return __query_for_hit(&interval->root->rb, location);
}


static force_inline bool
__overlapped(struct sub_interval* node, unsigned long start, unsigned long end)
{
    if (node->start >= start && node->start <= end)
        return true;

    if (node->end >= start && node->end <= end)
        return true;

    return false;
}

static struct sub_interval*
__locate_first(struct rbnode* node, unsigned long start, unsigned long end)
{
    struct sub_interval* maybe_first;

    if (!node)
        return NULL;

    maybe_first = __locate_first(node->left, start, end);
    if (maybe_first)
        return maybe_first;
    
    maybe_first = sub_interval(node);
    if (__overlapped(maybe_first, start, end))
        return maybe_first;
    
    return __locate_first(node->right, start, end);
}

static struct sub_interval*
__locate_next(struct rbnode* node, unsigned long start, unsigned long end)
{
    struct sub_interval *curr, *next;

    while (node) {
        curr = sub_interval(node);

        if (__overlapped(curr, start, end))
            return NULL;

        next = __locate_first(curr->rb.right, start, end);
        if (next)
            return next;

        node = rbnode_parent(node);
    }

    return NULL;
}

void
interval_search_begin(struct interval* interval, struct interval_iter* it,
                      unsigned long span_start, unsigned long span_end) 
{
    *it = (struct interval_iter) {
        .root = interval,
        .span_start = span_start,
        .span_end = span_end,
        .prev = __locate_first(&interval->root->rb, span_start, span_end)
    };
}

struct sub_interval*
interval_search_next(struct interval_iter* iter) 
{
    struct sub_interval* next;

    if (unlikely(!iter->prev))
        return NULL;

    next = __locate_next(&iter->prev->rb, iter->span_start, iter->span_end);
    iter->prev = next;

    return next;
}

void
interval_search_end(struct interval_iter* iter) 
{
    // Do nothing
}
