#ifndef __LUNAIX_BLOCK_H
#define __LUNAIX_BLOCK_H

#include <hal/ahci/hba.h>
#include <lunaix/device.h>

#define LPT_SIG 0x414e554c
#define PARTITION_NAME_SIZE 48
#define DEV_ID_SIZE 32

typedef uint64_t partition_t;
typedef uint32_t bdev_t;

struct block_dev
{
    char bdev_id[DEV_ID_SIZE];
    char name[PARTITION_NAME_SIZE];
    struct hba_device* hd_dev;
    struct device* dev;
    uint64_t base_lba;
    uint64_t end_lba;
};

struct lpt_entry
{
    char part_name[PARTITION_NAME_SIZE];
    uint64_t base_lba;
    uint64_t end_lba;
} __attribute__((packed));

// Lunaix Partition Table
struct lpt_header
{
    uint32_t signature;
    uint32_t crc;
    uint32_t pt_start_lba;
    uint32_t pt_end_lba;
    uint32_t table_len;
} __attribute__((packed));

void
block_init();

int
block_mount_disk(struct hba_device* hd_dev);

void
blk_mapping_init();

void
blk_set_blkmapping(struct block_dev* bdev);

#endif /* __LUNAIX_BLOCK_H */
