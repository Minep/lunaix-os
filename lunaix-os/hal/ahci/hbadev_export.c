#include <hal/ahci/ahci.h>
#include <lunaix/block.h>
#include <lunaix/fs/twifs.h>

void
__blk_rd_serial(struct twimap* map)
{
    struct hba_device* hbadev = twimap_data(map, struct hba_device*);
    twimap_printf(map, "%s", hbadev->serial_num);
}

void
__blk_rd_last_status(struct twimap* map)
{
    struct hba_device* hbadev = twimap_data(map, struct hba_device*);
    twimap_printf(map,
                  "%p\t%p\t%p",
                  hbadev->last_result.status,
                  hbadev->last_result.error,
                  hbadev->last_result.sense_key);
}

void
__blk_rd_capabilities(struct twimap* map)
{
    struct hba_device* hbadev = twimap_data(map, struct hba_device*);
    twimap_printf(map, "%p", hbadev->capabilities);
}

void
__blk_rd_aoffset(struct twimap* map)
{
    struct hba_device* hbadev = twimap_data(map, struct hba_device*);
    twimap_printf(map, "%d", hbadev->alignment_offset);
}

void
__blk_rd_wwid(struct twimap* map)
{
    struct hba_device* hbadev = twimap_data(map, struct hba_device*);
    u32_t h = hbadev->wwn >> 32;
    u32_t l = (u32_t)hbadev->wwn;
    if ((h | l)) {
        twimap_printf(map, "wwn:%x%x", h, l);
    } else {
        twimap_printf(map, "0");
    }
}

void
ahci_fsexport(struct block_dev* bdev, void* fs_node)
{
    struct twifs_node* dev_root = (struct twifs_node*)fs_node;
    struct twimap* map;

    map = twifs_mapping(dev_root, bdev->driver, "serial");
    map->read = __blk_rd_serial;

    map = twifs_mapping(dev_root, bdev->driver, "last_status");
    map->read = __blk_rd_last_status;

    map = twifs_mapping(dev_root, bdev->driver, "wwid");
    map->read = __blk_rd_wwid;

    map = twifs_mapping(dev_root, bdev->driver, "capabilities");
    map->read = __blk_rd_capabilities;

    map = twifs_mapping(dev_root, bdev->driver, "alignment_offset");
    map->read = __blk_rd_aoffset;
}