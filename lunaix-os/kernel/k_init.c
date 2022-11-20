#include <lunaix/common.h>
#include <lunaix/tty/tty.h>

#include <lunaix/device.h>
#include <lunaix/foptions.h>
#include <lunaix/input.h>
#include <lunaix/isrm.h>
#include <lunaix/lxconsole.h>
#include <lunaix/mm/mmio.h>
#include <lunaix/mm/page.h>
#include <lunaix/mm/pmm.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/process.h>
#include <lunaix/sched.h>
#include <lunaix/spike.h>
#include <lunaix/types.h>

#include <arch/x86/boot/multiboot.h>
#include <arch/x86/idt.h>
#include <arch/x86/interrupts.h>

#include <klibc/stdio.h>
#include <klibc/string.h>

extern uint8_t __kernel_start;
extern uint8_t __kernel_end;
extern uint8_t __init_hhk_end;

#define PP_KERN_SHARED (PP_FGSHARED | PP_TKERN)

// Set remotely by kernel/asm/x86/prologue.S
multiboot_info_t* _k_init_mb_info;

x86_page_table* __kernel_ptd;

extern void
__proc0(); /* proc0.c */

void
spawn_proc0();

void
setup_memory(multiboot_memory_map_t* map, size_t map_size);

void
_kernel_pre_init()
{
    // interrupts
    _init_idt();
    isrm_init();
    intr_routine_init();

    // memory
    pmm_init(MEM_1MB + (_k_init_mb_info->mem_upper << 10));
    vmm_init();

    unsigned int map_size =
      _k_init_mb_info->mmap_length / sizeof(multiboot_memory_map_t);

    setup_memory((multiboot_memory_map_t*)_k_init_mb_info->mmap_addr, map_size);
}

void
_kernel_init()
{
    int errno = 0;

    // allocators
    cake_init();
    valloc_init();

    sched_init();

    // crt
    tty_init(ioremap(VGA_FRAMEBUFFER, PG_SIZE));
    tty_set_theme(VGA_COLOR_WHITE, VGA_COLOR_BLACK);

    // file system & device subsys
    vfs_init();
    fsm_init();
    input_init();

    vfs_export_attributes();

    lxconsole_init();

    if ((errno = vfs_mount_root("ramfs", NULL))) {
        panickf("Fail to mount root. (errno=%d)", errno);
    }

    vfs_mount("/dev", "devfs", NULL, 0);
    vfs_mount("/sys", "twifs", NULL, MNT_RO);
    vfs_mount("/task", "taskfs", NULL, MNT_RO);

    lxconsole_spawn_ttydev();
    device_init_builtin();

    syscall_install();

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

    proc0->intr_ctx = (isr_param){ .registers = { .ds = KDATA_SEG,
                                                  .es = KDATA_SEG,
                                                  .fs = KDATA_SEG,
                                                  .gs = KDATA_SEG },
                                   .cs = KCODE_SEG,
                                   .eip = (void*)__proc0,
                                   .ss = KDATA_SEG,
                                   .eflags = cpu_reflags() };
    proc0->parent = proc0;

    // 方案1：必须在读取eflags之后禁用。否则当进程被调度时，中断依然是关闭的！
    // cpu_disable_interrupt();

    /* Ok... 首先fork进我们的零号进程，而后由那里，我们fork进init进程。 */

    // 把当前虚拟地址空间（内核）复制一份。
    proc0->page_table = vmm_dup_vmspace(proc0->pid);

    // 直接切换到新的拷贝，进行配置。
    cpu_lcr3(proc0->page_table);

    // 为内核创建一个专属栈空间。
    for (size_t i = 0; i < (KSTACK_SIZE >> PG_SIZE_BITS); i++) {
        uintptr_t pa = pmm_alloc_page(KERNEL_PID, 0);
        vmm_set_mapping(PD_REFERENCED,
                        KSTACK_START + (i << PG_SIZE_BITS),
                        pa,
                        PG_PREM_RW,
                        VMAP_NULL);
    }

    // 手动设置进程上下文：用于第一次调度
    asm volatile("movl %%esp, %%ebx\n"
                 "movl %1, %%esp\n"
                 "pushf\n"
                 "pushl %2\n"
                 "pushl %3\n"
                 "pushl $0\n"
                 "pushl $0\n"
                 "movl %%esp, %0\n"
                 "movl %%ebx, %%esp\n"
                 : "=m"(proc0->intr_ctx.registers.esp)
                 : "i"(KSTACK_TOP), "i"(KCODE_SEG), "r"(proc0->intr_ctx.eip)
                 : "%ebx", "memory");

    // 加载x87默认配置
    asm volatile("fninit\n"
                 "fxsave (%%eax)" ::"a"(proc0->fxstate)
                 : "memory");

    // 向调度器注册进程。
    commit_process(proc0);

    // 由于时钟中断与APIC未就绪，我们需要手动进行第一次调度。这里也会同时隐式地恢复我们的eflags.IF位
    proc0->state = PS_RUNNING;
    asm volatile("pushl %0\n"
                 "jmp switch_to\n" ::"r"(proc0));

    /* Should not return */
    assert_msg(0, "Unexpected Return");
}

extern void __usrtext_start;
extern void __usrtext_end;

// 按照 Memory map 标识可用的物理页
void
setup_memory(multiboot_memory_map_t* map, size_t map_size)
{

    // First pass, to mark the physical pages
    for (unsigned int i = 0; i < map_size; i++) {
        multiboot_memory_map_t mmap = map[i];
        if (mmap.type == MULTIBOOT_MEMORY_AVAILABLE) {
            // 整数向上取整除法
            uintptr_t pg = map[i].addr_low + 0x0fffU;
            pmm_mark_chunk_free(pg >> PG_SIZE_BITS,
                                map[i].len_low >> PG_SIZE_BITS);
        }
    }

    // 将内核占据的页，包括前1MB，hhk_init 设为已占用
    size_t pg_count = V2P(&__kernel_end) >> PG_SIZE_BITS;
    pmm_mark_chunk_occupied(KERNEL_PID, 0, pg_count, PP_FGLOCKED);

    for (uintptr_t i = &__usrtext_start; i < &__usrtext_end; i += PG_SIZE) {
        vmm_set_mapping(PD_REFERENCED, i, V2P(i), PG_PREM_UR, VMAP_NULL);
    }

    // reserve higher half
    for (size_t i = L1_INDEX(KERNEL_MM_BASE); i < 1023; i++) {
        assert(vmm_set_mapping(PD_REFERENCED, i << 22, 0, 0, VMAP_NOMAP));
    }
}
