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
__block_read(struct v_file* file, void* buffer, size_t len);

int
__block_write(struct v_file* file, void* buffer, size_t len);

void
block_twifs_create()
{
    struct twifs_node* dev = twifs_toplevel_node("dev", 3);
    struct twifs_node* dev_block = twifs_dir_node(dev, "block", 5);

    if (!dev_block) {
        kprintf(KERROR "fail to create twifs node");
        return;
    }

    struct block_dev* bdev;
    struct twifs_node* bdev_node;
    for (size_t i = 0; i < MAX_DEV; i++) {
        if (!(bdev = dev_registry[i])) {
            continue;
        }

        bdev_node =
          twifs_dir_node(dev_block, bdev->bdev_id, strlen(bdev->bdev_id));
        bdev_node->fops.read = __block_read;
        bdev_node->fops.write = __block_write;
        bdev_node->data = i;
        bdev_node->inode->fsize = bdev->hd_dev->max_lba;
    }
}

int
__block_read(struct v_file* file, void* buffer, size_t len)
{
    int index = (int)((struct twifs_node*)file->inode->data)->data;
    int errno;
    struct block_dev* bdev;
    if (index < 0 || index >= MAX_DEV || !(bdev = dev_registry[index])) {
        return ENXIO;
    }
    size_t acc_size = 0, rd_size = 0, bsize = bdev->hd_dev->block_size,
           rd_block = 0;
    void* block = valloc(bsize);

    while (acc_size < len) {
        if (!bdev->hd_dev->ops.read_buffer(
              bdev->hd_dev, file->f_pos + rd_block, block, bsize)) {
            errno = ENXIO;
            goto error;
        }
        rd_size = MIN(len - acc_size, bsize);
        memcpy(buffer + acc_size, block, rd_size);
        acc_size += rd_size;
        rd_block++;
    }

    vfree(block);
    return rd_block;

error:
    vfree(block);
    return errno;
}

int
__block_write(struct v_file* file, void* buffer, size_t len)
{
    int index = (int)((struct twifs_node*)file->inode->data)->data;
    int errno;
    struct block_dev* bdev;
    if (index < 0 || index >= MAX_DEV || !(bdev = dev_registry[index])) {
        return ENXIO;
    }
    size_t acc_size = 0, wr_size = 0, bsize = bdev->hd_dev->block_size,
           wr_block = 0;
    void* block = valloc(bsize);

    while (acc_size < len) {
        wr_size = MIN(len - acc_size, bsize);
        memcpy(block, buffer + acc_size, wr_size);
        if (!bdev->hd_dev->ops.write_buffer(
              bdev->hd_dev, file->f_pos + wr_block, block, bsize)) {
            errno = ENXIO;
            break;
        }
        acc_size += wr_size;
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
    struct block_dev* dev = cake_grab(lbd_pile);
    strncpy(dev->name, hd_dev->model, PARTITION_NAME_SIZE);
    dev->hd_dev = hd_dev;
    dev->base_lba = 0;
    dev->end_lba = hd_dev->max_lba;
    if (!__block_register(dev)) {
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

// FIXME make it more general, manipulate the device through vfs mapping
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
    snprintf(dev->bdev_id, DEV_ID_SIZE, "bd%x", free_slot);
    dev_registry[free_slot++] = dev;
    return 1;
}