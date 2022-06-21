#include <lunaix/common.h>
#include <lunaix/tty/tty.h>

#include <lunaix/clock.h>
#include <lunaix/mm/kalloc.h>
#include <lunaix/mm/page.h>
#include <lunaix/mm/pmm.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/process.h>
#include <lunaix/sched.h>
#include <lunaix/spike.h>
#include <lunaix/syslog.h>
#include <lunaix/timer.h>

#include <hal/rtc.h>

#include <arch/x86/boot/multiboot.h>
#include <arch/x86/idt.h>
#include <arch/x86/interrupts.h>

#include <klibc/stdio.h>
#include <klibc/string.h>

#include <stddef.h>
#include <stdint.h>

extern uint8_t __kernel_start;
extern uint8_t __kernel_end;
extern uint8_t __init_hhk_end;

#define PP_KERN_SHARED (PP_FGSHARED | PP_TKERN)

// Set remotely by kernel/asm/x86/prologue.S
multiboot_info_t* _k_init_mb_info;

x86_page_table* __kernel_ptd;

struct proc_info tmp;

LOG_MODULE("BOOT");

extern void
__proc0(); /* proc0.c */

void
spawn_proc0();

void
setup_memory(multiboot_memory_map_t* map, size_t map_size);

void
_kernel_pre_init()
{
    _init_idt();
    intr_routine_init();

    pmm_init(MEM_1MB + (_k_init_mb_info->mem_upper << 10));
    vmm_init();
    rtc_init();

    tty_init((void*)VGA_BUFFER_PADDR);
    tty_set_theme(VGA_COLOR_WHITE, VGA_COLOR_BLACK);

    __kernel_ptd = cpu_rcr3();

    tmp = (struct proc_info){ .page_table = __kernel_ptd };

    __current = &tmp;
}

void
_kernel_init()
{
    kprintf("[MM] Mem: %d KiB, Extended Mem: %d KiB\n",
            _k_init_mb_info->mem_lower,
            _k_init_mb_info->mem_upper);

    unsigned int map_size =
      _k_init_mb_info->mmap_length / sizeof(multiboot_memory_map_t);

    setup_memory((multiboot_memory_map_t*)_k_init_mb_info->mmap_addr, map_size);

    kprintf(KINFO "[MM] Allocated %d pages for stack start at %p\n",
            KSTACK_SIZE >> PG_SIZE_BITS,
            KSTACK_START);

    sched_init();

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
        kprintf("[MM] Base: 0x%x, len: %u KiB, type: %u\n",
                map[i].addr_low,
                map[i].len_low >> 10,
                map[i].type);
        if (mmap.type == MULTIBOOT_MEMORY_AVAILABLE) {
            // 整数向上取整除法
            uintptr_t pg = map[i].addr_low + 0x0fffU;
            pmm_mark_chunk_free(pg >> PG_SIZE_BITS,
                                map[i].len_low >> PG_SIZE_BITS);
            kprintf(KINFO "[MM] Freed %u pages start from 0x%x\n",
                    map[i].len_low >> PG_SIZE_BITS,
                    pg & ~0x0fffU);
        }
    }

    // 将内核占据的页，包括前1MB，hhk_init 设为已占用
    size_t pg_count = V2P(&__kernel_end) >> PG_SIZE_BITS;
    pmm_mark_chunk_occupied(KERNEL_PID, 0, pg_count, 0);
    kprintf(KINFO "[MM] Allocated %d pages for kernel.\n", pg_count);

    size_t vga_buf_pgs = VGA_BUFFER_SIZE >> PG_SIZE_BITS;

    // 首先，标记VGA部分为已占用
    pmm_mark_chunk_occupied(
      KERNEL_PID, VGA_BUFFER_PADDR >> PG_SIZE_BITS, vga_buf_pgs, 0);

    // 重映射VGA文本缓冲区（以后会变成显存，i.e., framebuffer）
    for (size_t i = 0; i < vga_buf_pgs; i++) {
        vmm_set_mapping(PD_REFERENCED,
                        VGA_BUFFER_VADDR + (i << PG_SIZE_BITS),
                        VGA_BUFFER_PADDR + (i << PG_SIZE_BITS),
                        PG_PREM_URW,
                        VMAP_NULL);
    }

    assert_msg(!((uintptr_t)&__usrtext_start & 0xfff) &&
                 !((uintptr_t)&__usrtext_end & 0xfff),
               "Bad usrtext alignment");

    for (uintptr_t i = &__usrtext_start; i < &__usrtext_end; i += PG_SIZE) {
        vmm_set_mapping(PD_REFERENCED, i, V2P(i), PG_PREM_UR, VMAP_NULL);
    }

    // 更新VGA缓冲区位置至虚拟地址
    tty_set_buffer((void*)VGA_BUFFER_VADDR);

    kprintf(KINFO "[MM] Mapped VGA to %p.\n", VGA_BUFFER_VADDR);
}
