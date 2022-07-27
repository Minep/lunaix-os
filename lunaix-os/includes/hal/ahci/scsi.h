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

struct scsi_cdb12
{
    uint8_t opcode;
    uint8_t misc1;
    uint32_t lba_be;
    uint32_t length;
    uint8_t misc2;
    uint8_t ctrl;
} __attribute__((packed));

struct scsi_cdb16
{
    uint8_t opcode;
    uint8_t misc1;
    uint32_t lba_be_hi;
    uint32_t lba_be_lo;
    uint32_t length;
    uint8_t misc2;
    uint8_t ctrl;
} __attribute__((packed));

void
scsi_create_packet12(struct scsi_cdb12* cdb,
                     uint8_t opcode,
                     uint32_t lba,
                     uint32_t alloc_size);

void
scsi_create_packet16(struct scsi_cdb16* cdb,
                     uint8_t opcode,
                     uint64_t lba,
                     uint32_t alloc_size);

void
scsi_read_buffer(struct hba_device* dev,
                 uint64_t lba,
                 void* buffer,
                 uint32_t size);

void
scsi_write_buffer(struct hba_device* dev,
                  uint64_t lba,
                  void* buffer,
                  uint32_t size);

#endif /* __LUNAIX_ATAPI_H */
