/**
 * @file llist.h
 * @author Lunaixsky
 * @brief This doubly linked cyclic list is adopted from Linux kernel
 * <linux/list.h>
 * @version 0.1
 * @date 2022-03-12
 *
 * @copyright Copyright (c) 2022
 *
 */
#ifndef __LUNAIX_LLIST_H
#define __LUNAIX_LLIST_H

#include <lunaix/common.h>

struct llist_header
{
    struct llist_header* prev;
    struct llist_header* next;
};

static inline void
__llist_add(struct llist_header* elem,
            struct llist_header* prev,
            struct llist_header* next)
{
    next->prev = elem;
    elem->next = next;
    elem->prev = prev;
    prev->next = elem;
}

static inline void
llist_init_head(struct llist_header* head)
{
    head->next = head;
    head->prev = head;
}

static inline void
llist_append(struct llist_header* head, struct llist_header* elem)
{
    __llist_add(elem, head->prev, head);
}

static inline void
llist_prepend(struct llist_header* head, struct llist_header* elem)
{
    __llist_add(elem, head, head->next);
}

static inline void
llist_delete(struct llist_header* elem)
{
    elem->prev->next = elem->next;
    elem->next->prev = elem->prev;

    // make elem orphaned
    elem->prev = elem;
    elem->next = elem;
}

static inline int
llist_empty(struct llist_header* elem)
{
    return elem->next == elem && elem->prev == elem;
}

#define DEFINE_LLIST(name)                                                     \
    struct llist_header name = (struct llist_header)                           \
    {                                                                          \
        .prev = &name, .next = &name                                           \
    }

/**
 * list_entry - get the struct for this entry
 * @ptr:	the &struct list_head pointer.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the list_struct within the struct.
 */
#define list_entry(ptr, type, member) container_of(ptr, type, member)

/**
 * list_for_each_entry	-	iterate over list of given type
 * @pos:	the type * to use as a loop counter.
 * @head:	the head for your list.
 * @member:	the name of the list_struct within the struct.
 */
#define llist_for_each(pos, n, head, member)                                   \
    for (pos = list_entry((head)->next, typeof(*pos), member),                 \
        n = list_entry(pos->member.next, typeof(*pos), member);                \
         &pos->member != (head);                                               \
         pos = n, n = list_entry(n->member.next, typeof(*n), member))

struct hlist_node
{
    struct hlist_node *next, **pprev;
};

static inline void
hlist_delete(struct hlist_node* node)
{
    if (!node->pprev)
        return;
    *node->pprev = node->next;
    node->next = 0;
    node->pprev = 0;
}

static inline void
hlist_add(struct hlist_node** head, struct hlist_node* node)
{
    node->next = *head;
    if (*head) {
        (*head)->pprev = &node->next;
    }
    node->pprev = head;
    *head = node;
}
#endif /* __LUNAIX_LLIST_H */
