#ifndef __LUNAIX_PGPOL_H
#define __LUNAIX_PGPOL_H

/*
 * Page Policy - Allocation Type
 *
 * Controlling the kind of page to allocate.
 *
 * __NONE:      (internal use) Used to mark a free page
 * NORMAL:      A normal, only kernel accessible page
 * NORMAL_USER: A normal, only user acccessible page
 * PCACHE:      Page used by the page cache (pcache) system
 * DMA:         A DMA Page 
 *
 */

#define __PGPOL_NONE            0
#define PGPOL_NORMAL            1
#define PGPOL_NORMAL_USER       2
#define PGPOL_PCACHE            3
#define PGPOL_DMA               4
#define PGPOL_KIND_MASK         0xf

/**
 * Marked a reserved page
 */
#define __PGPOL_RESERVED        (-1U)

/*
 * Page Policy - Allocation Attribute
 *
 * Controlling the attribute of page allocated
 *
 * ZERO:        Page should be zeroed before use
 * NOFAIL:      Ensure the allocation is successfuly, panic 
 *               if failure after all attempts.
 * NORETRY:     Do not retry if an allocation attempt is 
 *               unsuccessful.
 * NNAPOT:      Allow unaligned page. For page with order <= 10,
 *               this flag allow such page not to aligned to 
 *               its order. For order > 10, the alignment is 
 *               mandantory.
 * PIN:         Pinned page, not movable. (Future)
 *
 */

#define PGPOL_ZERO        (1 << 4)
#define PGPOL_NOFAIL      (1 << 5)
#define PGPOL_NORETRY     (1 << 6)
#define PGPOL_NNAPOT      (1 << 7)
#define PGPOL_PIN         (1 << 8)


#define PGPOL_PGTABLE     \
        ( PGPOL_NORMAL | PGPOL_PIN | PGPOL_ZERO | PGPOL_NOFAIL)


typedef unsigned int pgpol_t;


#endif
