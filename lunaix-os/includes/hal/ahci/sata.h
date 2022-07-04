#ifndef __LUNAIX_SATA_H
#define __LUNAIX_SATA_H

#include <stdint.h>

#define SATA_REG_FIS_D2H 0x34
#define SATA_REG_FIS_H2D 0x27
#define SATA_REG_FIS_COMMAND 0x80
#define SATA_LBA_COMPONENT(lba, offset) ((((lba_lo) >> (offset)) & 0xff))

struct sata_fis_head
{
    uint8_t type;
    uint8_t options;
    uint8_t status_cmd;
    uint8_t feat_err;
} __HBA_PACKED__;

struct sata_reg_fis
{
    struct sata_fis_head head;

    uint8_t lba0, lba8, lba16;
    uint8_t dev;
    uint8_t lba24, lba32, lba40;
    uint8_t reserved1;

    uint16_t count;

    uint8_t reserved[6];
} __HBA_PACKED__;

struct sata_data_fis
{
    struct sata_fis_head head;

    uint8_t data[0];
} __HBA_PACKED__;

void
sata_create_fis(struct sata_reg_fis* cmd_fis,
                uint8_t command,
                uint32_t lba_lo,
                uint32_t lba_hi,
                uint16_t sector_count);

#endif /* __LUNAIX_SATA_H */
