#ifndef __LUNAIX_ARCHINIT_H
#define __LUNAIX_ARCHINIT_H

#include <lunaix/types.h>
#include <sys/boot/bstage.h>
#include <sys/boot/multiboot.h>

ptr_t boot_text
kpg_init();

struct boot_handoff* boot_text
mb_parse(struct multiboot_info* mb);

#endif /* __LUNAIX_ARCHINIT_H */
