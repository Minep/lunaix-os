#ifndef LUNAIX_MULTIBOOT_H
#define LUNAIX_MULTIBOOT_H 1

#ifdef CONFIG_X86_BL_MB
#   include "mb.h"
#   define MULTIBOOT_MAGIC 0x1BADB002
#elif CONFIG_X86_BL_MB2
#   include "mb2.h"
#   define MULTIBOOT_MAGIC 0xe85250d6
#else
#error "Multiboot interfacing is not enabled"
#endif

#endif