#include <hal/ahci/ahci.h>
#include <lunaix/block.h>
#include <lunaix/fs/twifs.h>

void
__twimap_read_serial(struct twimap* map)
{
    struct hba_device* hbadev = twimap_data(map, struct hba_device*);
    twimap_printf(map, "%s", hbadev->serial_num);
}

void
__twimap_read_last_status(struct twimap* map)
{
    struct hba_device* hbadev = twimap_data(map, struct hba_device*);
    twimap_printf(map,
                  "%p\t%p\t%p",
                  hbadev->last_result.status,
                  hbadev->last_result.error,
                  hbadev->last_result.sense_key);
}

void
__twimap_read_capabilities(struct twimap* map)
{
    struct hba_device* hbadev = twimap_data(map, struct hba_device*);
    twimap_printf(map, "%p", hbadev->capabilities);
}

void
__twimap_read_alignment(struct twimap* map)
{
    struct hba_device* hbadev = twimap_data(map, struct hba_device*);
    twimap_printf(map, "%d", hbadev->alignment_offset);
}

void
__twimap_read_wwid(struct twimap* map)
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

    twimap_export_value(dev_root, serial,       FSACL_aR, bdev->driver);
    twimap_export_value(dev_root, last_status,  FSACL_aR, bdev->driver);
    twimap_export_value(dev_root, wwid,         FSACL_aR, bdev->driver);
    twimap_export_value(dev_root, capabilities, FSACL_aR, bdev->driver);
    twimap_export_value(dev_root, alignment,    FSACL_aR, bdev->driver);
}