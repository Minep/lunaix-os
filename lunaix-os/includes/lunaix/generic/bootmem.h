#ifndef __LUNAIX_BOOTMEM_H
#define __LUNAIX_BOOTMEM_H

#include <lunaix/types.h>

/*
 * bootmem:
 *
 * Architecture-defined memory manager during boot stage. 
 * 
 * It provide basic memory service before kernel's mm
 * context is avaliable. As it's name stated, this is 
 * particularly useful for allocating temporary memory 
 * to get essential things done in the boot stage.
 * 
 * Implementation detail is not enforced by Lunaix, but it
 * is recommend that such memory pool should be reclaimed
 * after somewhere as earlier as possible (should not later
 * than the first process spawning)
 * 
 */

void*
bootmem_alloc(unsigned int size);

void
bootmem_free(void* ptr);

#endif /* __LUNAIX_BOOTMEM_H */
