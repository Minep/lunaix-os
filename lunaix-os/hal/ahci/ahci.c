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
#include <hal/ahci.h>
#include <hal/pci.h>
#include <klibc/string.h>
#include <lunaix/mm/kalloc.h>
#include <lunaix/mm/mmio.h>
#include <lunaix/mm/pmm.h>
#include <lunaix/spike.h>
#include <lunaix/syslog.h>

#define HBA_FIS_SIZE 256
#define HBA_CLB_SIZE 1024

LOG_MODULE("AHCI")

static struct ahci_hba hba;

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

    hba.base = (hba_reg_t*)ioremap(PCI_BAR_ADDR_MM(bar6), size);

    // 重置HBA
    hba.base[HBA_RGHC] |= HBA_RGHC_RESET;
    wait_until(!(hba.base[HBA_RGHC] & HBA_RGHC_RESET));

    // 启用AHCI工作模式，启用中断
    hba.base[HBA_RGHC] |= (HBA_RGHC_ACHI_ENABLE | HBA_RGHC_INTR_ENABLE);

    // As per section 3.1.1, this is 0 based value.
    hba_reg_t cap = hba.base[HBA_RCAP];
    hba.ports_num = (cap & 0x1f) + 1;  // CAP.PI
    hba.cmd_slots = (cap >> 8) & 0x1f; // CAP.NCS
    hba.version = hba.base[HBA_RVER];

    /* ------ HBA端口配置 ------ */
    hba_reg_t pmap = hba.base[HBA_RPI];
    uintptr_t clb_pg_addr, fis_pg_addr, clb_pa, fis_pa;
    for (size_t i = 0, fisp = 0, clbp = 0; i < 32;
         i++, pmap >>= 1, fisp = (fisp + 1) % 16, clbp = (clbp + 1) % 4) {
        if (!(pmap & 0x1)) {
            continue;
        }

        struct ahci_port* port =
          (struct ahci_port*)lxmalloc(sizeof(struct ahci_port));
        hba_reg_t* port_regs =
          (hba_reg_t*)(&hba.base[HBA_RPBASE + i * HBA_RPSIZE]);

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

        *port =
          (struct ahci_port){ .regs = port_regs,
                              .ssts = port_regs[HBA_RPxSSTS],
                              .cmdlstv = clb_pg_addr + clbp * HBA_CLB_SIZE,
                              .fisv = fis_pg_addr + fisp * HBA_FIS_SIZE };

        /* 初始化端口，并置于就绪状态 */
        port_regs[HBA_RPxCI] = 0;
        port_regs[HBA_RPxIE] |= (HBA_PxINTR_DMA | HBA_PxINTR_D2HR);
        port_regs[HBA_RPxCMD] |= (HBA_PxCMD_FRE | HBA_PxCMD_ST);

        hba.ports[i] = port;
    }
}

char sata_ifs[][20] = { "Not detected",
                        "SATA I (1.5Gbps)",
                        "SATA II (3.0Gbps)",
                        "SATA III (6.0Gbps)" };

void
ahci_list_device()
{
    kprintf(KINFO "Version: %x; Ports: %d\n", hba.version, hba.ports_num);
    for (size_t i = 0; i < 32; i++) {
        struct ahci_port* port = hba.ports[i];
        if (!port)
            continue;
        kprintf("\t Port %d: %s\n", i, &sata_ifs[HBA_RPxSSTS_IF(port->ssts)]);
    }
}