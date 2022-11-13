#include <hal/ahci/ahci.h>
#include <hal/ahci/hba.h>
#include <hal/ahci/sata.h>
#include <hal/ahci/scsi.h>
#include <klibc/string.h>

#include <lunaix/spike.h>

#define IDDEV_OFFMAXLBA 60
#define IDDEV_OFFMAXLBA_EXT 230
#define IDDEV_OFFLSECSIZE 117
#define IDDEV_OFFWWN 108
#define IDDEV_OFFSERIALNUM 10
#define IDDEV_OFFMODELNUM 27
#define IDDEV_OFFADDSUPPORT 69
#define IDDEV_OFFALIGN 209
#define IDDEV_OFFLPP 106
#define IDDEV_OFFCAPABILITIES 49

static u32_t cdb_size[] = { SCSI_CDB12, SCSI_CDB16, 0, 0 };

void
ahci_parse_dev_info(struct hba_device* dev_info, uint16_t* data)
{
    dev_info->max_lba = *((u32_t*)(data + IDDEV_OFFMAXLBA));
    dev_info->block_size = *((u32_t*)(data + IDDEV_OFFLSECSIZE));
    dev_info->cbd_size = cdb_size[(*data & 0x3)];
    dev_info->wwn = *(uint64_t*)(data + IDDEV_OFFWWN);
    dev_info->block_per_sec = 1 << (*(data + IDDEV_OFFLPP) & 0xf);
    dev_info->alignment_offset = *(data + IDDEV_OFFALIGN) & 0x3fff;
    dev_info->capabilities = *((u32_t*)(data + IDDEV_OFFCAPABILITIES));

    if (!dev_info->block_size) {
        dev_info->block_size = 512;
    }

    if ((*(data + IDDEV_OFFADDSUPPORT) & 0x8)) {
        dev_info->max_lba = *((uint64_t*)(data + IDDEV_OFFMAXLBA_EXT));
        dev_info->flags |= HBA_DEV_FEXTLBA;
    }

    ahci_parsestr(dev_info->serial_num, data + IDDEV_OFFSERIALNUM, 10);
    ahci_parsestr(dev_info->model, data + IDDEV_OFFMODELNUM, 20);
}

void
ahci_parsestr(char* str, uint16_t* reg_start, int size_word)
{
    int j = 0;
    for (int i = 0; i < size_word; i++, j += 2) {
        uint16_t reg = *(reg_start + i);
        str[j] = (char)(reg >> 8);
        str[j + 1] = (char)(reg & 0xff);
    }
    str[j - 1] = '\0';
    strrtrim(str);
}

int
ahci_try_send(struct hba_port* port, int slot)
{
    int retries = 0, bitmask = 1 << slot;

    // 确保端口是空闲的
    wait_until(!(port->regs[HBA_RPxTFD] & (HBA_PxTFD_BSY | HBA_PxTFD_DRQ)));

    hba_clear_reg(port->regs[HBA_RPxIS]);

    while (retries < MAX_RETRY) {
        // PxCI寄存器置位，告诉HBA这儿有个数据需要发送到SATA端口
        port->regs[HBA_RPxCI] = bitmask;

        wait_until_expire(!(port->regs[HBA_RPxCI] & bitmask), 1000000);

        port->regs[HBA_RPxCI] &= ~bitmask; // ensure CI bit is cleared
        if ((port->regs[HBA_RPxTFD] & HBA_PxTFD_ERR)) {
            // 有错误
            sata_read_error(port);
            retries++;
        } else {
            break;
        }
    }

    hba_clear_reg(port->regs[HBA_RPxIS]);

    return retries < MAX_RETRY;
}

void
ahci_post(struct hba_port* port, struct hba_cmd_state* state, int slot)
{
    int bitmask = 1 << slot;

    // 确保端口是空闲的
    wait_until(!(port->regs[HBA_RPxTFD] & (HBA_PxTFD_BSY | HBA_PxTFD_DRQ)));

    hba_clear_reg(port->regs[HBA_RPxIS]);

    port->cmdctx.issued[slot] = state;
    port->cmdctx.tracked_ci |= bitmask;
    port->regs[HBA_RPxCI] |= bitmask;
}