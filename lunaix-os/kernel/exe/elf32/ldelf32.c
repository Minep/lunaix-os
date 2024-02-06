#include <lunaix/exebi/elf32.h>
#include <lunaix/load.h>
#include <lunaix/mm/mmap.h>
#include <lunaix/mm/page.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/spike.h>

#include <sys/mm/mempart.h>

int
elf32_smap(struct load_context* ldctx,
           const struct elf32* elf,
           struct elf32_phdr* phdre,
           uintptr_t base_va)
{
    struct v_file* elfile = (struct v_file*)elf->elf_file;

    assert(PG_ALIGNED(phdre->p_offset));

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
    struct exec_container* container = ldctx->container;
    struct mmap_param param = { .vms_mnt = container->vms_mnt,
                                .pvms = vmspace(container->proc),
                                .proct = proct,
                                .offset = PG_ALIGN(phdre->p_offset),
                                .mlen = ROUNDUP(phdre->p_memsz, PG_SIZE),
                                .flags = MAP_FIXED | MAP_PRIVATE,
                                .type = REGION_TYPE_CODE };

    struct mm_region* seg_reg;
    int status = mmap_user(NULL, &seg_reg, PG_ALIGN(va), elfile, &param);

    if (!status) {
        size_t next_addr = phdre->p_memsz + va;
        ldctx->end = MAX(ldctx->end, ROUNDUP(next_addr, PG_SIZE));
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
    struct elf32 elf;
    struct exec_container* container = context->container;

    if ((errno = elf32_openat(&elf, exefile))) {
        goto done;
    }

    if (!elf32_check_arch(&elf)) {
        errno = EINVAL;
        goto done;
    }

    if (!(elf32_check_exec(&elf, ET_EXEC) || elf32_check_exec(&elf, ET_DYN))) {
        errno = ENOEXEC;
        goto done;
    }

    ldpath = valloc(256);
    errno = elf32_find_loader(&elf, ldpath, 256);
    uintptr_t load_base = 0;

    if (errno < 0) {
        goto done;
    }

    if (errno != NO_LOADER) {
        container->argv_pp[1] = ldpath;

        // close old elf
        if ((errno = elf32_close(&elf))) {
            goto done;
        }

        // open the loader instead
        if ((errno = elf32_open(&elf, ldpath))) {
            goto done;
        }

        // Is this the valid loader?
        if (!elf32_static_linked(&elf) || !elf32_check_exec(&elf, ET_DYN)) {
            errno = ELIBBAD;
            goto done_close_elf32;
        }

        load_base = USR_MMAP;
    }

    context->entry = elf.eheader.e_entry + load_base;

    struct v_file* elfile = (struct v_file*)elf.elf_file;

    for (size_t i = 0; i < elf.eheader.e_phnum && !errno; i++) {
        struct elf32_phdr* phdr = &elf.pheaders[i];

        if (phdr->p_type != PT_LOAD) {
            continue;
        }

        if (phdr->p_align != PG_SIZE) {
            // surprising alignment!
            errno = ENOEXEC;
            break;
        }

        errno = elf32_smap(context, &elf, phdr, load_base);
    }

done_close_elf32:
    elf32_close(&elf);

done:
    if (!container->argv_pp[1]) {
        vfree_safe(ldpath);
    }
    return errno;
}
