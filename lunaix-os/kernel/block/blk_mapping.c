#include <lunaix/block.h>
#include <lunaix/fs/twifs.h>

static struct twifs_node* blk_root;

void
blk_mapping_init()
{
    blk_root = twifs_dir_node(NULL, "block");
}

void
__blk_rd_size(struct twimap* map)
{
    struct block_dev* bdev = twimap_data(map, struct block_dev*);
    size_t secsize = bdev->hd_dev->block_size;
    twimap_printf(map, "%u", (bdev->end_lba - bdev->base_lba) * secsize);
}

void
__blk_rd_secsize(struct twimap* map)
{
    struct block_dev* bdev = twimap_data(map, struct block_dev*);
    size_t secsize = bdev->hd_dev->block_size;
    twimap_printf(map, "%u", secsize);
}

void
__blk_rd_range(struct twimap* map)
{
    struct block_dev* bdev = twimap_data(map, struct block_dev*);
    twimap_printf(
      map, "%u,%u", (uint32_t)bdev->base_lba, (uint32_t)bdev->end_lba);
}

void
__blk_rd_model(struct twimap* map)
{
    struct block_dev* bdev = twimap_data(map, struct block_dev*);
    twimap_printf(map, "%s", bdev->hd_dev->model);
}

void
__blk_rd_serial(struct twimap* map)
{
    struct block_dev* bdev = twimap_data(map, struct block_dev*);
    twimap_printf(map, "%s", bdev->hd_dev->serial_num);
}

void
__blk_rd_status(struct twimap* map)
{
    struct block_dev* bdev = twimap_data(map, struct block_dev*);
    twimap_printf(map, "%p", bdev->hd_dev->last_result.status);
}

void
__blk_rd_error(struct twimap* map)
{
    struct block_dev* bdev = twimap_data(map, struct block_dev*);
    twimap_printf(map, "%p", bdev->hd_dev->last_result.error);
}

void
__blk_rd_sense_key(struct twimap* map)
{
    struct block_dev* bdev = twimap_data(map, struct block_dev*);
    twimap_printf(map, "%p", bdev->hd_dev->last_result.sense_key);
}

void
__blk_rd_wwid(struct twimap* map)
{
    struct block_dev* bdev = twimap_data(map, struct block_dev*);
    uint32_t h = bdev->hd_dev->wwn >> 32;
    uint32_t l = (uint32_t)bdev->hd_dev->wwn;
    twimap_printf(map, "%x%x", h, l);
}

void
blk_set_blkmapping(struct block_dev* bdev)
{
    struct twifs_node* dev_root = twifs_dir_node(blk_root, bdev->bdev_id);

    struct twimap* map = twifs_mapping(dev_root, bdev, "size");
    map->read = __blk_rd_size;

    map = twifs_mapping(dev_root, bdev, "secsize");
    map->read = __blk_rd_secsize;

    map = twifs_mapping(dev_root, bdev, "range");
    map->read = __blk_rd_range;

    map = twifs_mapping(dev_root, bdev, "model");
    map->read = __blk_rd_model;

    map = twifs_mapping(dev_root, bdev, "serial");
    map->read = __blk_rd_serial;

    map = twifs_mapping(dev_root, bdev, "status");
    map->read = __blk_rd_status;

    map = twifs_mapping(dev_root, bdev, "error");
    map->read = __blk_rd_error;

    map = twifs_mapping(dev_root, bdev, "sense-key");
    map->read = __blk_rd_sense_key;

    map = twifs_mapping(dev_root, bdev, "wwid");
    map->read = __blk_rd_wwid;
}