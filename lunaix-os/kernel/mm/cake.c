/**
 * @file valloc.c
 * @author Lunaixsky (zelong56@gmail.com)
 * @brief A simplified cake(slab) allocator.
 *          P.s. I call it cake as slab sounds more 'ridge' to me. :)
 * @version 0.1
 * @date 2022-07-02
 *
 * @copyright Copyright (c) 2022
 *
 */

#include <klibc/string.h>
#include <lunaix/mm/cake.h>
#include <lunaix/mm/pmm.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/spike.h>
#include <lunaix/syslog.h>

LOG_MODULE("CAKE")

#define SLAB_SIZE PG_SIZE

struct cake_pile master_pile;

struct llist_header piles = { .next = &piles, .prev = &piles };

void*
__alloc_cake(unsigned int cake_pg)
{
    uintptr_t pa = pmm_alloc_cpage(KERNEL_PID, cake_pg, 0);
    return vmm_vmap(pa, cake_pg * PG_SIZE, PG_PREM_RW);
}

struct cake_s*
__new_cake(struct cake_pile* pile)
{
    struct cake_s* cake = __alloc_cake(pile->pg_per_cake);

    int max_piece = pile->pieces_per_cake;

    cake->first_piece =
      (void*)((uintptr_t)(cake + 1) + max_piece * sizeof(piece_index_t));
    cake->next_free = 0;

    piece_index_t* free_list = &cake->free_list;
    for (size_t i = 0; i < max_piece - 1; i++) {
        free_list[i] = i + 1;
    }
    free_list[max_piece - 1] = EO_FREE_PIECE;

    llist_append(&pile->free, &cake->cakes);

    return cake;
}

void
__init_pile(struct cake_pile* pile,
            char* name,
            unsigned int piece_size,
            unsigned int pg_per_cake)
{
    *pile = (struct cake_pile){ .piece_size = piece_size,
                                .cakes_count = 1,
                                .pieces_per_cake =
                                  (pg_per_cake * PG_SIZE) /
                                  (piece_size + sizeof(piece_index_t)),
                                .pg_per_cake = pg_per_cake };

    strncpy(&pile->pile_name, name, PILE_NAME_MAXLEN);

    llist_init_head(&pile->free);
    llist_init_head(&pile->full);
    llist_init_head(&pile->partial);
    llist_append(&piles, &pile->piles);
}

void
cake_init()
{
    __init_pile(&master_pile, "master", sizeof(master_pile), 1);
}

struct cake_pile*
cake_new_pile(char* name, unsigned int piece_size, unsigned int pg_per_cake)
{
    struct cake_pile* pile = (struct cake_pile*)cake_grab(&master_pile);

    __init_pile(pile, name, piece_size, pg_per_cake);

    return pile;
}

void*
cake_grab(struct cake_pile* pile)
{
    struct cake_s *pos, *n;
    llist_for_each(pos, n, &pile->partial, cakes)
    {
        if (pos->next_free != EO_FREE_PIECE) {
            goto found;
        }
    }

    if (llist_empty(&pile->free)) {
        pos = __new_cake(pile);
    } else {
        pos = list_entry(pile->free.next, typeof(*pos), cakes);
    }

found:
    piece_index_t found_index = pos->next_free;
    pos->next_free = pos->free_list[found_index];
    pos->used_pieces++;
    pile->alloced_pieces++;

    llist_delete(&pos->cakes);
    if (pos->next_free == EO_FREE_PIECE) {
        llist_append(&pile->full, &pos->cakes);
    } else {
        llist_append(&pile->partial, &pos->cakes);
    }

    return (void*)((uintptr_t)pos->first_piece +
                   found_index * pile->piece_size);
}

int
cake_release(struct cake_pile* pile, void* area)
{
    piece_index_t area_index;
    struct cake_s *pos, *n;
    struct llist_header* hdrs[2] = { &pile->full, &pile->partial };

    for (size_t i = 0; i < 2; i++) {
        llist_for_each(pos, n, hdrs[i], cakes)
        {
            if (pos->first_piece > area) {
                continue;
            }
            area_index =
              (uintptr_t)(area - pos->first_piece) / pile->piece_size;
            if (area_index < pile->pieces_per_cake) {
                goto found;
            }
        }
    }

    return 0;

found:
    pos->free_list[area_index] = pos->next_free;
    pos->next_free = area_index;
    pos->used_pieces--;
    pile->alloced_pieces--;

    llist_delete(pos);
    if (!pos->used_pieces) {
        llist_append(&pile->free, &pos->cakes);
    } else {
        llist_append(&pile->partial, &pos->cakes);
    }

    return 1;
}

void
cake_stats()
{
    kprintf(KDEBUG "<name> <cake> <pg/c> <p/c> <alloced>\n");

    struct cake_pile *pos, *n;
    llist_for_each(pos, n, &piles, piles)
    {
        kprintf("%s %d %d %d %d\n",
                pos->pile_name,
                pos->cakes_count,
                pos->pg_per_cake,
                pos->pieces_per_cake,
                pos->alloced_pieces);
    }
}