/**
 * @file ahci.c
 * @author Lunaixsky (zelong56@gmail.com)
 * @brief A software implementation of Serial ATA AHCI 1.3.1 Specification
 * @version 0.1
 * @date 2022-06-28
 *
 * @copyright Copyright (c) 2022
 *
 */
#include <hal/ahci/ahci.h>
#include <hal/ahci/hba.h>
#include <hal/ahci/sata.h>
#include <hal/ahci/scsi.h>
#include <hal/ahci/utils.h>

#include <hal/pci.h>
#include <klibc/string.h>
#include <lunaix/block.h>
#include <lunaix/mm/mmio.h>
#include <lunaix/mm/pmm.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/spike.h>
#include <lunaix/syslog.h>

#define HBA_FIS_SIZE 256
#define HBA_CLB_SIZE 1024

// #define DO_HBA_FULL_RESET

LOG_MODULE("AHCI")

static struct ahci_hba hba;

void
__ahci_hba_isr(const isr_param* param);

int
ahci_init_device(struct hba_port* port);

void
achi_register_ops(struct hba_port* port);

unsigned int
ahci_get_port_usage()
{
    return hba.ports_bmp;
}

struct hba_port*
ahci_get_port(unsigned int index)
{
    if (index >= 32) {
        return 0;
    }
    return hba.ports[index];
}

void
__hba_reset_port(hba_reg_t* port_reg)
{
    // 根据：SATA-AHCI spec section 10.4.2 描述的端口重置流程
    port_reg[HBA_RPxCMD] &= ~HBA_PxCMD_ST;
    port_reg[HBA_RPxCMD] &= ~HBA_PxCMD_FRE;
    int cnt = wait_until_expire(!(port_reg[HBA_RPxCMD] & HBA_PxCMD_CR), 500000);
    if (cnt) {
        return;
    }
    // 如果port未响应，则继续执行重置
    port_reg[HBA_RPxSCTL] = (port_reg[HBA_RPxSCTL] & ~0xf) | 1;
    io_delay(100000); //等待至少一毫秒，差不多就行了
    port_reg[HBA_RPxSCTL] &= ~0xf;
}

void
ahci_init()
{
    struct pci_device* ahci_dev = pci_get_device_by_class(AHCI_HBA_CLASS);
    assert_msg(ahci_dev, "AHCI: Not found.");

    uintptr_t bar6, size;
    size = pci_bar_sizing(ahci_dev, &bar6, 6);
    assert_msg(bar6 && PCI_BAR_MMIO(bar6), "AHCI: BAR#6 is not MMIO.");

    pci_reg_t cmd = pci_read_cspace(ahci_dev->cspace_base, PCI_REG_STATUS_CMD);

    // 禁用传统中断（因为我们使用MSI），启用MMIO访问，允许PCI设备间访问
    cmd |= (PCI_RCMD_MM_ACCESS | PCI_RCMD_DISABLE_INTR | PCI_RCMD_BUS_MASTER);

    pci_write_cspace(ahci_dev->cspace_base, PCI_REG_STATUS_CMD, cmd);

    pci_setup_msi(ahci_dev, AHCI_HBA_IV);
    intr_subscribe(AHCI_HBA_IV, __ahci_hba_isr);

    memset(&hba, 0, sizeof(hba));

    hba.base = (hba_reg_t*)ioremap(PCI_BAR_ADDR_MM(bar6), size);

#ifdef DO_HBA_FULL_RESET
    // 重置HBA
    hba.base[HBA_RGHC] |= HBA_RGHC_RESET;
    wait_until(!(hba.base[HBA_RGHC] & HBA_RGHC_RESET));
#endif

    // 启用AHCI工作模式，启用中断
    hba.base[HBA_RGHC] |= HBA_RGHC_ACHI_ENABLE;
    hba.base[HBA_RGHC] |= HBA_RGHC_INTR_ENABLE;

    // As per section 3.1.1, this is 0 based value.
    hba_reg_t cap = hba.base[HBA_RCAP];
    hba_reg_t pmap = hba.base[HBA_RPI];

    hba.ports_num = (cap & 0x1f) + 1;  // CAP.PI
    hba.cmd_slots = (cap >> 8) & 0x1f; // CAP.NCS
    hba.version = hba.base[HBA_RVER];
    hba.ports_bmp = pmap;

    /* ------ HBA端口配置 ------ */
    uintptr_t clb_pg_addr, fis_pg_addr, clb_pa, fis_pa;
    for (size_t i = 0, fisp = 0, clbp = 0; i < 32;
         i++, pmap >>= 1, fisp = (fisp + 1) % 16, clbp = (clbp + 1) % 4) {
        if (!(pmap & 0x1)) {
            continue;
        }

        struct hba_port* port =
          (struct hba_port*)valloc(sizeof(struct hba_port));
        hba_reg_t* port_regs =
          (hba_reg_t*)(&hba.base[HBA_RPBASE + i * HBA_RPSIZE]);

#ifndef DO_HBA_FULL_RESET
        __hba_reset_port(port_regs);
#endif

        if (!clbp) {
            // 每页最多4个命令队列
            clb_pa = pmm_alloc_page(KERNEL_PID, PP_FGLOCKED);
            clb_pg_addr = ioremap(clb_pa, 0x1000);
            memset(clb_pg_addr, 0, 0x1000);
        }
        if (!fisp) {
            // 每页最多16个FIS
            fis_pa = pmm_alloc_page(KERNEL_PID, PP_FGLOCKED);
            fis_pg_addr = ioremap(fis_pa, 0x1000);
            memset(fis_pg_addr, 0, 0x1000);
        }

        /* 重定向CLB与FIS */
        port_regs[HBA_RPxCLB] = clb_pa + clbp * HBA_CLB_SIZE;
        port_regs[HBA_RPxFB] = fis_pa + fisp * HBA_FIS_SIZE;

        *port = (struct hba_port){ .regs = port_regs,
                                   .ssts = port_regs[HBA_RPxSSTS],
                                   .cmdlst = clb_pg_addr + clbp * HBA_CLB_SIZE,
                                   .fis = fis_pg_addr + fisp * HBA_FIS_SIZE };

        /* 初始化端口，并置于就绪状态 */
        port_regs[HBA_RPxCI] = 0;

        // 需要通过全部置位去清空这些寄存器（相当的奇怪……）
        port_regs[HBA_RPxSERR] = -1;

        hba.ports[i] = port;

        if (!HBA_RPxSSTS_IF(port->ssts)) {
            continue;
        }

        wait_until(!(port_regs[HBA_RPxCMD] & HBA_PxCMD_CR));
        port_regs[HBA_RPxCMD] |= HBA_PxCMD_FRE;
        port_regs[HBA_RPxCMD] |= HBA_PxCMD_ST;

        if (!ahci_init_device(port)) {
            kprintf(KERROR "fail to init device");
        }

        block_mount_disk(port->device);
    }
}

char sata_ifs[][20] = { "Not detected",
                        "SATA I (1.5Gbps)",
                        "SATA II (3.0Gbps)",
                        "SATA III (6.0Gbps)" };

void
__ahci_hba_isr(const isr_param* param)
{
    // TODO: clear the interrupt status
    // TODO: I/O-operation scheduler should be here
    kprintf(KDEBUG "HBA INTR\n");
}

void
ahci_list_device()
{
    kprintf(KINFO "Version: %x; Ports: %d; Slot: %d\n",
            hba.version,
            hba.ports_num,
            hba.cmd_slots);
    struct hba_port* port;
    for (size_t i = 0; i < 32; i++) {
        port = hba.ports[i];

        // 愚蠢的gcc似乎认为 struct hba_port* 不可能为空
        //  所以将这个非常关键的if给优化掉了。
        //  这里将指针强制转换为整数，欺骗gcc :)
        if ((uintptr_t)port == 0) {
            continue;
        }

        int device_state = HBA_RPxSSTS_IF(port->ssts);

        kprintf("\t Port %d: %s (%x)\n",
                i,
                &sata_ifs[device_state],
                port->device->flags);

        struct hba_device* dev_info = port->device;
        if (!device_state || !dev_info) {
            continue;
        }
        kprintf("\t\t capacity: %d KiB\n",
                (dev_info->max_lba * dev_info->block_size) >> 10);
        kprintf("\t\t block size: %dB\n", dev_info->block_size);
        kprintf("\t\t block/sector: %d\n", dev_info->block_per_sec);
        kprintf("\t\t alignment: %dB\n", dev_info->alignment_offset);
        kprintf("\t\t capabilities: %x\n", dev_info->capabilities);
        kprintf("\t\t model: %s\n", &dev_info->model);
        kprintf("\t\t serial: %s\n", &dev_info->serial_num);
    }
}

int
__get_free_slot(struct hba_port* port)
{
    hba_reg_t pxsact = port->regs[HBA_RPxSACT];
    hba_reg_t pxci = port->regs[HBA_RPxCI];
    hba_reg_t free_bmp = pxsact | pxci;
    uint32_t i = 0;
    for (; i <= hba.cmd_slots && (free_bmp & 0x1); i++, free_bmp >>= 1)
        ;
    return i | -(i > hba.cmd_slots);
}

void
sata_create_fis(struct sata_reg_fis* cmd_fis,
                uint8_t command,
                uint64_t lba,
                uint16_t sector_count)
{
    cmd_fis->head.type = SATA_REG_FIS_H2D;
    cmd_fis->head.options = SATA_REG_FIS_COMMAND;
    cmd_fis->head.status_cmd = command;
    cmd_fis->dev = 0;

    cmd_fis->lba0 = SATA_LBA_COMPONENT(lba, 0);
    cmd_fis->lba8 = SATA_LBA_COMPONENT(lba, 8);
    cmd_fis->lba16 = SATA_LBA_COMPONENT(lba, 16);
    cmd_fis->lba24 = SATA_LBA_COMPONENT(lba, 24);

    cmd_fis->lba32 = SATA_LBA_COMPONENT(lba, 32);
    cmd_fis->lba40 = SATA_LBA_COMPONENT(lba, 40);

    cmd_fis->count = sector_count;
}

int
hba_prepare_cmd(struct hba_port* port,
                struct hba_cmdt** cmdt,
                struct hba_cmdh** cmdh,
                void* buffer,
                unsigned int size)
{
    int slot = __get_free_slot(port);
    assert_msg(slot >= 0, "HBA: No free slot");
    assert_msg(size <= 0x400000, "HBA: buffer too big");

    // 构建命令头（Command Header）和命令表（Command Table）
    struct hba_cmdh* cmd_header = &port->cmdlst[slot];
    struct hba_cmdt* cmd_table = vzalloc_dma(sizeof(struct hba_cmdt));

    memset(cmd_header, 0, sizeof(*cmd_header));

    // 将命令表挂到命令头上
    cmd_header->cmd_table_base = vmm_v2p(cmd_table);
    cmd_header->options =
      HBA_CMDH_FIS_LEN(sizeof(struct sata_reg_fis)) | HBA_CMDH_CLR_BUSY;

    if (buffer) {
        cmd_header->prdt_len = 1;
        cmd_table->entries[0] =
          (struct hba_prdte){ .data_base = vmm_v2p(buffer),
                              .byte_count = size - 1 };
    }

    *cmdh = cmd_header;
    *cmdt = cmd_table;

    return slot;
}

int
ahci_init_device(struct hba_port* port)
{
    /* 发送ATA命令，参考：SATA AHCI Spec Rev.1.3.1, section 5.5 */
    struct hba_cmdt* cmd_table;
    struct hba_cmdh* cmd_header;

    // mask DHR interrupt
    port->regs[HBA_RPxIE] &= ~HBA_PxINTR_DHR;

    // 确保端口是空闲的
    wait_until(!(port->regs[HBA_RPxTFD] & (HBA_PxTFD_BSY)));

    // 预备DMA接收缓存，用于存放HBA传回的数据
    uint16_t* data_in = (uint16_t*)valloc_dma(512);

    int slot = hba_prepare_cmd(port, &cmd_table, &cmd_header, data_in, 512);

    // 清空任何待响应的中断
    port->regs[HBA_RPxIS] = 0;
    port->device = vzalloc(sizeof(struct hba_device));
    port->device->port = port;

    // 在命令表中构建命令FIS
    struct sata_reg_fis* cmd_fis = (struct sata_reg_fis*)cmd_table->command_fis;

    // 根据设备类型使用合适的命令
    if (port->regs[HBA_RPxSIG] == HBA_DEV_SIG_ATA) {
        // ATA 一般为硬盘
        sata_create_fis(cmd_fis, ATA_IDENTIFY_DEVICE, 0, 0);
    } else {
        // ATAPI 一般为光驱，软驱，或者磁带机
        port->device->flags |= HBA_DEV_FATAPI;
        sata_create_fis(cmd_fis, ATA_IDENTIFY_PAKCET_DEVICE, 0, 0);
    }

    // PxCI寄存器置位，告诉HBA这儿有个数据需要发送到SATA端口
    port->regs[HBA_RPxCI] = (1 << slot);

    wait_until(!(port->regs[HBA_RPxCI] & (1 << slot)));

    if ((port->regs[HBA_RPxTFD] & HBA_PxTFD_ERR)) {
        // 有错误
        sata_read_error(port);
        goto fail;
    }

    /*
        等待数据到达内存
        解析IDENTIFY DEVICE传回来的数据。
          参考：
            * ATA/ATAPI Command Set - 3 (ACS-3), Section 7.12.7
    */
    ahci_parse_dev_info(port->device, data_in);

    if (!(port->device->flags & HBA_DEV_FATAPI)) {
        goto done;
    }

    /*
        注意：ATAPI设备是无法通过IDENTIFY PACKET DEVICE 获取容量信息的。
        我们需要使用SCSI命令的READ_CAPACITY(16)进行获取。
        步骤如下：
            1. 因为ATAPI走的是SCSI，而AHCI对此专门进行了SATA的封装，
               也就是通过SATA的PACKET命令对SCSI命令进行封装。所以我们
               首先需要构建一个PACKET命令的FIS
            2. 接着，在ACMD中构建命令READ_CAPACITY的CDB - 一种SCSI命令的封装
            3. 然后把cmd_header->options的A位置位，表示这是一个送往ATAPI的命令。
                一点细节：
                    1. HBA往底层SATA控制器发送PACKET FIS
                    2. SATA控制器回复PIO Setup FIS
                    3. HBA读入ACMD中的CDB，打包成Data FIS进行答复
                    4. SATA控制器解包，拿到CDB，通过SCSI协议转发往ATAPI设备。
                    5. ATAPI设备回复Return Parameter，SATA通过DMA Setup FIS
                       发起DMA请求，HBA介入，将Return Parameter写入我们在PRDT
                       里设置的data_in位置。
            4. 最后照常等待HBA把结果写入data_in，然后直接解析就好了。
          参考：
            * ATA/ATAPI Command Set - 3 (ACS-3), Section 7.18
            * SATA AHCI HBA Spec, Section 5.3.7
            * SCSI Command Reference Manual, Section 3.26
    */
    struct scsi_cdb16* cdb16 = (struct scsi_cdb16*)cmd_table->atapi_cmd;

    sata_create_fis(cmd_fis, ATA_PACKET, 512 << 8, 0);
    scsi_create_packet16(cdb16, SCSI_READ_CAPACITY_16, 0, 512);

    cdb16->misc1 = 0x10; // service action
    cmd_header->transferred_size = 0;
    cmd_header->options |= HBA_CMDH_ATAPI;

    port->regs[HBA_RPxCI] = (1 << slot);
    wait_until(!(port->regs[HBA_RPxCI] & (1 << slot)));

    if ((port->regs[HBA_RPxTFD] & HBA_PxTFD_ERR)) {
        // 有错误
        sata_read_error(port);
        goto fail;
    }

    scsi_parse_capacity(port->device, (uint32_t*)data_in);

done:
    // reset interrupt status and unmask D2HR interrupt
    port->regs[HBA_RPxIS] = -1;
    port->regs[HBA_RPxIE] |= HBA_PxINTR_DHR;
    achi_register_ops(port);

    vfree_dma(data_in);
    vfree_dma(cmd_table);

    return 1;

fail:
    port->regs[HBA_RPxIS] = -1;
    port->regs[HBA_RPxIE] |= HBA_PxINTR_DHR;
    vfree_dma(data_in);
    vfree_dma(cmd_table);

    return 0;
}

int
ahci_identify_device(struct hba_device* device)
{
    // 用于重新识别设备（比如在热插拔的情况下）
    vfree(device);
    return ahci_init_device(device->port);
}

void
achi_register_ops(struct hba_port* port)
{
    port->device->ops.identify = ahci_identify_device;
    if (!(port->device->flags & HBA_DEV_FATAPI)) {
        port->device->ops.read_buffer = sata_read_buffer;
        port->device->ops.write_buffer = sata_write_buffer;
    } else {
        port->device->ops.read_buffer = scsi_read_buffer;
        port->device->ops.write_buffer = scsi_write_buffer;
    }
}