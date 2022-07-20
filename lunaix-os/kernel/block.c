#include <hal/ahci/hba.h>
#include <klibc/string.h>
#include <lib/crc.h>
#include <lunaix/block.h>
#include <lunaix/mm/cake.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/syslog.h>

#define BLOCK_EREAD 1
#define BLOCK_ESIG 2
#define BLOCK_ECRC 3
#define BLOCK_EFULL 3

LOG_MODULE("BLOCK")

#define MAX_DEV 32

struct cake_pile* lbd_pile;
struct block_dev** dev_registry;

int free_slot = 0;

int
__block_mount_partitions(struct hba_device* hd_dev);

int
__block_register(struct block_dev* dev);

void
block_init()
{
    lbd_pile = cake_new_pile("block_dev", sizeof(struct block_dev), 1, 0);
    dev_registry = vcalloc(sizeof(struct block_dev*), MAX_DEV);
}

int
block_mount_disk(struct hba_device* hd_dev)
{
    int errno = 0;
    struct block_dev* dev = cake_grab(lbd_pile);
    strncpy(dev->name, hd_dev->model, PARTITION_NAME_SIZE);
    dev->hd_dev = hd_dev;
    dev->base_lba = 0;
    dev->end_lba = hd_dev->max_lba;
    if (!__block_register(dev)) {
        errno = BLOCK_EFULL;
        goto error;
    }

    if ((errno = __block_mount_partitions(hd_dev))) {
        goto error;
    }

error:
    kprintf(KERROR "Fail to mount hd: %s[%s] (%x)",
            hd_dev->model,
            hd_dev->serial_num,
            -errno);
}

int
__block_mount_partitions(struct hba_device* hd_dev)
{
    int errno = 0;
    void* buffer = valloc_dma(hd_dev->block_size);
    if (!hd_dev->ops.read_buffer(hd_dev, 1, buffer, hd_dev->block_size)) {
        errno = BLOCK_EREAD;
        goto done;
    }

    struct lpt_header* header = (struct lpt_header*)buffer;
    if (header->signature != LPT_SIG) {
        errno = BLOCK_ESIG;
        goto done;
    }

    if (header->crc != crc32b(buffer, sizeof(*header))) {
        errno = BLOCK_ECRC;
        goto done;
    }

    uint64_t lba = header->pt_start_lba;
    int j = 0;
    int count_per_sector = hd_dev->block_size / sizeof(struct lpt_entry);
    while (lba < header->pt_end_lba) {
        if (!hd_dev->ops.read_buffer(hd_dev, lba, buffer, hd_dev->block_size)) {
            errno = BLOCK_EREAD;
            goto done;
        }
        struct lpt_entry* entry = (struct lpt_entry*)buffer;
        for (int i = 0; i < count_per_sector; i++, j++) {
            struct block_dev* dev = cake_grab(lbd_pile);
            strncpy(dev->name, entry->part_name, PARTITION_NAME_SIZE);
            dev->hd_dev = hd_dev;
            dev->base_lba = entry->base_lba;
            dev->end_lba = entry->end_lba;
            __block_register(dev);

            if (j >= header->table_len) {
                goto done;
            }
        }
    }

done:
    vfree_dma(buffer);
    return -errno;
}

int
__block_register(struct block_dev* dev)
{
    if (free_slot >= MAX_DEV) {
        return 0;
    }

    dev_registry[free_slot++] = dev;
    return 1;
}