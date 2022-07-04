#ifndef __LUNAIX_AHCI_H
#define __LUNAIX_AHCI_H

/*
 * Macro naming rule:
 *      HBA_R[xxx]
 *          HBA Register [xxx]
 *          e.g. HBA_RPxCLB is Register PxCLB
 *
 * All registers offset are 0 based index of a DWORD array
 */

#define AHCI_HBA_CLASS 0x10601

#define ATA_IDENTIFY_DEVICE 0xec
#define ATA_IDENTIFY_PAKCET_DEVICE 0xa1
#define ATA_PACKET 0xa0

/**
 * @brief 初始化AHCI与HBA
 *
 */
void
ahci_init();

void
ahci_list_device();

#endif /* __LUNAIX_AHCI_H */
