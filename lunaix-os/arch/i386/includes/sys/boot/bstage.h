#ifndef __LUNAIX_BSTAGE_H
#define __LUNAIX_BSTAGE_H

#define boot_text __attribute__((section(".boot.text")))
#define boot_data __attribute__((section(".boot.data")))
#define boot_bss __attribute__((section(".boot.bss")))

#endif /* __LUNAIX_BSTAGE_H */
