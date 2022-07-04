#ifndef __LUNAIX_AHCI_UTILS_H
#define __LUNAIX_AHCI_UTILS_H

#include <hal/ahci/hba.h>
#include <stdint.h>

void
ahci_parse_dev_info(struct hba_device* dev_info, uint16_t* data);

void
ahci_parsestr(char* str, uint16_t* reg_start, int size_word);

void
scsi_parse_capacity(struct hba_device* device, uint32_t* parameter);

#endif /* __LUNAIX_UTILS_H */
