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

struct ahci_driver
{
    struct llist_header ahci_drvs;
    struct ahci_hba hba;
    int id;
};

/**
 * @brief 初始化AHCI与HBA
 *
 */
void
ahci_init();

void
ahci_parse_dev_info(struct hba_device* dev_info, uint16_t* data);

void
ahci_parsestr(char* str, uint16_t* reg_start, int size_word);

/**
 * @brief Issue a HBA command (synchronized)
 *
 * @param port
 * @param slot
 * @return int
 */
int
ahci_try_send(struct hba_port* port, int slot);

/**
 * @brief Issue a HBA command (asynchronized)
 *
 * @param port
 * @param state
 * @param slot
 */
void
ahci_post(struct hba_port* port, struct hba_cmd_state* state, int slot);

#endif /* __LUNAIX_AHCI_H */
