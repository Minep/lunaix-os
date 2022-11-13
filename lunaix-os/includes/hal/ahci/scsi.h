#ifndef __LUNAIX_SCSI_H
#define __LUNAIX_SCSI_H

#include <stdint.h>

#define SCSI_FLIP(val)                                                         \
    ((((val)&0x000000ff) << 24) | (((val)&0x0000ff00) << 8) |                  \
     (((val)&0x00ff0000) >> 8) | (((val)&0xff000000) >> 24))

#define SCSI_READ_CAPACITY_16 0x9e
#define SCSI_READ_CAPACITY_10 0x25
#define SCSI_READ_BLOCKS_16 0x88
#define SCSI_READ_BLOCKS_12 0xa8
#define SCSI_WRITE_BLOCKS_16 0x8a
#define SCSI_WRITE_BLOCKS_12 0xaa

#define SCSI_CDB16 16
#define SCSI_CDB12 12

struct scsi_cdb12
{
    uint8_t opcode;
    uint8_t misc1;
    u32_t lba_be;
    u32_t length;
    uint8_t misc2;
    uint8_t ctrl;
} __attribute__((packed));

struct scsi_cdb16
{
    uint8_t opcode;
    uint8_t misc1;
    u32_t lba_be_hi;
    u32_t lba_be_lo;
    u32_t length;
    uint8_t misc2;
    uint8_t ctrl;
} __attribute__((packed));

void
scsi_create_packet12(struct scsi_cdb12* cdb,
                     uint8_t opcode,
                     u32_t lba,
                     u32_t alloc_size);

void
scsi_create_packet16(struct scsi_cdb16* cdb,
                     uint8_t opcode,
                     uint64_t lba,
                     u32_t alloc_size);

void
scsi_submit(struct hba_device* dev, struct blkio_req* io_req);

void
scsi_parse_capacity(struct hba_device* device, u32_t* parameter);

#endif /* __LUNAIX_ATAPI_H */
