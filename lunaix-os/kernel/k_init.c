#include <lunaix/common.h>
#include <lunaix/tty/tty.h>

#include <lunaix/mm/page.h>
#include <lunaix/mm/pmm.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/mm/kalloc.h>
#include <lunaix/spike.h>
#include <lunaix/syslog.h>
#include <lunaix/timer.h>
#include <lunaix/clock.h>
#include <lunaix/peripheral/ps2kbd.h>
#include <lunaix/process.h>
#include <lunaix/sched.h>
#include <lunaix/syscall.h>

#include <hal/rtc.h>
#include <hal/apic.h>
#include <hal/ioapic.h>
#include <hal/acpi/acpi.h>

#include <arch/x86/boot/multiboot.h>
#include <arch/x86/idt.h>
#include <arch/x86/interrupts.h>

#include <klibc/stdio.h>
#include <klibc/string.h>

#include <stdint.h>
#include <stddef.h>


extern uint8_t __kernel_start;
extern uint8_t __kernel_end;
extern uint8_t __init_hhk_end;

#define PP_KERN_SHARED (PP_FGSHARED | PP_TKERN)

// Set remotely by kernel/asm/x86/prologue.S
multiboot_info_t* _k_init_mb_info;

x86_page_table* __kernel_ptd;

struct proc_info tmp;

LOG_MODULE("BOOT");

extern void _lxinit_main();
void spawn_lxinit();
void _kernel_post_init();

void
setup_memory(multiboot_memory_map_t* map, size_t map_size);

void
lock_reserved_memory();

void
_kernel_pre_init() {
    _init_idt();
    intr_routine_init();

    pmm_init(MEM_1MB + (_k_init_mb_info->mem_upper << 10));
    vmm_init();
    rtc_init();

    tty_init((void*)VGA_BUFFER_PADDR);
    tty_set_theme(VGA_COLOR_WHITE, VGA_COLOR_BLACK);

    __kernel_ptd = cpu_rcr3();

    tmp = (struct proc_info) {
        .page_table = __kernel_ptd
    };

    __current = &tmp;
}

void
_kernel_init() {
    kprintf("[MM] Mem: %d KiB, Extended Mem: %d KiB\n",
           _k_init_mb_info->mem_lower,
           _k_init_mb_info->mem_upper);

    unsigned int map_size = _k_init_mb_info->mmap_length / sizeof(multiboot_memory_map_t);
    
    setup_memory((multiboot_memory_map_t*)_k_init_mb_info->mmap_addr, map_size);

    // 为内核创建一个专属栈空间。
    for (size_t i = 0; i < (KSTACK_SIZE >> PG_SIZE_BITS); i++) {
        vmm_alloc_page(KERNEL_PID, (void*)(KSTACK_START + (i << PG_SIZE_BITS)), NULL, PG_PREM_RW, 0);
    }
    kprintf(KINFO "[MM] Allocated %d pages for stack start at %p\n", KSTACK_SIZE>>PG_SIZE_BITS, KSTACK_START);

    sched_init();
}

/**
 * @brief 创建并运行init进程
 * 
 */
void spawn_lxinit() {
    struct proc_info kinit;

    memset(&kinit, 0, sizeof(kinit));
    kinit.parent = (void*)0;
    kinit.pid = 1;
    kinit.intr_ctx = (isr_param) {
        .registers.esp = KSTACK_TOP - 20,
        .cs = KCODE_SEG,
        .eip = (void*)_lxinit_main,
        .ss = KDATA_SEG,
        .eflags = cpu_reflags()
    };

    setup_proc_mem(&kinit, PD_REFERENCED);

    // Ok... 准备fork进我们的init进程
    /*
        这里是一些栈的设置，因为我们将切换到一个新的地址空间里，并且使用一个全新的栈。
        让iret满意！
    */
    asm volatile(
        "movl %%cr3, %%eax\n"
        "movl %%esp, %%ebx\n"
        "movl %0, %%cr3\n"
        "movl %1, %%esp\n"
        "pushf\n"
        "pushl %2\n"
        "pushl %3\n"
        "pushl $0\n"
        "pushl $0\n"
        "movl %%eax, %%cr3\n"
        "movl %%ebx, %%esp\n"
        ::"r"(kinit.page_table), "i"(KSTACK_TOP), "i"(KCODE_SEG), "r"(kinit.intr_ctx.eip)
        :"%eax", "%ebx", "memory"
    );

    // 向调度器注册进程，然后这里阻塞等待调度器调度就好了。
    push_process(&kinit);

}

void 
_kernel_post_init() {
    assert_msg(kalloc_init(), "Fail to initialize heap");
    
    size_t hhk_init_pg_count = ((uintptr_t)(&__init_hhk_end)) >> PG_SIZE_BITS;
    kprintf(KINFO "[MM] Releaseing %d pages from 0x0.\n", hhk_init_pg_count);

    // Fuck it, I will no longer bother this little 1MiB
    // I just release 4 pages for my APIC & IOAPIC remappings
    for (size_t i = 0; i < 3; i++) {
        vmm_unmap_page(KERNEL_PID, (void*)(i << PG_SIZE_BITS));
    }
    
    // 锁定所有系统预留页（内存映射IO，ACPI之类的），并且进行1:1映射
    lock_reserved_memory();

    acpi_init(_k_init_mb_info);
    uintptr_t ioapic_addr = acpi_get_context()->madt.ioapic->ioapic_addr;
    pmm_mark_page_occupied(KERNEL_PID, FLOOR(__APIC_BASE_PADDR, PG_SIZE_BITS), 0);
    pmm_mark_page_occupied(KERNEL_PID, FLOOR(ioapic_addr, PG_SIZE_BITS), 0);

    vmm_set_mapping(KERNEL_PID, APIC_BASE_VADDR, __APIC_BASE_PADDR, PG_PREM_RW);
    vmm_set_mapping(KERNEL_PID, IOAPIC_BASE_VADDR, ioapic_addr, PG_PREM_RW);

    apic_init();
    ioapic_init();
    timer_init(SYS_TIMER_FREQUENCY_HZ);
    clock_init();
    ps2_kbd_init();

    syscall_install();

    for (size_t i = 256; i < hhk_init_pg_count; i++) {
        vmm_unmap_page(KERNEL_PID, (void*)(i << PG_SIZE_BITS));
    }

    spawn_lxinit();

    spin();
}

void
lock_reserved_memory() {
    multiboot_memory_map_t* mmaps = _k_init_mb_info->mmap_addr;
    size_t map_size = _k_init_mb_info->mmap_length / sizeof(multiboot_memory_map_t);
    for (unsigned int i = 0; i < map_size; i++) {
        multiboot_memory_map_t mmap = mmaps[i];
        if (mmap.type == MULTIBOOT_MEMORY_AVAILABLE) {
            continue;
        }
        uint8_t* pa = PG_ALIGN(mmap.addr_low);
        size_t pg_num = CEIL(mmap.len_low, PG_SIZE_BITS);
        for (size_t j = 0; j < pg_num; j++)
        {
            vmm_set_mapping(KERNEL_PID, (pa + (j << PG_SIZE_BITS)), (pa + (j << PG_SIZE_BITS)), PG_PREM_R);
        }
    }
}

// 按照 Memory map 标识可用的物理页
void
setup_memory(multiboot_memory_map_t* map, size_t map_size) {

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
            pmm_mark_chunk_free(pg >> PG_SIZE_BITS, map[i].len_low >> PG_SIZE_BITS);
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
    pmm_mark_chunk_occupied(KERNEL_PID, VGA_BUFFER_PADDR >> PG_SIZE_BITS, vga_buf_pgs, 0);
    
    // 重映射VGA文本缓冲区（以后会变成显存，i.e., framebuffer）
    for (size_t i = 0; i < vga_buf_pgs; i++)
    {
        vmm_map_page(
            KERNEL_PID,
            (void*)(VGA_BUFFER_VADDR + (i << PG_SIZE_BITS)), 
            (void*)(VGA_BUFFER_PADDR + (i << PG_SIZE_BITS)), 
            PG_PREM_URW
        );
    }
    
    // 更新VGA缓冲区位置至虚拟地址
    tty_set_buffer((void*)VGA_BUFFER_VADDR);

    kprintf(KINFO "[MM] Mapped VGA to %p.\n", VGA_BUFFER_VADDR);
}