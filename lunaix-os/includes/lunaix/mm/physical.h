#ifndef __LUNAIX_PHYSICAL_H
#define __LUNAIX_PHYSICAL_H

#include <lunaix/compiler.h>
#include <sys/mm/physical.h>

/**
 * @brief 长久页：不会被缓存，但允许释放
 *
 */
#define PP_FGPERSIST    0b0001

/**
 * @brief 锁定页：不会被缓存，默认不可释放
 *
 */
#define PP_FGLOCKED     0b0011

/**
 * @brief 预留页：不会被缓存，永远不可释放
 *
 */
#define PP_RESERVED     0b1000

struct ppage_arch;

struct ppage
{
    unsigned int refs;
    union {
        struct {
            unsigned short flags:8;
            unsigned short pool:4;
            unsigned short type:4;
        };
        unsigned short attr;
    };
    unsigned short companion;
    
    struct llist_header sibs;

    struct ppage_arch arch;
} cacheline_align;


#endif /* __LUNAIX_PHYSICAL_H */
