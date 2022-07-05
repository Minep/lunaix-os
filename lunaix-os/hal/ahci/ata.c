#include <hal/ahci/hba.h>
#include <hal/ahci/sata.h>

#include <lunaix/mm/valloc.h>
#include <lunaix/spike.h>

int
__sata_buffer_io(struct hba_port* port,
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
    header->options |= HBA_CMDH_WRITE * (write == 1);

    uint16_t count = size / port->device->block_size;
    if (count == 0 && size < port->device->block_size) {
        // 一个special case: 当count=0的时候，表示的是65536个区块会被传输
        // 如果这个0是因为不够除而导致的，那么说明size太小了
        goto fail;
    }

    struct sata_reg_fis* fis = table->command_fis;
    if ((port->device->flags & HBA_DEV_FEXTLBA)) {
        // 如果该设备支持48位LBA寻址
        sata_create_fis(
          fis, write ? ATA_WRITE_DMA_EXT : ATA_READ_DMA_EXT, lba, count);
    } else {
        sata_create_fis(fis, write ? ATA_WRITE_DMA : ATA_READ_DMA, lba, count);
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

int
sata_read_buffer(struct hba_port* port,
                 uint64_t lba,
                 void* buffer,
                 uint32_t size)
{
    return __sata_buffer_io(port, lba, buffer, size, 0);
}

int
sata_write_buffer(struct hba_port* port,
                  uint64_t lba,
                  void* buffer,
                  uint32_t size)
{
    return __sata_buffer_io(port, lba, buffer, size, 1);
}

void
sata_read_error(struct hba_port* port)
{
    uint32_t tfd = port->regs[HBA_RPxTFD];
    port->device->last_error = (tfd >> 8) & 0xff;
}