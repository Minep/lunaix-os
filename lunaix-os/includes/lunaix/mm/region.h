#ifndef __LUNAIX_REGION_H
#define __LUNAIX_REGION_H

#include <lunaix/mm/mm.h>

struct mm_region*
region_add(struct llist_header* lead, ptr_t start, ptr_t end, u32_t attr);

void
region_release_all(struct llist_header* lead);

struct mm_region*
region_get(struct llist_header* lead, unsigned long vaddr);

void
region_copy(struct llist_header* src, struct llist_header* dest);

#endif /* __LUNAIX_REGION_H */
