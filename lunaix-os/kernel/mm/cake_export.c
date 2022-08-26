#include <lunaix/fs/twifs.h>
#include <lunaix/mm/cake.h>

extern struct llist_header piles;

int
__cake_stat_gonext(struct twimap* map)
{
    struct cake_pile* pile = twimap_index(map, struct cake_pile*);
    if (pile->piles.next == &piles) {
        return 0;
    }
    map->index = list_entry(pile->piles.next, struct cake_pile, piles);
    return 1;
}

void
__cake_stat_reset(struct twimap* map)
{
    map->index = container_of(piles.next, struct cake_pile, piles);
}

void
__cake_rd_stat(struct twimap* map)
{
    struct cake_pile* pos = twimap_index(map, struct cake_pile*);
    twimap_printf(map,
                  "%s %d %d %d %d\n",
                  pos->pile_name,
                  pos->cakes_count,
                  pos->pg_per_cake,
                  pos->pieces_per_cake,
                  pos->alloced_pieces);
}

void
__cake_rd_psize(struct twimap* map)
{
    struct cake_pile* pile = twimap_data(map, struct cake_pile*);
    twimap_printf(map, "%u", pile->piece_size);
}

void
__cake_rd_ccount(struct twimap* map)
{
    struct cake_pile* pile = twimap_data(map, struct cake_pile*);
    twimap_printf(map, "%u", pile->cakes_count);
}

void
__cake_rd_alloced(struct twimap* map)
{
    struct cake_pile* pile = twimap_data(map, struct cake_pile*);
    twimap_printf(map, "%u", pile->alloced_pieces);
}

void
__cake_rd_ppc(struct twimap* map)
{
    struct cake_pile* pile = twimap_data(map, struct cake_pile*);
    twimap_printf(map, "%u", pile->pieces_per_cake);
}

void
__cake_rd_ppg(struct twimap* map)
{
    struct cake_pile* pile = twimap_data(map, struct cake_pile*);
    twimap_printf(map, "%u", pile->pg_per_cake);
}

void
cake_export_pile(struct twifs_node* root, struct cake_pile* pile)
{
    struct twifs_node* pile_rt = twifs_dir_node(root, pile->pile_name);

    struct twimap* map = twifs_mapping(pile_rt, pile, "piece_size");
    map->read = __cake_rd_psize;

    map = twifs_mapping(pile_rt, pile, "cake_count");
    map->read = __cake_rd_ccount;

    map = twifs_mapping(pile_rt, pile, "grabbed");
    map->read = __cake_rd_alloced;

    map = twifs_mapping(pile_rt, pile, "pieces_per_cake");
    map->read = __cake_rd_ppc;

    map = twifs_mapping(pile_rt, pile, "page_per_cake");
    map->read = __cake_rd_ppg;
}

void
cake_export()
{
    struct twifs_node* cake_root = twifs_dir_node(NULL, "cake");

    struct twimap* map = twifs_mapping(cake_root, NULL, "pinkiepie");
    map->reset = __cake_stat_reset;
    map->go_next = __cake_stat_gonext;
    map->read = __cake_rd_stat;
    __cake_stat_reset(map);

    struct cake_pile *pos, *n;
    llist_for_each(pos, n, &piles, piles)
    {
        cake_export_pile(cake_root, pos);
    }
}