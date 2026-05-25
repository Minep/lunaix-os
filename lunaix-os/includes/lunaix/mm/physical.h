#ifndef __LUNAIX_PHYSICAL_H
#define __LUNAIX_PHYSICAL_H

#include <lunaix/compiler.h>
#include <asm/physical.h>

struct ppage_arch;

struct ppage
{
    union {
        struct {
            unsigned char pool:4;
            unsigned char order:4;
        };
        unsigned char attr;
    };
    unsigned char companion;
    unsigned int pol;
    unsigned int refs;
    
    struct llist_header sibs;

    struct ppage_arch arch;
} align(16);


#endif /* __LUNAIX_PHYSICAL_H */
