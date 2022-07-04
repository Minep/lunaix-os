#ifndef __LUNAIX_AHCI_UTILS_H
#define __LUNAIX_AHCI_UTILS_H

#include <hal/ahci/ahci.h>
#include <stdint.h>

void
ahci_parse_dev_info(struct ahci_device_info* dev_info, uint16_t* data);

void
ahci_parsestr(char* str, uint16_t* reg_start, int size_word);

#endif /* __LUNAIX_UTILS_H */
