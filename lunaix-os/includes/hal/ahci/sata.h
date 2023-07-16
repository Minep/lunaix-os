#ifndef __LUNAIX_SATA_H
#define __LUNAIX_SATA_H

#include <hal/ahci/hba.h>

#define SATA_REG_FIS_D2H 0x34
#define SATA_REG_FIS_H2D 0x27
#define SATA_REG_FIS_COMMAND 0x80
#define SATA_LBA_COMPONENT(lba, offset) ((((lba) >> (offset)) & 0xff))

#define ATA_IDENTIFY_DEVICE 0xec
#define ATA_IDENTIFY_PAKCET_DEVICE 0xa1
#define ATA_PACKET 0xa0
#define ATA_READ_DMA_EXT 0x25
#define ATA_READ_DMA 0xc8
#define ATA_WRITE_DMA_EXT 0x35
#define ATA_WRITE_DMA 0xca

#define MAX_RETRY 2

struct sata_fis_head
{
    u8_t type;
    u8_t options;
    u8_t status_cmd;
    u8_t feat_err;
} __HBA_PACKED__;

struct sata_reg_fis
{
    struct sata_fis_head head;

    u8_t lba0, lba8, lba16;
    u8_t dev;
    u8_t lba24, lba32, lba40;
    u8_t feature;

    u16_t count;

    u8_t reserved[6];
} __HBA_PACKED__;

struct sata_data_fis
{
    struct sata_fis_head head;

    u8_t data[0];
} __HBA_PACKED__;

void
sata_create_fis(struct sata_reg_fis* cmd_fis,
                u8_t command,
                lba_t lba,
                u16_t sector_count);

void
sata_submit(struct hba_device* dev, struct blkio_req* io_req);

void
sata_read_error(struct hba_port* port);
#endif /* __LUNAIX_SATA_H */
