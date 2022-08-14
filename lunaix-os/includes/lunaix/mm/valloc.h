#ifndef __LUNAIX_VALLOC_H
#define __LUNAIX_VALLOC_H

void*
valloc(unsigned int size);

void*
vzalloc(unsigned int size);

void*
vcalloc(unsigned int size, unsigned int count);

void
vfree(void* ptr);

void*
valloc_dma(unsigned int size);

void*
vzalloc_dma(unsigned int size);

void
vfree_dma(void* ptr);

void
valloc_init();

#endif /* __LUNAIX_VALLOC_H */
