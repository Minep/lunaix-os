#include <hal/ahci/hba.h>
#include <hal/ahci/scsi.h>
#include <hal/ahci/utils.h>

#include <klibc/string.h>
#include <lunaix/spike.h>

void
scsi_create_packet12(struct scsi_cdb12* cdb,
                     uint8_t opcode,
                     uint32_t lba,
                     uint32_t alloc_size)
{
    memset(cdb, 0, sizeof(*cdb));
    cdb->opcode = opcode;
    cdb->lba_be = SCSI_FLIP(lba);
    cdb->length = alloc_size;
}

void
scsi_create_packet16(struct scsi_cdb16* cdb,
                     uint8_t opcode,
                     uint32_t lba_hi,
                     uint32_t lba_lo,
                     uint32_t alloc_size)
{
    memset(cdb, 0, sizeof(*cdb));
    cdb->opcode = opcode;
    cdb->lba_be_hi = SCSI_FLIP(lba_hi);
    cdb->lba_be_lo = SCSI_FLIP(lba_lo);
    cdb->length = alloc_size;
}

void
scsi_parse_capacity(struct hba_device* device, uint32_t* parameter)
{
    device->max_lba = SCSI_FLIP(*(parameter + 1));
    device->block_size = SCSI_FLIP(*(parameter + 2));
}