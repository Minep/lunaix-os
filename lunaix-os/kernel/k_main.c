#include <stdint.h>

#include <lunaix/constants.h>
#include <lunaix/tty/tty.h>

#include <lunaix/mm/page.h>
#include <lunaix/mm/pmm.h>
#include <lunaix/mm/vmm.h>

#include <hal/cpu.h>

#include <arch/x86/boot/multiboot.h>
#include <arch/x86/gdt.h>
#include <arch/x86/idt.h>

#include <libc/stdio.h>

extern void __kernel_start;
extern void __kernel_end;
extern void __init_hhk_end;

void
_kernel_init(multiboot_info_t* mb_info)
{
    _init_idt();

    multiboot_memory_map_t* map = (multiboot_memory_map_t*)mb_info->mmap_addr;

    // TODO: 内核初始化
    //   (v) 根据memory map初始化内存管理器
    //   (v) 分配新的栈空间
    //       调整映射：
    //   ( )    + 映射 memory map （APCI，APIC，IO映射） （以后）
    //   (v)    + 释放 hhk_init 所占据的空间

    // 初始化物理内存管理器
    pmm_init(MEM_1MB + mb_info->mem_upper << 10);
    vmm_init();
    tty_init(VGA_BUFFER_PADDR);

    tty_set_theme(VGA_COLOR_GREEN, VGA_COLOR_BLACK);

    printf("[KERNEL] Initializing Memory ...\n");
    unsigned int map_size =
      mb_info->mmap_length / sizeof(multiboot_memory_map_t);
    printf("[MM] Mem: %d KiB, Extended Mem: %d KiB\n",
           mb_info->mem_lower,
           mb_info->mem_upper);

    // 按照 Memory map 标识可用的物理页
    for (unsigned int i = 0; i < map_size; i++) {
        multiboot_memory_map_t mmap = map[i];
        printf("[MM] Base: 0x%x, len: %u KiB, type: %u\n",
               map[i].addr_low,
               map[i].len_low >> 10,
               map[i].type);
        if (mmap.type == MULTIBOOT_MEMORY_AVAILABLE) {
            // 整数向上取整除法
            uintptr_t pg = map[i].addr_low + 0x0fffU;
            pmm_mark_chunk_free(pg >> 12, map[i].len_low >> 12);
            printf("[MM] Freed %u pages start from 0x%x\n",
                   map[i].len_low >> 12,
                   pg & ~0x0fffU);
        }
    }

    // 将内核占据的页设为已占用
    size_t pg_count = (uintptr_t)(&__kernel_end - &__kernel_start) >> 12;
    pmm_mark_chunk_occupied(V2P(&__kernel_start) >> 12, pg_count);
    printf("[MM] Allocated %d pages for kernel.\n", pg_count);


    size_t hhk_init_pg_count = ((uintptr_t)(&__init_hhk_end)) >> 12;
    printf("[MM] Releaseing %d pages from 0x0.\n", hhk_init_pg_count);

    // 清除 hhk_init 与前1MiB的映射
    // 从这里开始，到新的vga缓存地址设置前，不能够进行任何的打印操作
    for (size_t i = 0; i < hhk_init_pg_count; i++) {
        vmm_unmap_page((i << 12));
    }

    size_t vga_buf_pgs = VGA_BUFFER_SIZE >> 12;
    
    // 首先，标记VGA部分为已占用
    pmm_mark_chunk_occupied(VGA_BUFFER_PADDR >> 12, vga_buf_pgs);
    
    // 重映射VGA文本缓冲区（以后会变成显存，i.e., framebuffer）
    for (size_t i = 0; i < vga_buf_pgs; i++)
    {
        vmm_map_page(VGA_BUFFER_VADDR + (i << 12), VGA_BUFFER_PADDR + (i << 12), PG_PREM_RW, PG_PREM_RW);
    }
    
    // 更新VGA缓冲区位置至虚拟地址
    tty_set_buffer(VGA_BUFFER_VADDR);

    printf("[MM] Mapped VGA to %p.\n", VGA_BUFFER_VADDR);

    // 为内核创建一个专属栈空间。
    for (size_t i = 0; i < (K_STACK_SIZE >> 12); i++) {
        vmm_alloc_page(K_STACK_START + (i << 12), PG_PREM_RW, PG_PREM_RW);
    }
    printf("[MM] Allocated %d pages for stack start at %p\n", K_STACK_SIZE>>12, K_STACK_START);

    printf("[KERNEL] Done!\n\n");
}

void
_kernel_main()
{
    char* buf[64];
    
    printf("Hello higher half kernel world!\nWe are now running in virtual "
           "address space!\n\n");
    
    cpu_get_brand(buf);
    printf("CPU: %s\n\n", buf);

    uintptr_t k_start = vmm_v2p(&__kernel_start);
    printf("The kernel's base address mapping: %p->%p\n", &__kernel_start, k_start);
    // __asm__("int $0\n");
}