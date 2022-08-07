#include <hal/ahci/hba.h>
#include <klibc/stdio.h>
#include <klibc/string.h>
#include <lib/crc.h>
#include <lunaix/block.h>
#include <lunaix/fs/twifs.h>
#include <lunaix/mm/cake.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/syslog.h>

#include <lunaix/spike.h>

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
    free_slot = 0;
}

int
__block_read(struct device* dev,
             void* buf,
             unsigned int offset,
             unsigned int len)
{
    int errno;
    struct block_dev* bdev = (struct block_dev*)dev->underlay;
    size_t acc_size = 0, rd_size = 0, bsize = bdev->hd_dev->block_size,
           rd_block = offset / bsize, r = offset % bsize;
    void* block = vzalloc(bsize);

    while (acc_size < len) {
        if (!bdev->hd_dev->ops.read_buffer(
              bdev->hd_dev, rd_block, block, bsize)) {
            errno = ENXIO;
            goto error;
        }
        rd_size = MIN(len - acc_size, bsize - r);
        memcpy(buf + acc_size, block + r, rd_size);
        acc_size += rd_size;
        r = 0;
        rd_block++;
    }

    vfree(block);
    return rd_block;

error:
    vfree(block);
    return errno;
}

int
__block_write(struct device* dev,
              void* buf,
              unsigned int offset,
              unsigned int len)
{
    int errno;
    struct block_dev* bdev = (struct block_dev*)dev->underlay;
    size_t acc_size = 0, wr_size = 0, bsize = bdev->hd_dev->block_size,
           wr_block = offset / bsize, r = offset % bsize;
    void* block = vzalloc(bsize);

    while (acc_size < len) {
        wr_size = MIN(len - acc_size, bsize - r);
        memcpy(block + r, buf + acc_size, wr_size);
        if (!bdev->hd_dev->ops.write_buffer(
              bdev->hd_dev, wr_block, block, bsize)) {
            errno = ENXIO;
            break;
        }
        acc_size += wr_size;
        r = 0;
        wr_block++;
    }

    vfree(block);
    return wr_block;

error:
    vfree(block);
    return errno;
}

int
block_mount_disk(struct hba_device* hd_dev)
{
    int errno = 0;
    struct block_dev* bdev = cake_grab(lbd_pile);
    strncpy(bdev->name, hd_dev->model, PARTITION_NAME_SIZE);
    bdev->hd_dev = hd_dev;
    bdev->base_lba = 0;
    bdev->end_lba = hd_dev->max_lba;
    if (!__block_register(bdev)) {
        errno = BLOCK_EFULL;
        goto error;
    }

    return errno;

error:
    kprintf(KERROR "Fail to mount hd: %s[%s] (%x)\n",
            hd_dev->model,
            hd_dev->serial_num,
            -errno);
}

int
__block_register(struct block_dev* bdev)
{
    if (free_slot >= MAX_DEV) {
        return 0;
    }

    struct device* dev = device_addvol(NULL, bdev, "sd%c", 'a' + free_slot);
    dev->write = __block_write;
    dev->read = __block_read;

    bdev->dev = dev;
    dev_registry[free_slot++] = bdev;
    return 1;
}