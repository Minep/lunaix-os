#ifndef __LUNAIX_REGION_H
#define __LUNAIX_REGION_H

#include <lunaix/mm/mm.h>
#include <lunaix/process.h>

void region_add(struct proc_info* proc, unsigned long start, unsigned long end, unsigned int attr);

void region_release_all(struct proc_info* proc);

struct mm_region* region_get(struct proc_info* proc, unsigned long vaddr);


#endif /* __LUNAIX_REGION_H */
