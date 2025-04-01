#include <lunaix/block.h>
#include <lunaix/fs/twifs.h>

static struct twifs_node* blk_root;

void
blk_mapping_init()
{
    blk_root = twifs_dir_node(NULL, "block");
}

static void
__twimap_read_lblk_size(struct twimap* map)
{
    struct block_dev* bdev = twimap_data(map, struct block_dev*);
    size_t lblksz = bdev->blk_size;
    twimap_printf(map, "%u", lblksz);
}

static void
__twimap_read_name(struct twimap* map)
{
    struct block_dev* bdev = twimap_data(map, struct block_dev*);
    twimap_printf(map, "%s", bdev->name);
}

static void
__twimap_read_lba_begin(struct twimap* map)
{
    struct block_dev* bdev = twimap_data(map, struct block_dev*);
    twimap_printf(map, "%d", bdev->start_lba);
}

static void
__twimap_read_lba_end(struct twimap* map)
{
    struct block_dev* bdev = twimap_data(map, struct block_dev*);
    twimap_printf(map, "%d", bdev->end_lba);
}

static void
__twimap_read_size(struct twimap* map)
{
    struct block_dev* bdev = twimap_data(map, struct block_dev*);
    twimap_printf(
      map, "%u", (u32_t)(bdev->end_lba - bdev->start_lba) * bdev->blk_size);
}

void
__map_internal(struct block_dev* bdev, void* fsnode)
{
    struct twifs_node* dev_root;
    
    dev_root = (struct twifs_node*)fsnode;
    
    twimap_export_value(dev_root, size,      FSACL_aR, bdev);
    twimap_export_value(dev_root, lblk_size, FSACL_aR, bdev);
    twimap_export_value(dev_root, name,      FSACL_aR, bdev);
    twimap_export_value(dev_root, lba_begin, FSACL_aR, bdev);
    twimap_export_value(dev_root, lba_end,   FSACL_aR, bdev);
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