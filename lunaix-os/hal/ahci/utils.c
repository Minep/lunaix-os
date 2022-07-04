#include <hal/ahci/utils.h>
#include <klibc/string.h>

#define IDDEV_MAXLBA_OFFSET 60
#define IDDEV_LSECSIZE_OFFSET 117
#define IDDEV_WWN_OFFSET 108
#define IDDEV_SERIALNUM_OFFSET 10
#define IDDEV_MODELNUM_OFFSET 27

void
ahci_parse_dev_info(struct ahci_device_info* dev_info, uint16_t* data)
{
    dev_info->max_lba = *((uint32_t*)(data + IDDEV_MAXLBA_OFFSET));
    dev_info->sector_size = *((uint32_t*)(data + IDDEV_LSECSIZE_OFFSET));
    memcpy(&dev_info->wwn, (uint8_t*)(data + IDDEV_WWN_OFFSET), 8);
    if (!dev_info->sector_size) {
        dev_info->sector_size = 512;
    }
    ahci_parsestr(&dev_info->serial_num, data + IDDEV_SERIALNUM_OFFSET, 10);
    ahci_parsestr(&dev_info->model, data + IDDEV_MODELNUM_OFFSET, 20);
}

void
ahci_parsestr(char* str, uint16_t* reg_start, int size_word)
{
    for (int i = 0, j = 0; i < size_word; i++, j += 2) {
        uint16_t reg = *(reg_start + i);
        str[j] = (char)(reg >> 8);
        str[j + 1] = (char)(reg & 0xff);
    }
}