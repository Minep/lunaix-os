#include "lunaix/compiler.h"
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

static force_inline struct rbnode*
__rb(struct sub_interval* node)
{
    if (unlikely(!node))
        return NULL;
    return &node->rb;
}

static force_inline void
__set_param_result_once(struct search_param* param, struct rbnode** leaf_ref, 
                        struct sub_interval* node, unsigned long loc)
{
    if (likely(param->result.node))
        return;

    param->result.slot = leaf_ref;
    param->result.node = node;
    param->hint_start = loc;
}
#define set_result_once(param, loc, node, direction) \
    __set_param_result_once(param, &(node)->rb.direction, node, loc)

static bool 
__locate_slot(struct search_param* param, struct rbnode* root, 
              unsigned curr, unsigned long min, unsigned long max)
{
    struct sub_interval* root_iv;
    bool found;
    int state;
    
    if (unlikely(curr < min || curr + param->size > max))
        return false;

    if (!root)
        return true;

    root_iv = sub_interval(root);
    state = RESTART_DEFAULT;
    found = false;

restart:
    if (curr < root_iv->start) {
        found = __locate_slot(param, root->left, curr, min, root_iv->start);
        if (found) {
            set_result_once(param, curr, root_iv, left);
            goto done;
        }
    }
    
    if (curr > root_iv->end) {
        found = __locate_slot(param, root->right, curr, root_iv->end + 1, max);
        if (found) {
            set_result_once(param, curr, root_iv, right);
            goto done;
        }
    }

    if (unlikely(!param->retry_variant))
        goto done;

    state++;

    if (state == RESTART_RIGHT) {
        curr = root_iv->end + 1;
        goto restart;
    }

    if (state == RESTART_LEFT) {
        if (root_iv->start >= param->size) {
            curr = root_iv->start - param->size;
        }
        goto restart;
    }

done:
    return found;
}

static struct sub_interval*
__query_for_hit(struct rbnode* root, unsigned long loc)
{
    struct sub_interval* node;

    if (unlikely(!root))
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
    return __locate_slot(param, __rb(interval->root),
                         param->hint_start,
                         interval->min_bound, interval->max_bound);
}

static inline void
__rebalance(struct interval* interval, 
            struct sub_interval* parent, struct sub_interval* node)
{
    struct rbnode* maybe_newroot;

    maybe_newroot = 
        rbtree_fixup_insert(__rb(interval->root), __rb(parent), __rb(node));

    interval->root = sub_interval(maybe_newroot);
    interval->root_ref = &interval->root->rb;
}


static inline bool
__try_insert(struct interval* interval, struct sub_interval* node, bool fixed)
{
    struct search_param param;
    bool found;

    param.hint_start = node->start;
    param.size = sub_interval_span(node);
    param.retry_variant = !fixed;
    param.result.node = NULL;

    found = __search_for_slot(interval, &param);
    if (!found) {
        return false;
    }

    if (likely(interval->root)) {
        node->start = param.hint_start;
        node->end = param.hint_start + param.size - 1;

        *param.result.slot = __rb(node);
    }
    else {
        interval->root = node;
    }

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
    interval->root_ref = &interval->root->rb;
}

struct sub_interval*
interval_query(struct interval* interval, unsigned long location) 
{
    return __query_for_hit(__rb(interval->root), location);
}


static force_inline bool
__overlapped(struct sub_interval* node, unsigned long start, unsigned long end)
{
    if (node->start >= start && node->start < end)
        return true;

    if (node->end >= start && node->end < end)
        return true;

    return false;
}

static struct sub_interval*
__locate_pivot(struct rbnode* node, unsigned long start, unsigned long end)
{
    struct sub_interval* si;

    if (!node)
        return NULL;

    si = sub_interval(node);

    if (start > si->end)
        return __locate_pivot(node->right, start, end);

    if (end < si->start)
        return __locate_pivot(node->left, start, end);

    return si;
}

static struct sub_interval*
__min_edge_of_pivot(struct rbnode* pivot, unsigned long start, unsigned long end)
{
    struct sub_interval* si, *found;

    if (!pivot)
        return NULL;

    si = sub_interval(pivot);

    if (si->start > start) {
        found = __min_edge_of_pivot(pivot->left, start, end);
    }
    else {
        found = __min_edge_of_pivot(pivot->right, start, end);
    }

    if (!found && __overlapped(si, start, end))
        return si;

    return found;
}

void
interval_search_begin(struct interval* interval, struct interval_iter* it,
                      unsigned long span_start, unsigned long span_end) 
{
    struct sub_interval* this;
    struct rbnode *parent;

    /**
     * pre-locate the pivot in case the range is bury deep in tree.
     * so we can save some stack space.
     * */
    this = __locate_pivot(__rb(interval->root), span_start, span_end);

    *it = (struct interval_iter) {
        .root = interval,
        .span_start = span_start,
        .span_end = span_end,
    };

    /*
     * we went extra trouble to save the reference of pivot,
     * so we can allow deletion while in traversal.
     * */
    if (!this) {
        it->pivot_ref = NULL;
        return;
    }

    parent = rbnode_parent(&this->rb);

    if (!parent) {
        it->pivot_ref = &interval->root_ref;
        return;
    }

    if (parent->right == &this->rb) {
        it->pivot_ref = &parent->right;
    }
    else {
        it->pivot_ref = &parent->left;
    }
}

struct sub_interval*
interval_search_next(struct interval_iter* iter) 
{
    struct sub_interval* next;

    if (unlikely(!iter->pivot_ref))
        return NULL;

    if (unlikely(iter->span_start >= iter->span_end))
        return NULL;

    next = __min_edge_of_pivot(*iter->pivot_ref, iter->span_start, iter->span_end);
    if (next) {
        iter->span_start = next->end + 1;
    }

    return next;
}

void
interval_search_end(struct interval_iter* iter) 
{
    // Do nothing
}
