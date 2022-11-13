#include <hal/ahci/ahci.h>
#include <hal/ahci/hba.h>
#include <hal/ahci/sata.h>
#include <hal/ahci/scsi.h>

#include <klibc/string.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/spike.h>
#include <lunaix/syslog.h>

void
scsi_create_packet12(struct scsi_cdb12* cdb,
                     uint8_t opcode,
                     u32_t lba,
                     u32_t alloc_size)
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
                     u32_t alloc_size)
{
    memset(cdb, 0, sizeof(*cdb));
    cdb->opcode = opcode;
    cdb->lba_be_hi = SCSI_FLIP((u32_t)(lba >> 32));
    cdb->lba_be_lo = SCSI_FLIP((u32_t)lba);
    cdb->length = SCSI_FLIP(alloc_size);
}

void
scsi_parse_capacity(struct hba_device* device, u32_t* parameter)
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

void
scsi_submit(struct hba_device* dev, struct blkio_req* io_req)
{
    struct hba_port* port = dev->port;
    struct hba_cmdh* header;
    struct hba_cmdt* table;

    int write = !!(io_req->flags & BLKIO_WRITE);
    int slot = hba_prepare_cmd(port, &table, &header);
    hba_bind_vbuf(header, table, io_req->vbuf);

    header->options |= (HBA_CMDH_WRITE * (write == 1)) | HBA_CMDH_ATAPI;

    size_t size = vbuf_size(io_req->vbuf);
    u32_t count = ICEIL(size, port->device->block_size);

    struct sata_reg_fis* fis = (struct sata_reg_fis*)table->command_fis;
    void* cdb = table->atapi_cmd;
    sata_create_fis(fis, ATA_PACKET, (size << 8), 0);
    fis->feature = 1 | ((!write) << 2);

    if (port->device->cbd_size == SCSI_CDB16) {
        scsi_create_packet16((struct scsi_cdb16*)cdb,
                             write ? SCSI_WRITE_BLOCKS_16 : SCSI_READ_BLOCKS_16,
                             io_req->blk_addr,
                             count);
    } else {
        scsi_create_packet12((struct scsi_cdb12*)cdb,
                             write ? SCSI_WRITE_BLOCKS_12 : SCSI_READ_BLOCKS_12,
                             io_req->blk_addr & (u32_t)-1,
                             count);
    }

    // field: cdb->misc1
    *((uint8_t*)cdb + 1) = 3 << 5; // RPROTECT=011b 禁用保护检查

    // The async way...
    struct hba_cmd_state* cmds = valloc(sizeof(struct hba_cmd_state));
    *cmds = (struct hba_cmd_state){ .cmd_table = table, .state_ctx = io_req };
    ahci_post(port, cmds, slot);
}