#include <lunaix/types.h>
#include <lunaix/block.h>
#include <lunaix/boot_generic.h>
#include <lunaix/device.h>
#include <lunaix/foptions.h>
#include <lunaix/fs/twifs.h>
#include <lunaix/input.h>
#include <lunaix/mm/cake.h>
#include <lunaix/mm/mmio.h>
#include <lunaix/mm/page.h>
#include <lunaix/mm/pmm.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/process.h>
#include <lunaix/sched.h>
#include <lunaix/spike.h>
#include <lunaix/trace.h>
#include <lunaix/tty/tty.h>
#include <lunaix/owloysius.h>
#include <lunaix/pcontext.h>

#include <hal/acpi/acpi.h>
#include <hal/intc.h>

#include <sys/abi.h>
#include <sys/mm/mm_defs.h>

#include <klibc/strfmt.h>
#include <klibc/string.h>

void
spawn_lunad();

void
kmem_init(struct boot_handoff* bhctx);

void
kernel_bootstrap(struct boot_handoff* bhctx)
{
    pmm_init(bhctx->mem.size);
    vmm_init();

    /* Begin kernel bootstrapping sequence */
    boot_begin(bhctx);

    /* Setup kernel memory layout and services */
    kmem_init(bhctx);

    boot_parse_cmdline(bhctx);

    /* Prepare stack trace environment */
    trace_modksyms_init(bhctx);

    device_scan_drivers();

    invoke_init_function(on_earlyboot);

    // FIXME this goes to hal/gfxa
    tty_init(ioremap(0xB8000, PG_SIZE));
    tty_set_theme(VGA_COLOR_WHITE, VGA_COLOR_BLACK);

    device_sysconf_load();

    /* Get intc online, this is the cornerstone when initing devices */
    intc_init();

    clock_init();
    timer_init();

    /*
        TODO autoload these init function that do not have dependency between
       them
    */

    /* Let's get fs online as soon as possible, as things rely on them */
    vfs_init();
    fsm_init();
    input_init();
    block_init();
    sched_init();

    device_onboot_load();

    /* the bare metal are now happy, let's get software over with */

    must_success(vfs_mount_root("ramfs", NULL));
    must_success(vfs_mount("/dev", "devfs", NULL, 0));
    
    invoke_init_function(on_boot);

    must_success(vfs_unmount("/dev"));

    /* Finish up bootstrapping sequence, we are ready to spawn the root process
     * and start geting into uspace
     */
    boot_end(bhctx);
    boot_cleanup();

    spawn_lunad();
}

extern void
lunad_main();

/**
 * @brief 创建并运行Lunaix守护进程
 *
 */
void
spawn_lunad()
{
    int has_error;
    struct thread* kthread;
    
    has_error = spawn_process(&kthread, (ptr_t)lunad_main, false);
    assert_msg(!has_error, "failed to spawn lunad");

    run(kthread);
    
    fail("Unexpected Return");
}

void
kmem_init(struct boot_handoff* bhctx)
{
    extern u8_t __kexec_end;
    // 将内核占据的页，包括前1MB，hhk_init 设为已占用
    size_t pg_count = leaf_count((ptr_t)&__kexec_end - KERNEL_RESIDENT);
    pmm_mark_chunk_occupied(0, pg_count, PP_FGLOCKED);

    pte_t* ptep = mkptep_va(VMS_SELF, KERNEL_RESIDENT);
    ptep = mkl0tep(ptep);

    do {
#if   LnT_ENABLED(1)
        assert(mkl1t(ptep++, 0, KERNEL_DATA));
#elif LnT_ENABLED(2)
        assert(mkl2t(ptep++, 0, KERNEL_DATA));
#elif LnT_ENABLED(3)
        assert(mkl3t(ptep++, 0, KERNEL_DATA));
#else
        assert(mklft(ptep++, 0, KERNEL_DATA));
#endif
    } while (ptep_vfn(ptep) < MAX_PTEN - 2);

    // allocators
    cake_init();
    valloc_init();
}
