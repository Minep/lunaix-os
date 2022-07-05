#include <hal/ahci/hba.h>
#include <hal/ahci/sata.h>
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
                     uint64_t lba,
                     uint32_t alloc_size)
{
    memset(cdb, 0, sizeof(*cdb));
    cdb->opcode = opcode;
    cdb->lba_be_hi = SCSI_FLIP(lba >> 32);
    cdb->lba_be_lo = SCSI_FLIP((uint32_t)lba);
    cdb->length = alloc_size;
}

void
scsi_parse_capacity(struct hba_device* device, uint32_t* parameter)
{
    device->max_lba = SCSI_FLIP(*(parameter + 1));
    device->block_size = SCSI_FLIP(*(parameter + 2));
}

void
__scsi_buffer_io(struct hba_port* port,
                 uint64_t lba,
                 void* buffer,
                 uint32_t size,
                 int write)
{
    assert_msg(((uintptr_t)buffer & 0x3) == 0, "HBA: Bad buffer alignment");

    struct hba_cmdh* header;
    struct hba_cmdt* table;
    int slot = hba_alloc_slot(port, &table, &header, 0);
    int bitmask = 1 << slot;

    // 确保端口是空闲的
    wait_until(!(port->regs[HBA_RPxTFD] & (HBA_PxTFD_BSY)));

    port->regs[HBA_RPxIS] = 0;

    table->entries[0] =
      (struct hba_prdte){ .byte_count = size - 1, .data_base = buffer };
    header->prdt_len = 1;
    header->options |= (HBA_CMDH_WRITE * (write == 1)) | HBA_CMDH_ATAPI;

    uint32_t count = size / port->device->block_size;
    if (count == 0) {
        // 对ATAPI设备来说，READ/WRITE (16/12) 没有这个count=0的 special case
        //  但是对于READ/WRITE (6)，就存在这个special case
        goto fail;
    }

    struct sata_reg_fis* fis = table->command_fis;
    void* cdb = table->atapi_cmd;
    sata_create_fis(fis, ATA_PACKET, (size << 8) & 0xffffff, 0);

    if (port->device->cbd_size == 16) {
        scsi_create_packet16((struct scsi_cdb16*)cdb,
                             write ? SCSI_WRITE_BLOCKS_16 : SCSI_READ_BLOCKS_16,
                             lba,
                             count);
    } else {
        scsi_create_packet12((struct scsi_cdb12*)cdb,
                             write ? SCSI_WRITE_BLOCKS_12 : SCSI_READ_BLOCKS_12,
                             lba,
                             count);
    }

    port->regs[HBA_RPxCI] = bitmask;

    wait_until(!(port->regs[HBA_RPxCI] & bitmask));

    if ((port->regs[HBA_RPxTFD] & HBA_PxTFD_ERR)) {
        // 有错误
        sata_read_error(port);
        goto fail;
    }

    vfree_dma(table);
    return 1;

fail:
    vfree_dma(table);
    return 0;
}

void
scsi_read_buffer(struct hba_port* port,
                 uint64_t lba,
                 void* buffer,
                 uint32_t size)
{
    __scsi_buffer_io(port, lba, buffer, size, 0);
}

void
scsi_write_buffer(struct hba_port* port,
                  uint64_t lba,
                  void* buffer,
                  uint32_t size)
{
    __scsi_buffer_io(port, lba, buffer, size, 1);
}