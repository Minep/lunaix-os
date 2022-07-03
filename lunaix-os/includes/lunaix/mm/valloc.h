#ifndef __LUNAIX_VALLOC_H
#define __LUNAIX_VALLOC_H

void*
valloc(unsigned int size);

void
vfree(void* ptr);

void
valloc_init();

#endif /* __LUNAIX_VALLOC_H */
