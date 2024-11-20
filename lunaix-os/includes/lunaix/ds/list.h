/**
 * @file slist.h
 * @author Lunaixsky
 * @brief This singly linked list is adopted from Linux kernel
 * <linux/llist.h>
 * @version 0.1
 * @date 2022-03-12
 *
 * @copyright Copyright (c) 2024
 *
 */
#ifndef __LUNAIX_LIST_H
#define __LUNAIX_LIST_H

#include <lunaix/types.h>

struct list_head {
	struct list_node *first;
};

struct list_node {
	struct list_node *next;
};

#define DEFINE_LIST(name)	struct list_head name = { .first = NULL }

static inline void 
list_head_init(struct list_head *list)
{
	list->first = NULL;
}

static inline void 
list_node_init(struct list_node *node)
{
	node->next = node;
}

#define slist_entry(ptr, type, member)		\
	container_of(ptr, type, member)

#define member_address_is_nonnull(ptr, member)	\
	((ptr_t)(ptr) + offsetof(typeof(*(ptr)), member) != 0)

#define list_for_each(pos, n, node, member)									\
	for (pos = slist_entry((node), typeof(*pos), member);		       		\
	     	member_address_is_nonnull(pos, member) &&			       		\
			(n = slist_entry(pos->member.next, typeof(*n), member), true); 	\
	     pos = n)

static inline bool 	
__llist_add_batch(struct list_node *new_first,
				  struct list_node *new_last,
				  struct list_head *head)
{
	new_last->next = head->first;
	head->first = new_first;
	return new_last->next == NULL;
}

static inline void
list_add(struct list_head* head, struct list_node* node)
{
	__llist_add_batch(node, node, head);
}

#endif /* __LUNAIX_LIST_H */
