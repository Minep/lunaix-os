#ifndef __LUNAIX_AA64_INIT_H
#define __LUNAIX_AA64_INIT_H

#include <lunaix/types.h>
#include <lunaix/generic/bootmem.h>

#define boot_text __attribute__((section(".boot.text")))
#define boot_data __attribute__((section(".boot.data")))
#define boot_bss  __attribute__((section(".boot.bss")))

ptr_t
kremap();

#endif /* __LUNAIX_AA64_INIT_H */
