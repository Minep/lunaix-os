#include <lunaix/exebi/elf.h>
#include <lunaix/load.h>
#include <lunaix/mm/mmap.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/spike.h>

#include <asm/mempart.h>

int
elf_smap(struct load_context* ldctx,
           const struct elf* elf,
           struct elf_phdr* phdre,
           uintptr_t base_va)
{
    struct v_file* elfile = (struct v_file*)elf->elf_file;

    assert(!va_offset(phdre->p_offset));

    int proct = 0;
    if ((phdre->p_flags & PF_R)) {
        proct |= PROT_READ;
    }
    if ((phdre->p_flags & PF_W)) {
        proct |= PROT_WRITE;
    }
    if ((phdre->p_flags & PF_X)) {
        proct |= PROT_EXEC;
    }

    uintptr_t va = phdre->p_va + base_va;
    struct exec_host* container = ldctx->container;
    struct mmap_param param = { .vms_mnt = container->vms_mnt,
                                .pvms = vmspace(container->proc),
                                .proct = proct,
                                .offset = page_aligned(phdre->p_offset),
                                .mlen = page_upaligned(phdre->p_memsz),
                                .flen = phdre->p_filesz,
                                .flags = MAP_FIXED | MAP_PRIVATE,
                                .type = REGION_TYPE_CODE };

    struct mm_region* seg_reg;
    int status = mmap_user(NULL, &seg_reg, page_aligned(va), elfile, &param);

    if (!status) {
        size_t next_addr = phdre->p_memsz + va;
        ldctx->end = MAX(ldctx->end, page_upaligned(next_addr));
        ldctx->mem_sz += phdre->p_memsz;
    } else {
        // we probably fucked up our process
        terminate_current(-1);
    }

    return status;
}

int
load_executable(struct load_context* context, const struct v_file* exefile)
{
    int errno = 0;

    char* ldpath = NULL;
    struct elf elf;
    struct exec_host* container = context->container;

    if ((errno = elf_openat(&elf, exefile))) {
        goto done;
    }

    if (!(elf_check_exec(&elf, ET_EXEC) || elf_check_exec(&elf, ET_DYN))) {
        errno = ENOEXEC;
        goto done;
    }

    ldpath = valloc(256);
    errno = elf_find_loader(&elf, ldpath, 256);
    uintptr_t load_base = 0;

    if (errno < 0) {
        goto done;
    }

    if (errno != NO_LOADER) {
        // close old elf
        if ((errno = elf_close(&elf))) {
            goto done;
        }

        // open the loader instead
        if ((errno = elf_open(&elf, ldpath))) {
            goto done;
        }

        // Is this the valid loader?
        if (!elf_static_linked(&elf) || !elf_check_exec(&elf, ET_DYN)) {
            errno = ELIBBAD;
            goto done_close_elf32;
        }

        load_base = USR_MMAP;
    }

    context->entry = elf.eheader.e_entry + load_base;

    struct v_file* elfile = (struct v_file*)elf.elf_file;

    for (size_t i = 0; i < elf.eheader.e_phnum && !errno; i++) {
        struct elf_phdr* phdr = &elf.pheaders[i];

        if (phdr->p_type != PT_LOAD) {
            continue;
        }

        if (phdr->p_align != PAGE_SIZE) {
            // surprising alignment!
            errno = ENOEXEC;
            break;
        }

        errno = elf_smap(context, &elf, phdr, load_base);
    }

done_close_elf32:
    elf_close(&elf);

done:
    vfree_safe(ldpath);
    return errno;
}
