#ifndef __LUNAIX_HBA_H
#define __LUNAIX_HBA_H

#include <lunaix/blkio.h>
#include <lunaix/buffer.h>
#include <lunaix/types.h>

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
#define HBA_PxINTR_DHR (1)
#define HBA_PxINTR_DPS (1 << 5)
#define HBA_PxINTR_TFE (1 << 30)
#define HBA_PxINTR_HBF (1 << 29)
#define HBA_PxINTR_HBD (1 << 28)
#define HBA_PxINTR_IF (1 << 27)
#define HBA_PxINTR_NIF (1 << 26)
#define HBA_PxINTR_OF (1 << 24)
#define HBA_PxTFD_ERR (1)
#define HBA_PxTFD_BSY (1 << 7)
#define HBA_PxTFD_DRQ (1 << 3)

#define HBA_FATAL                                                              \
    (HBA_PxINTR_TFE | HBA_PxINTR_HBF | HBA_PxINTR_HBD | HBA_PxINTR_IF)

#define HBA_NONFATAL (HBA_PxINTR_NIF | HBA_PxINTR_OF)

#define HBA_RGHC_ACHI_ENABLE (1 << 31)
#define HBA_RGHC_INTR_ENABLE (1 << 1)
#define HBA_RGHC_RESET 1

#define HBA_RPxSSTS_PWR(x) (((x) >> 8) & 0xf)
#define HBA_RPxSSTS_IF(x) (((x) >> 4) & 0xf)
#define HBA_RPxSSTS_PHYSTATE(x) ((x)&0xf)

#define hba_clear_reg(reg) (reg) = -1

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

#define HBA_MAX_PRDTE 4

struct hba_cmdh
{
    uint16_t options;
    uint16_t prdt_len;
    u32_t transferred_size;
    u32_t cmd_table_base;
    u32_t reserved[5];
} __HBA_PACKED__;

#define HBA_PRDTE_BYTE_CNT(cnt) ((cnt & 0x3FFFFF) | 0x1)

struct hba_prdte
{
    u32_t data_base;
    u32_t reserved[2];
    u32_t byte_count;
} __HBA_PACKED__;

struct hba_cmdt
{
    uint8_t command_fis[64];
    uint8_t atapi_cmd[16];
    uint8_t reserved[0x30];
    struct hba_prdte entries[HBA_MAX_PRDTE];
} __HBA_PACKED__;

#define HBA_DEV_FEXTLBA 1
#define HBA_DEV_FATAPI (1 << 1)

struct hba_port;
struct ahci_hba;

struct hba_device
{
    char serial_num[20];
    char model[40];
    u32_t flags;
    uint64_t max_lba;
    u32_t block_size;
    uint64_t wwn;
    uint8_t cbd_size;
    struct
    {
        uint8_t sense_key;
        uint8_t error;
        uint8_t status;
        uint8_t reserve;
    } last_result;
    u32_t alignment_offset;
    u32_t block_per_sec;
    u32_t capabilities;
    struct hba_port* port;
    struct ahci_hba* hba;

    struct
    {
        int (*identify)(struct hba_device* dev);
        void (*submit)(struct hba_device* dev, struct blkio_req* io_req);
    } ops;
};

struct hba_cmd_state
{
    struct hba_cmdt* cmd_table;
    void* state_ctx;
};

struct hba_cmd_context
{
    struct hba_cmd_state* issued[32];
    u32_t tracked_ci;
};

struct hba_port
{
    volatile hba_reg_t* regs;
    unsigned int ssts;
    struct hba_cmdh* cmdlst;
    struct hba_cmd_context cmdctx;
    void* fis;
    struct hba_device* device;
    struct ahci_hba* hba;
};

struct ahci_hba
{
    volatile hba_reg_t* base;
    unsigned int ports_num;
    unsigned int ports_bmp;
    unsigned int cmd_slots;
    unsigned int version;
    struct hba_port* ports[32];
};

int
hba_prepare_cmd(struct hba_port* port,
                struct hba_cmdt** cmdt,
                struct hba_cmdh** cmdh);

int
hba_bind_vbuf(struct hba_cmdh* cmdh,
              struct hba_cmdt* cmdt,
              struct vecbuf* vbuf);

int
hba_bind_sbuf(struct hba_cmdh* cmdh, struct hba_cmdt* cmdt, struct membuf mbuf);

#endif /* __LUNAIX_HBA_H */
