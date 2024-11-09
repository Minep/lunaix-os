#include <lunaix/types.h>
#include <lunaix/block.h>
#include <lunaix/boot_generic.h>
#include <lunaix/device.h>
#include <lunaix/input.h>

#include <lunaix/mm/cake.h>
#include <lunaix/mm/pmm.h>
#include <lunaix/mm/page.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/mm/vmm.h>

#include <lunaix/process.h>
#include <lunaix/sched.h>
#include <lunaix/spike.h>
#include <lunaix/trace.h>
#include <lunaix/owloysius.h>
#include <lunaix/hart_state.h>
#include <lunaix/syslog.h>
#include <lunaix/sections.h>

#include <hal/devtree.h>

#include <asm/mm_defs.h>

LOG_MODULE("kinit")

extern void
lunad_main();

/**
 * @brief 创建并运行Lunaix守护进程
 *
 */
static void
spawn_lunad()
{
    int has_error;
    struct thread* kthread;
    
    has_error = spawn_process(&kthread, (ptr_t)lunad_main, false);
    assert_msg(!has_error, "failed to spawn lunad");

    run(kthread);
    
    fail("Unexpected Return");
}

static void
kmem_init(struct boot_handoff* bhctx)
{
    pte_t* ptep = mkptep_va(VMS_SELF, KERNEL_RESIDENT);

    ptep = mkl0tep(ptep);

    unsigned int i = ptep_vfn(ptep);
    do {
        if (lntep_implie_vmnts(ptep, L0T_SIZE)) {
            ptep++;
            continue;
        }

#if   LnT_ENABLED(1)
        assert(mkl1t(ptep++, 0, KERNEL_PGTAB));
#elif LnT_ENABLED(2)
        assert(mkl2t(ptep++, 0, KERNEL_PGTAB));
#elif LnT_ENABLED(3)
        assert(mkl3t(ptep++, 0, KERNEL_PGTAB));
#else
        assert(mklft(ptep++, 0, KERNEL_PGTAB));
#endif
    } while (++i < MAX_PTEN);

    // allocators
    cake_init();
    valloc_init();
}

static void
__remap_and_load_dtb(struct boot_handoff* bhctx)
{
#ifdef CONFIG_USE_DEVICETREE
    ptr_t dtb = bhctx->kexec.dtb_pa;

    if (!dtb) {
        return;
    }

    if (va_offset(dtb)) {
        WARN("bad-aligned dtb location, expect page aligned");
        return;
    }

    pte_t *ptep, pte;
    size_t nr_pages;
    bool loaded;
    
    pte  = mkpte(dtb, KERNEL_DATA);
    ptep = mkptep_va(VMS_SELF, dtb_start);
    nr_pages = leaf_count(CONFIG_DTB_MAXSIZE);

    pmm_onhold_range(dtb, nr_pages);
    vmm_set_ptes_contig(ptep, pte, PAGE_SIZE, nr_pages);

    loaded = dt_load(dtb_start);
    if (!loaded) {
        ERROR("dtb load failed");
    }
#endif

    return;
}

void
kernel_bootstrap(struct boot_handoff* bhctx)
{
    pmm_init(bhctx);
    // now we can start reserving physical space

    /* Begin kernel bootstrapping sequence */
    boot_begin(bhctx);

    /* Setup kernel memory layout and services */
    kmem_init(bhctx);


    boot_parse_cmdline(bhctx);

    /* Prepare stack trace environment */
    trace_modksyms_init(bhctx);

    device_scan_drivers();

    initfn_invoke_sysconf();
    
    __remap_and_load_dtb(bhctx);
    device_sysconf_load();

    // TODO register devtree hooks
    // TODO re-scan devtree to bind devices.

    clock_init();
    timer_init();

    initfn_invoke_earlyboot();

    vfs_init();
    fsm_init();
    input_init();
    block_init();
    sched_init();

    device_onboot_load();

    /* the bare metal are now happy, let's get software over with */

    must_success(vfs_mount_root("ramfs", NULL));
    must_success(vfs_mount("/dev", "devfs", NULL, 0));
    
    initfn_invoke_boot();

    /* Finish up bootstrapping sequence, we are ready to spawn the root process
     * and start geting into uspace
     */
    boot_end(bhctx);

    spawn_lunad();
}

