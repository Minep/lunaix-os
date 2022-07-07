#ifndef __LUNAIX_AHCI_H
#define __LUNAIX_AHCI_H

#include "hba.h"

/*
 * Macro naming rule:
 *      HBA_R[xxx]
 *          HBA Register [xxx]
 *          e.g. HBA_RPxCLB is Register PxCLB
 *
 * All registers offset are 0 based index of a DWORD array
 */

#define AHCI_HBA_CLASS 0x10601

/**
 * @brief 初始化AHCI与HBA
 *
 */
void
ahci_init();

void
ahci_list_device();

unsigned int
ahci_get_port_usage();

struct hba_port*
ahci_get_port(unsigned int index);

#endif /* __LUNAIX_AHCI_H */
