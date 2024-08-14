/**
 * @file cake.c
 * @author Lunaixsky (zelong56@gmail.com)
 * @brief A simplified cake(slab) allocator.
 *          P.s. I call it cake as slab sounds quite 'rigid' to me :)
 * @version 0.1
 * @date 2022-07-02
 *
 * @copyright Copyright (c) 2022
 *
 */

#include <klibc/string.h>
#include <lunaix/mm/cake.h>
#include <lunaix/mm/page.h>
#include <lunaix/spike.h>
#include <lunaix/syslog.h>

LOG_MODULE("CAKE")

#define CACHE_LINE_SIZE 128

static struct cake_pile master_pile, fl_pile, cakes;

struct llist_header piles = { .next = &piles, .prev = &piles };

static inline bool
embedded_pile(struct cake_pile* pile)
{
    return !(pile->options & PILE_FL_EXTERN);
}

static void*
__alloc_cake_pages(unsigned int cake_pg)
{
    struct leaflet* leaflet = alloc_leaflet(count_order(cake_pg));
    if (!leaflet) {
        return NULL;
    }
    
    return (void*)vmap(leaflet, KERNEL_DATA);
}

static struct cake_fl*
__alloc_fl(piece_t prev_id, piece_t max_len) {
    unsigned int i, j;
    struct cake_fl* cake_fl;
    
    cake_fl = cake_grab(&fl_pile);
    for (i = 0, j=prev_id; i < CAKE_FL_MAXLEN && j < max_len; i++, j++)
    {
        cake_fl->indices[i] = j + 1;
    }

    for (i -= 1; j == max_len && i < CAKE_FL_SIZE; i++) {
        cake_fl->indices[i] = EO_FREE_PIECE;
    }

    cake_fl->next = NULL;
    return cake_fl;
}

static void
__free_fls(struct cake_s* cake) {
    unsigned int i, j;
    struct cake_fl *cake_fl, *next;
    
    cake_fl = cake->fl;
    while (cake_fl)
    {
        next = cake_fl->next;
        cake_release(&fl_pile, cake_fl);
        cake_fl = next;
    }

    cake->fl = NULL;
}

static piece_t*
__fl_slot(struct cake_s* cake, piece_t index) 
{
    int id_acc;
    struct cake_fl* cake_fl;
    struct cake_pile* pile;

    pile = cake->owner;
    if (embedded_pile(pile)) {
        return &cake->free_list[index];
    }

    id_acc = 0;
    cake_fl = cake->fl;
    while (index >= CAKE_FL_MAXLEN)
    {
        index  -= CAKE_FL_MAXLEN;
        id_acc += CAKE_FL_MAXLEN;

        if (cake_fl->next) {
            cake_fl = cake_fl->next;
            continue;
        }
        
        cake_fl = __alloc_fl(id_acc, pile->pieces_per_cake);
        cake_fl->next = cake_fl;
    }

    return &cake_fl->indices[index];
}

static inline struct cake_s*
__create_cake_extern(struct cake_pile* pile)
{
    struct cake_s* cake;
    
    cake = cake_grab(&cakes);
    if (unlikely(!cake)) {
        return NULL;
    }

    cake->first_piece = __alloc_cake_pages(pile->pg_per_cake);
    cake->fl = __alloc_fl(0, pile->pieces_per_cake);
    cake->next_free = 0;

    return cake;
}

static inline struct cake_s*
__create_cake_embed(struct cake_pile* pile)
{
    struct cake_s* cake;
    
    cake = __alloc_cake_pages(pile->pg_per_cake);
    if (unlikely(!cake)) {
        return NULL;
    }

    u32_t max_piece = pile->pieces_per_cake;

    assert(max_piece);
    
    cake->first_piece = (void*)((ptr_t)cake + pile->offset);
    cake->next_free = 0;

    piece_t* free_list = cake->free_list;
    for (size_t i = 0; i < max_piece - 1; i++) {
        free_list[i] = i + 1;
    }
    free_list[max_piece - 1] = EO_FREE_PIECE;

    return cake;
}

static struct cake_s*
__new_cake(struct cake_pile* pile)
{
    struct cake_s* cake;

    if (embedded_pile(pile)) {
        cake = __create_cake_embed(pile);
    }
    else {
        cake = __create_cake_extern(pile);
    }
    
    cake->owner = pile;
    pile->cakes_count++;
    llist_append(&pile->free, &cake->cakes);

    return cake;
}

static void
__init_pile(struct cake_pile* pile,
            char* name,
            unsigned int piece_size,
            unsigned int pg_per_cake,
            int options)
{
    if (!pile) {
        return;
    }

    unsigned int offset = sizeof(long);

    // 默认每块儿蛋糕对齐到地址总线宽度
    if ((options & PILE_ALIGN_CACHE)) {
        // 对齐到128字节缓存行大小，主要用于DMA
        offset = CACHE_LINE_SIZE;
    }

    piece_size = ROUNDUP(piece_size, offset);
    *pile = (struct cake_pile){ .piece_size = piece_size,
                                .cakes_count = 0,
                                .options = options,
                                .pg_per_cake = pg_per_cake };

    if (!embedded_pile(pile)) {
        pile->offset = 0;
        pile->pieces_per_cake = (pg_per_cake * PAGE_SIZE) / piece_size;
    }
    else {
        unsigned int free_list_size;

        pile->pieces_per_cake 
            = (pg_per_cake * PAGE_SIZE) /
              (piece_size + sizeof(piece_t));
        
        free_list_size 
            = pile->pieces_per_cake * sizeof(piece_t);

        pile->offset 
            = ROUNDUP(sizeof(struct cake_s) + free_list_size, offset);
        pile->pieces_per_cake 
            -= ICEIL((pile->offset - free_list_size), piece_size);
    }

    strncpy(pile->pile_name, name, PILE_NAME_MAXLEN);

    llist_init_head(&pile->free);
    llist_init_head(&pile->full);
    llist_init_head(&pile->partial);
    llist_append(&piles, &pile->piles);
}

static void
__destory_cake(struct cake_s* cake)
{
    // Pinkie Pie is going to MAD!

    struct leaflet* leaflet;
    struct cake_pile* owner;
    pfn_t _pfn;

    _pfn = pfn(vmm_v2p(cake->first_piece));
    leaflet = ppfn_leaflet(_pfn);
    owner = cake->owner;

    assert(!cake->used_pieces);

    llist_delete(&cake->cakes);
    owner->cakes_count--;

    
    if (!embedded_pile(cake)) {
        __free_fls(cake);
        cake_release(&cakes, cake); 
        vunmap(cake->first_piece, leaflet);
    }
    else {
        vunmap(__ptr(cake), leaflet);
    }

done:
    leaflet_return(leaflet);
}

void
cake_init()
{
    // pinkamina is our master, no one shall precede her.
    __init_pile(&master_pile, "pinkamina", 
                sizeof(master_pile), 1, 0);

    __init_pile(&fl_pile, "gummy", \
                sizeof(struct cake_fl), 1, 0);
    __init_pile(&cakes, "cakes", \
                sizeof(struct cake_s), 1, 0);
}

struct cake_pile*
cake_new_pile(char* name,
              unsigned int piece_size,
              unsigned int pg_per_cake,
              int options)
{
    struct cake_pile* pile = (struct cake_pile*)cake_grab(&master_pile);

    // must aligned to napot order!
    assert(is_pot(pg_per_cake));

    __init_pile(pile, name, piece_size, pg_per_cake, options);

    return pile;
}

void
cake_set_constructor(struct cake_pile* pile, pile_cb ctor)
{
    pile->ctor = ctor;
}

void*
cake_grab(struct cake_pile* pile)
{
    struct cake_s *pos, *n;
    if (!llist_empty(&pile->partial)) {
        pos = list_entry(pile->partial.next, typeof(*pos), cakes);
    } else if (llist_empty(&pile->free)) {
        pos = __new_cake(pile);
    } else {
        pos = list_entry(pile->free.next, typeof(*pos), cakes);
    }

    if (!pos)
        return NULL;
    
    void* piece;
    piece_t found_index, *fl_slot;
    
    found_index = pos->next_free;
    fl_slot = __fl_slot(pos, found_index);

    pos->next_free = *fl_slot;
    pos->used_pieces++;
    pile->alloced_pieces++;

    llist_delete(&pos->cakes);

    fl_slot = __fl_slot(pos, pos->next_free);
    if (*fl_slot == EO_FREE_PIECE) {
        llist_append(&pile->full, &pos->cakes);
    } else {
        llist_append(&pile->partial, &pos->cakes);
    }

    piece =
      (void*)(__ptr(pos->first_piece) + found_index * pile->piece_size);

    if (pile->ctor) {
        pile->ctor(pile, piece);
    }

    return piece;
}

int
cake_release(struct cake_pile* pile, void* area)
{
    piece_t piece_index;
    size_t dsize = 0;
    struct cake_s *pos, *n;
    struct llist_header* hdrs[2] = { &pile->full, &pile->partial };

    for (size_t i = 0; i < 2; i++) {
        llist_for_each(pos, n, hdrs[i], cakes)
        {
            if (pos->first_piece > area) {
                continue;
            }
            dsize = (ptr_t)(area - pos->first_piece);
            piece_index = dsize / pile->piece_size;
            if (piece_index < pile->pieces_per_cake) {
                goto found;
            }
        }
    }

    return 0;

found:
    assert(!(dsize % pile->piece_size));

    piece_t *fl_slot;

    fl_slot = __fl_slot(pos, piece_index);
    *fl_slot = pos->next_free;
    pos->next_free = piece_index;

    assert_msg(*fl_slot != pos->next_free, "double free");

    pos->used_pieces--;
    pile->alloced_pieces--;

    llist_delete(&pos->cakes);
    if (!pos->used_pieces) {
        llist_append(&pile->free, &pos->cakes);
    } else {
        llist_append(&pile->partial, &pos->cakes);
    }

    *((unsigned int*)area) = DEADCAKE_MARK;

    return 1;
}

void
cake_ctor_zeroing(struct cake_pile* pile, void* piece)
{
    memset(piece, 0, pile->piece_size);
}

static inline void
__reclaim(struct cake_pile *pile)
{
    struct cake_s *pos, *n;
    llist_for_each(pos, n, &pile->free, cakes)
    {
        __destory_cake(pos);
    }
}

void
cake_reclaim_freed()
{
    struct cake_pile *pos, *n;
    llist_for_each(pos, n, &master_pile.piles, piles)
    {
        __reclaim(pos);
    }
}