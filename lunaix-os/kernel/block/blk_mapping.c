#include <lunaix/block.h>
#include <lunaix/fs/twifs.h>

static struct twifs_node* blk_root;

void
blk_mapping_init()
{
    blk_root = twifs_dir_node(NULL, "block");
}

void
__blk_rd_lblksz(struct twimap* map)
{
    struct block_dev* bdev = twimap_data(map, struct block_dev*);
    size_t lblksz = bdev->blk_size;
    twimap_printf(map, "%u", lblksz);
}

void
__blk_rd_name(struct twimap* map)
{
    struct block_dev* bdev = twimap_data(map, struct block_dev*);
    twimap_printf(map, "%s", bdev->name);
}

void
__blk_rd_start_lba(struct twimap* map)
{
    struct block_dev* bdev = twimap_data(map, struct block_dev*);
    twimap_printf(map, "%d", bdev->start_lba);
}

void
__blk_rd_end_lba(struct twimap* map)
{
    struct block_dev* bdev = twimap_data(map, struct block_dev*);
    twimap_printf(map, "%d", bdev->end_lba);
}

void
__blk_rd_size(struct twimap* map)
{
    struct block_dev* bdev = twimap_data(map, struct block_dev*);
    twimap_printf(
      map, "%u", (u32_t)(bdev->end_lba - bdev->start_lba) * bdev->blk_size);
}

void
__map_internal(struct block_dev* bdev, void* fsnode)
{
    struct twifs_node* dev_root = (struct twifs_node*)fsnode;

    struct twimap* map = twifs_mapping(dev_root, bdev, "size");
    map->read = __blk_rd_size;

    map = twifs_mapping(dev_root, bdev, "lblk_size");
    map->read = __blk_rd_lblksz;

    map = twifs_mapping(dev_root, bdev, "name");
    map->read = __blk_rd_name;

    map = twifs_mapping(dev_root, bdev, "begin");
    map->read = __blk_rd_start_lba;

    map = twifs_mapping(dev_root, bdev, "end");
    map->read = __blk_rd_end_lba;
}

void
blk_set_blkmapping(struct block_dev* bdev, void* fsnode)
{
    struct twifs_node* dev_root = (struct twifs_node*)fsnode;

    __map_internal(bdev, dev_root);

    struct block_dev *pos, *n;
    llist_for_each(pos, n, &bdev->parts, parts)
    {
        struct twifs_node* part_node = twifs_dir_node(dev_root, pos->bdev_id);
        __map_internal(pos, part_node);
    }
}