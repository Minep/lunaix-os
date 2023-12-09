#define __BOOT_CODE__

#include <lunaix/boot_generic.h>
#include <sys/boot/bstage.h>
#include <sys/boot/multiboot.h>
#include <sys/mm/mempart.h>

#define BHCTX_ALLOC 4096

u8_t bhctx_buffer[BHCTX_ALLOC] boot_bss;

#define check_buffer(ptr)                                                      \
    if ((ptr) >= ((ptr_t)bhctx_buffer + BHCTX_ALLOC)) {                        \
        asm("ud2");                                                            \
    }

size_t boot_text
mb_memcpy(u8_t* destination, u8_t* base, unsigned int size)
{
    unsigned int i = 0;
    for (; i < size; i++) {
        *(destination + i) = *(base + i);
    }
    return i;
}

size_t boot_text
mb_strcpy(char* destination, char* base)
{
    int i = 0;
    char c = 0;
    while ((c = base[i])) {
        destination[i] = c;
        i++;
    }

    destination[++i] = 0;

    return i;
}

size_t boot_text
mb_strlen(char* s)
{
    int i = 0;
    while (s[i++])
        ;
    return i;
}

size_t boot_text
mb_parse_cmdline(struct boot_handoff* bhctx, void* buffer, char* cmdline)
{
#define SPACE ' '

    size_t slen = mb_strlen(cmdline);

    if (!slen) {
        return 0;
    }

    mb_memcpy(buffer, (u8_t*)cmdline, slen);
    bhctx->kexec.len = slen;

    return slen;
}

size_t boot_text
mb_parse_mmap(struct boot_handoff* bhctx,
              struct multiboot_info* mb,
              void* buffer)
{
    struct multiboot_mmap_entry* mb_mmap =
      (struct multiboot_mmap_entry*)mb->mmap_addr;
    size_t mmap_len = mb->mmap_length / sizeof(struct multiboot_mmap_entry);

    struct boot_mmapent* bmmap = (struct boot_mmapent*)buffer;
    for (size_t i = 0; i < mmap_len; i++) {
        struct boot_mmapent* bmmapent = &bmmap[i];
        struct multiboot_mmap_entry* mb_mapent = &mb_mmap[i];

        if (mb_mapent->type == MULTIBOOT_MEMORY_AVAILABLE) {
            bmmapent->type = BOOT_MMAP_FREE;
        } else if (mb_mapent->type == MULTIBOOT_MEMORY_ACPI_RECLAIMABLE) {
            bmmapent->type = BOOT_MMAP_RCLM;
        } else {
            bmmapent->type = BOOT_MMAP_RSVD;
        }

        bmmapent->start = mb_mapent->addr_low;
        bmmapent->size = mb_mapent->len_low;
    }

    bhctx->mem.size = (mb->mem_upper << 10) + MEM_1M;
    bhctx->mem.mmap = bmmap;
    bhctx->mem.mmap_len = mmap_len;

    return mmap_len * sizeof(struct boot_mmapent);
}

size_t boot_text
mb_parse_mods(struct boot_handoff* bhctx,
              struct multiboot_info* mb,
              void* buffer)
{
    if (!mb->mods_count) {
        bhctx->mods.mods_num = 0;
        return 0;
    }

    struct boot_modent* modents = (struct boot_modent*)buffer;
    struct multiboot_mod_list* mods = (struct multiboot_mod_list*)mb->mods_addr;

    ptr_t mod_str_ptr = (ptr_t)&modents[mb->mods_count];

    for (size_t i = 0; i < mb->mods_count; i++) {
        struct multiboot_mod_list* mod = &mods[i];
        modents[i] = (struct boot_modent){ .start = mod->mod_start,
                                           .end = mod->mod_end,
                                           .str = (char*)mod_str_ptr };
        mod_str_ptr += mb_strcpy((char*)mod_str_ptr, (char*)mod->cmdline);
    }

    bhctx->mods.mods_num = mb->mods_count;
    bhctx->mods.entries = modents;

    return mod_str_ptr - (ptr_t)buffer;
}

void boot_text
mb_prepare_hook(struct boot_handoff* bhctx)
{
    // nothing to do
}

void boot_text
mb_release_hook(struct boot_handoff* bhctx)
{
    // nothing to do
}

#define align_addr(addr) (((addr) + (sizeof(ptr_t) - 1)) & ~(sizeof(ptr_t) - 1))

struct boot_handoff* boot_text
mb_parse(struct multiboot_info* mb)
{
    struct boot_handoff* bhctx = (struct boot_handoff*)bhctx_buffer;
    ptr_t bhctx_ex = (ptr_t)&bhctx[1];
    
    *bhctx = (struct boot_handoff){ };

    /* Parse memory map */
    if ((mb->flags & MULTIBOOT_INFO_MEM_MAP)) {
        bhctx_ex += mb_parse_mmap(bhctx, mb, (void*)bhctx_ex);
        bhctx_ex = align_addr(bhctx_ex);
    }

    /* Parse cmdline */
    if ((mb->flags & MULTIBOOT_INFO_CMDLINE)) {
        bhctx_ex +=
          mb_parse_cmdline(bhctx, (void*)bhctx_ex, (char*)mb->cmdline);
        bhctx_ex = align_addr(bhctx_ex);
    }

    /* Parse sys modules */
    if ((mb->flags & MULTIBOOT_INFO_MODS)) {
        bhctx_ex += mb_parse_mods(bhctx, mb, (void*)bhctx_ex);
        bhctx_ex = align_addr(bhctx_ex);
    }

    check_buffer(bhctx_ex);

    bhctx->prepare = mb_prepare_hook;
    bhctx->release = mb_release_hook;

    return bhctx;
}