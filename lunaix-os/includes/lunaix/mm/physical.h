#ifndef __LUNAIX_PHYSICAL_H
#define __LUNAIX_PHYSICAL_H

#include <lunaix/compiler.h>
#include <asm/physical.h>

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
            union {
                struct {
                    unsigned char flags:2;
                    unsigned char order:6;
                };
                unsigned char top_flags;
            };
            struct {
                unsigned char pool:4;
                unsigned char type:4;
            };
        };
        unsigned short attr;
    };
    unsigned short companion;
    
    struct llist_header sibs;

    struct ppage_arch arch;
} align(16);


#endif /* __LUNAIX_PHYSICAL_H */
