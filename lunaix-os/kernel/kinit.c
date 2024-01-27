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

#include <hal/acpi/acpi.h>
#include <hal/intc.h>

#include <sys/abi.h>
#include <sys/interrupts.h>
#include <sys/mm/mempart.h>

#include <klibc/strfmt.h>
#include <klibc/string.h>

extern void
__proc0(); /* proc0.c */

void
spawn_proc0();

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

    spawn_proc0();
}

/**
 * @brief 创建并运行proc0进程
 *
 */
void
spawn_proc0()
{
    struct proc_info* proc0 = alloc_process();

    /**
     * @brief
     * 注意：这里和视频中说的不一样，属于我之后的一点微调。
     * 在视频中，spawn_proc0是在_kernel_post_init的末尾才调用的。并且是直接跳转到_proc0
     *
     * 但是我后来发现，上述的方法会产生竞态条件。这是因为spawn_proc0被调用的时候，时钟中断已经开启，
     * 而中断的产生会打乱栈的布局，从而使得下面的上下文设置代码产生未定义行为（Undefined
     * Behaviour）。 为了保险起见，有两种办法：
     *      1. 在创建proc0进程前关闭中断
     *      2. 将_kernel_post_init搬进proc0进程
     * （_kernel_post_init已经更名为init_platform）
     *
     * 目前的解决方案是2
     */

    proc0->parent = proc0;

    // 方案1：必须在读取eflags之后禁用。否则当进程被调度时，中断依然是关闭的！
    // cpu_disable_interrupt();

    /* Ok... 首先fork进我们的零号进程，而后由那里，我们fork进init进程。 */

    // 把当前虚拟地址空间（内核）复制一份。
    proc0->page_table = vmm_dup_vmspace(proc0->pid);

    // 直接切换到新的拷贝，进行配置。
    cpu_chvmspace(proc0->page_table);

    // 为内核创建一个专属栈空间。
    for (size_t i = 0; i < KERNEL_STACK_SIZE; i += PG_SIZE) {
        ptr_t pa = pmm_alloc_page(0);
        vmm_set_mapping(VMS_SELF, KERNEL_STACK + i, pa, PG_PREM_RW, VMAP_NULL);
    }

    proc_init_transfer(proc0, KERNEL_STACK_END, (ptr_t)__proc0, 0);

    // 向调度器注册进程。
    commit_process(proc0);

    // 由于时钟中断与APIC未就绪，我们需要手动进行第一次调度。这里也会同时隐式地恢复我们的eflags.IF位
    proc0->state = PS_RUNNING;
    switch_context(proc0);

    /* Should not return */
    assert_msg(0, "Unexpected Return");
}

void
kmem_init(struct boot_handoff* bhctx)
{
    extern u8_t __kexec_end;
    // 将内核占据的页，包括前1MB，hhk_init 设为已占用
    size_t pg_count = ((ptr_t)&__kexec_end - KERNEL_EXEC) >> PG_SIZE_BITS;
    pmm_mark_chunk_occupied(0, pg_count, PP_FGLOCKED);

    // reserve higher half
    for (size_t i = L1_INDEX(KERNEL_EXEC); i < 1023; i++) {
        assert(vmm_set_mapping(VMS_SELF, i << 22, 0, 0, VMAP_NOMAP));
    }

    // allocators
    cake_init();
    valloc_init();
}
