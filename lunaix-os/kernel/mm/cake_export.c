#include <lunaix/fs/twifs.h>
#include <lunaix/mm/cake.h>

extern struct llist_header piles;

static int
__twimap_gonext_pinkiepie(struct twimap* map)
{
    struct cake_pile* pile = twimap_index(map, struct cake_pile*);
    if (pile->piles.next == &piles) {
        return 0;
    }
    map->index = list_entry(pile->piles.next, struct cake_pile, piles);
    return 1;
}

static void
__twimap_reset_pinkiepie(struct twimap* map)
{
    map->index = container_of(&piles, struct cake_pile, piles);
    twimap_printf(map, "name cakes pages size slices actives\n");
}

static void
__twimap_read_pinkiepie(struct twimap* map)
{
    struct cake_pile* pos = twimap_index(map, struct cake_pile*);
    twimap_printf(map,
                  "%s %d %d %d %d %d\n",
                  pos->pile_name,
                  pos->cakes_count,
                  pos->pg_per_cake,
                  pos->piece_size,
                  pos->pieces_per_cake,
                  pos->alloced_pieces);
}

static void
__twimap_read_piece_size(struct twimap* map)
{
    struct cake_pile* pile = twimap_data(map, struct cake_pile*);
    twimap_printf(map, "%u", pile->piece_size);
}

static void
__twimap_read_cake_count(struct twimap* map)
{
    struct cake_pile* pile = twimap_data(map, struct cake_pile*);
    twimap_printf(map, "%u", pile->cakes_count);
}

static void
__twimap_read_grabbed(struct twimap* map)
{
    struct cake_pile* pile = twimap_data(map, struct cake_pile*);
    twimap_printf(map, "%u", pile->alloced_pieces);
}

static void
__twimap_read_pieces_per_cake(struct twimap* map)
{
    struct cake_pile* pile = twimap_data(map, struct cake_pile*);
    twimap_printf(map, "%u", pile->pieces_per_cake);
}

static void
__twimap_read_page_per_cake(struct twimap* map)
{
    struct cake_pile* pile = twimap_data(map, struct cake_pile*);
    twimap_printf(map, "%u", pile->pg_per_cake);
}

void
cake_export_pile(struct twifs_node* root, struct cake_pile* pile)
{
    struct twifs_node* pile_rt;
    
    pile_rt = twifs_dir_node(root, pile->pile_name);

    twimap_export_value(pile_rt, piece_size,        FSACL_ugR, pile);
    twimap_export_value(pile_rt, cake_count,        FSACL_ugR, pile);
    twimap_export_value(pile_rt, grabbed,           FSACL_ugR, pile);
    twimap_export_value(pile_rt, pieces_per_cake,   FSACL_ugR, pile);
    twimap_export_value(pile_rt, page_per_cake,     FSACL_ugR, pile);
}

void
cake_export()
{
    struct cake_pile *pos, *n;
    struct twifs_node* cake_root;
    
    cake_root = twifs_dir_node(NULL, "cake");

    twimap_export_list(cake_root, pinkiepie, FSACL_ugR, NULL);

    llist_for_each(pos, n, &piles, piles) {
        cake_export_pile(cake_root, pos);
    }
}
EXPORT_TWIFS_PLUGIN(cake_alloc, cake_export);