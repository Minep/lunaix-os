#ifndef __LUNAIX_SLAB_H
#define __LUNAIX_SLAB_H

#include <lunaix/ds/llist.h>

#define PILE_NAME_MAXLEN 20

struct cake_pile
{
    struct llist_header piles;
    struct llist_header full;
    struct llist_header partial;
    struct llist_header free;
    unsigned int piece_size;
    unsigned int cakes_count;
    unsigned int alloced_pieces;
    unsigned int pieces_per_cake;
    unsigned int pg_per_cake;
    char pile_name[PILE_NAME_MAXLEN];
};

typedef unsigned int piece_index_t;

#define EO_FREE_PIECE (-1)

struct cake_s
{
    struct llist_header cakes;
    void* first_piece;
    unsigned int used_pieces;
    unsigned int next_free;
    piece_index_t free_list[0];
};

/**
 * @brief 创建一个堆
 *
 * @param name 堆名称
 * @param piece_size 每个蛋糕上可以被切分的大小
 * @param pg_per_cake 每个蛋糕所占据的页数
 * @return struct cake_pile*
 */
struct cake_pile*
cake_new_pile(char* name, unsigned int piece_size, unsigned int pg_per_cake);

/**
 * @brief 拿一块儿蛋糕
 *
 * @param pile
 * @return void*
 */
void*
cake_grab(struct cake_pile* pile);

/**
 * @brief 归还一块儿蛋糕
 *
 * @param pile
 * @param area
 */
int
cake_release(struct cake_pile* pile, void* area);

void
cake_init();

/**
 * @brief 统计蛋糕数量 - 问问Pinkie :D
 *
 */
void
cake_stats();

#endif /* __LUNAIX_VALLOC_H */
