#ifndef __LUNAIX_FADT_H
#define __LUNAIX_FADT_H

#include "sdt.h"

// Pulled from ACPI Specification (v6.4) section 5.2.9

enum PMProfile
{
    Desktop = 1,
    Mobile = 2,
    Workstation = 3,
    EnterpriseServer = 4,
    SOHOServer = 5,
    AppliancePC = 6,
    PerformanceServer = 7,
    Tablet = 8,
};

#define IPM1AEVT_BLK_PORT 0
#define IPM1BEVT_BLK_PORT 1
#define IPM1ACNT_BLK_PORT 2
#define IPM1BCNT_BLK_PORT 3
#define IPM2CNT_BLK_PORT 4
#define IPMTMR_BLK_PORT 5

#define IPM1EVT_BLK_LEN 0
#define IPM1CNT_BLK_LEN 1
#define IPM2CNT_BLK_LEN 2
#define IPMTMR_BLK_LEN 3

#define ITIME_DAY_ALARM 0
#define ITIME_MON_ALARM 1
#define ITIME_CENTURY 2

#define IAPC_ARCH_LEGACY 0x1
#define IAPC_ARCH_8042 0x2
#define IAPC_ARCH_NO_VGA 0x4
#define IAPC_ARCH_NO_MSI 0x8
#define IAPC_ARCH_ASPM 0x10
#define IAPC_ARCH_NO_RTC 0x20

typedef struct acpi_fadt
{
    acpi_sdthdr_t header;
    u32_t firmware_controller_addr;
    u32_t dsdt_addr;
    u8_t reserved;
    u8_t pm_profile;
    u16_t sci_int;
    u32_t smi_cmd_port_addr;
    u8_t smi_acpi_enable;
    u8_t smi_acpi_disable;
    u8_t smi_s4bios_state;
    u8_t smi_pstate;
    u32_t pm_reg_ports[6];
    u32_t gpe0_port_addr;
    u32_t gpe1_port_addr;
    u8_t pm_reg_lens[4];
    u8_t gpe0_len;
    u8_t gpe1_len;
    u8_t gpe1_base;
    u8_t cst_cnt;
    u16_t p_lvl2_lat;
    u16_t p_lvl3_lat;
    u16_t flush_size;
    u16_t flush_stride;
    u8_t duty_offset;
    u8_t duty_width;
    u8_t time_info[3];
    u16_t boot_arch;
} ACPI_TABLE_PACKED acpi_fadt_t;

// TODO: FADT parser & support

#endif /* __LUNAIX_FADT_H */
