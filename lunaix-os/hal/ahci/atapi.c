#include <hal/ahci/hba.h>
#include <hal/ahci/sata.h>
#include <hal/ahci/scsi.h>
#include <hal/ahci/utils.h>

#include <klibc/string.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/spike.h>
#include <lunaix/syslog.h>

void
scsi_create_packet12(struct scsi_cdb12* cdb,
                     uint8_t opcode,
                     uint32_t lba,
                     uint32_t alloc_size)
{
    memset(cdb, 0, sizeof(*cdb));
    cdb->opcode = opcode;
    cdb->lba_be = SCSI_FLIP(lba);
    cdb->length = SCSI_FLIP(alloc_size);
}

void
scsi_create_packet16(struct scsi_cdb16* cdb,
                     uint8_t opcode,
                     uint64_t lba,
                     uint32_t alloc_size)
{
    memset(cdb, 0, sizeof(*cdb));
    cdb->opcode = opcode;
    cdb->lba_be_hi = SCSI_FLIP((uint32_t)(lba >> 32));
    cdb->lba_be_lo = SCSI_FLIP((uint32_t)lba);
    cdb->length = SCSI_FLIP(alloc_size);
}

void
scsi_parse_capacity(struct hba_device* device, uint32_t* parameter)
{
    if (device->cbd_size == SCSI_CDB16) {
        device->max_lba =
          SCSI_FLIP(*(parameter + 1)) | (SCSI_FLIP(*parameter) << 32);
        device->block_size = SCSI_FLIP(*(parameter + 2));
    } else {
        // for READ_CAPACITY(10)
        device->max_lba = SCSI_FLIP(*(parameter));
        device->block_size = SCSI_FLIP(*(parameter + 1));
    }
}

int
__scsi_buffer_io(struct hba_device* dev,
                 uint64_t lba,
                 void* buffer,
                 uint32_t size,
                 int write)
{
    assert_msg(((uintptr_t)buffer & 0x3) == 0, "HBA: Bad buffer alignment");

    struct hba_port* port = dev->port;
    struct hba_cmdh* header;
    struct hba_cmdt* table;
    int slot = hba_prepare_cmd(port, &table, &header, buffer, size);
    int bitmask = 1 << slot;

    // 确保端口是空闲的
    wait_until(!(port->regs[HBA_RPxTFD] & (HBA_PxTFD_BSY | HBA_PxTFD_DRQ)));

    port->regs[HBA_RPxIS] = 0;

    header->options |= (HBA_CMDH_WRITE * (write == 1)) | HBA_CMDH_ATAPI;

    uint32_t count = ICEIL(size, port->device->block_size);

    struct sata_reg_fis* fis = (struct sata_reg_fis*)table->command_fis;
    void* cdb = table->atapi_cmd;
    sata_create_fis(fis, ATA_PACKET, (size << 8), 0);
    fis->feature = 1 | ((!write) << 2);

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

    // field: cdb->misc1
    *((uint8_t*)cdb + 1) = 3 << 5; // RPROTECT=011b 禁用保护检查

    int retries = 0;

    while (retries < MAX_RETRY) {
        port->regs[HBA_RPxCI] = bitmask;

        wait_until_expire(!(port->regs[HBA_RPxCI] & bitmask), 1000000);

        if ((port->regs[HBA_RPxTFD] & HBA_PxTFD_ERR)) {
            // 有错误
            sata_read_error(port);
            retries++;
        } else {
            vfree_dma(table);
            return 1;
        }
    }

fail:
    vfree_dma(table);
    return 0;
}

int
scsi_read_buffer(struct hba_device* dev,
                 uint64_t lba,
                 void* buffer,
                 uint32_t size)
{
    return __scsi_buffer_io(dev, lba, buffer, size, 0);
}

int
scsi_write_buffer(struct hba_device* dev,
                  uint64_t lba,
                  void* buffer,
                  uint32_t size)
{
    return __scsi_buffer_io(dev, lba, buffer, size, 1);
}