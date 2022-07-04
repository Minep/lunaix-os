#ifndef __LUNAIX_AHCI_H
#define __LUNAIX_AHCI_H

#include <stdint.h>

/*
 * Macro naming rule:
 *      HBA_R[xxx]
 *          HBA Register [xxx]
 *          e.g. HBA_RPxCLB is Register PxCLB
 *
 * All registers offset are 0 based index of a DWORD array
 */

#define AHCI_HBA_CLASS 0x10601

#define HBA_RCAP 0
#define HBA_RGHC 1
#define HBA_RIS 2
#define HBA_RPI 3
#define HBA_RVER 4

#define HBA_RPBASE (0x40)
#define HBA_RPSIZE (0x80 >> 2)
#define HBA_RPxCLB 0
#define HBA_RPxFB 2
#define HBA_RPxIS 4
#define HBA_RPxIE 5
#define HBA_RPxCMD 6
#define HBA_RPxTFD 8
#define HBA_RPxSIG 9
#define HBA_RPxSSTS 10
#define HBA_RPxSCTL 11
#define HBA_RPxSERR 12
#define HBA_RPxSACT 13
#define HBA_RPxCI 14
#define HBA_RPxSNTF 15
#define HBA_RPxFBS 16

#define HBA_PxCMD_FRE (1 << 4)
#define HBA_PxCMD_CR (1 << 15)
#define HBA_PxCMD_FR (1 << 14)
#define HBA_PxCMD_ST (1)
#define HBA_PxINTR_DMA (1 << 2)
#define HBA_PxINTR_D2HR (1)

#define HBA_RGHC_ACHI_ENABLE (1 << 31)
#define HBA_RGHC_INTR_ENABLE (1 << 1)
#define HBA_RGHC_RESET 1

#define HBA_RPxSSTS_PWR(x) (((x) >> 8) & 0xf)
#define HBA_RPxSSTS_IF(x) (((x) >> 4) & 0xf)
#define HBA_RPxSSTS_PHYSTATE(x) ((x)&0xf)

#define HBA_DEV_SIG_ATAPI 0xeb140101
#define HBA_DEV_SIG_ATA 0x00000101

#define __HBA_PACKED__ __attribute__((packed))

typedef unsigned int hba_reg_t;

#define HBA_CMDH_FIS_LEN(fis_bytes) (((fis_bytes) / 4) & 0x1f)
#define HBA_CMDH_ATAPI (1 << 5)
#define HBA_CMDH_WRITE (1 << 6)
#define HBA_CMDH_PREFETCH (1 << 7)
#define HBA_CMDH_R (1 << 8)
#define HBA_CMDH_CLR_BUSY (1 << 10)
#define HBA_CMDH_PRDT_LEN(entries) (((entries)&0xffff) << 16)

struct ahci_hba_cmdh
{
    uint16_t options;
    uint16_t prdt_len;
    uint32_t transferred_size;
    uint32_t cmd_table_base;
    uint32_t reserved[5];
} __HBA_PACKED__;

#define HBA_PRDTE_BYTE_CNT(cnt) ((cnt & 0x3FFFFF) | 0x1)

struct ahci_hba_prdte
{
    uint32_t data_base;
    uint32_t reserved[2];
    uint32_t byte_count;
} __HBA_PACKED__;

struct ahci_hba_cmdt
{
    uint8_t command_fis[64];
    uint8_t atapi_cmd[16];
    uint8_t reserved[0x30];
    struct ahci_hba_prdte entries[3];
} __HBA_PACKED__;

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

struct ahci_device_info
{
    char serial_num[20];
    char model[40];
    uint32_t max_lba;
    uint32_t sector_size;
    uint8_t wwn[8];
};

struct ahci_port
{
    volatile hba_reg_t* regs;
    unsigned int ssts;
    struct ahci_hba_cmdh* cmdlst;
    struct sata_fis_head* fis;
    struct ahci_device_info* device_info;
};

struct ahci_hba
{
    volatile hba_reg_t* base;
    unsigned int ports_num;
    unsigned int cmd_slots;
    unsigned int version;
    struct ahci_port* ports[32];
};

#define ATA_IDENTIFY_DEVICE 0xec
#define ATA_IDENTIFY_PAKCET_DEVICE 0xa1

/**
 * @brief 初始化AHCI与HBA
 *
 */
void
ahci_init();

void
ahci_list_device();

int
ahci_identify_device(struct ahci_port* port);

#endif /* __LUNAIX_AHCI_H */
