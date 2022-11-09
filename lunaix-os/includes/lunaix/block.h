#ifndef __LUNAIX_BLOCK_H
#define __LUNAIX_BLOCK_H

#include <hal/ahci/hba.h>
#include <lunaix/blkio.h>
#include <lunaix/device.h>

#define LPT_SIG 0x414e554c
#define PARTITION_NAME_SIZE 48
#define DEV_ID_SIZE 32

struct block_dev
{
    char bdev_id[DEV_ID_SIZE];
    char name[PARTITION_NAME_SIZE];
    struct blkio_context* blkio;
    struct device* dev;
    void* driver;
    u64_t end_lba;
    u32_t blk_size;
};

struct lpt_entry
{
    char part_name[PARTITION_NAME_SIZE];
    u64_t base_lba;
    u64_t end_lba;
} __attribute__((packed));

// Lunaix Partition Table
struct lpt_header
{
    u32_t signature;
    u32_t crc;
    u32_t pt_start_lba;
    u32_t pt_end_lba;
    u32_t table_len;
} __attribute__((packed));

typedef u64_t partition_t;
typedef uint32_t bdev_t;
typedef void (*devfs_exporter)(struct block_dev* bdev, void* fsnode);

void
block_init();

struct block_dev*
block_alloc_dev(const char* blk_id, void* driver, req_handler ioreq_handler);

int
block_mount(struct block_dev* bdev, devfs_exporter export);

void
blk_mapping_init();

void
blk_set_blkmapping(struct block_dev* bdev, void* fsnode);

#endif /* __LUNAIX_BLOCK_H */
