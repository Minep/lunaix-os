#include <lunaix/constants.h>
#include <lunaix/tty/tty.h>

#include <lunaix/mm/page.h>
#include <lunaix/mm/pmm.h>
#include <lunaix/mm/vmm.h>

#include <arch/x86/boot/multiboot.h>

#include <stdint.h>
#include <stddef.h>


extern uint8_t __kernel_start;
extern uint8_t __kernel_end;
extern uint8_t __init_hhk_end;

void
_kernel_pre_init(multiboot_info_t* mb_info) {
    _init_idt();

    pmm_init(MEM_1MB + (mb_info->mem_upper << 10));
    vmm_init();

    tty_init((void*)VGA_BUFFER_PADDR);
    tty_set_theme(VGA_COLOR_GREEN, VGA_COLOR_BLACK);
}

void
_kernel_init(multiboot_info_t* mb_info) {
    printf("[KERNEL] === Initialization === \n");

    printf("[MM] Mem: %d KiB, Extended Mem: %d KiB\n",
           mb_info->mem_lower,
           mb_info->mem_upper);

    unsigned int map_size = mb_info->mmap_length / sizeof(multiboot_memory_map_t);
    setup_memory(mb_info->mmap_addr, map_size);
    setup_kernel_runtime();
}

void 
_kernel_post_init() {
    printf("[KERNEL] === Post Initialization === \n");
    size_t hhk_init_pg_count = ((uintptr_t)(&__init_hhk_end)) >> PG_SIZE_BITS;
    printf("[MM] Releaseing %d pages from 0x0.\n", hhk_init_pg_count);

    // 清除 hhk_init 与前1MiB的映射
    for (size_t i = 0; i < hhk_init_pg_count; i++) {
        vmm_unmap_page((void*)(i << PG_SIZE_BITS));
    }
}

// 按照 Memory map 标识可用的物理页
void
setup_memory(multiboot_memory_map_t* map, size_t map_size) {
    for (unsigned int i = 0; i < map_size; i++) {
        multiboot_memory_map_t mmap = map[i];
        printf("[MM] Base: 0x%x, len: %u KiB, type: %u\n",
               map[i].addr_low,
               map[i].len_low >> 10,
               map[i].type);
        if (mmap.type == MULTIBOOT_MEMORY_AVAILABLE) {
            // 整数向上取整除法
            uintptr_t pg = map[i].addr_low + 0x0fffU;
            pmm_mark_chunk_free(pg >> PG_SIZE_BITS, map[i].len_low >> PG_SIZE_BITS);
            printf("[MM] Freed %u pages start from 0x%x\n",
                   map[i].len_low >> PG_SIZE_BITS,
                   pg & ~0x0fffU);
        }
    }

    // 将内核占据的页设为已占用
    size_t pg_count = (uintptr_t)(&__kernel_end - &__kernel_start) >> PG_SIZE_BITS;
    pmm_mark_chunk_occupied(V2P(&__kernel_start) >> PG_SIZE_BITS, pg_count);
    printf("[MM] Allocated %d pages for kernel.\n", pg_count);


    size_t vga_buf_pgs = VGA_BUFFER_SIZE >> PG_SIZE_BITS;
    
    // 首先，标记VGA部分为已占用
    pmm_mark_chunk_occupied(VGA_BUFFER_PADDR >> PG_SIZE_BITS, vga_buf_pgs);
    
    // 重映射VGA文本缓冲区（以后会变成显存，i.e., framebuffer）
    for (size_t i = 0; i < vga_buf_pgs; i++)
    {
        vmm_map_page(
            (void*)(VGA_BUFFER_VADDR + (i << PG_SIZE_BITS)), 
            (void*)(VGA_BUFFER_PADDR + (i << PG_SIZE_BITS)), 
            PG_PREM_RW, PG_PREM_RW
        );
    }
    
    // 更新VGA缓冲区位置至虚拟地址
    tty_set_buffer((void*)VGA_BUFFER_VADDR);

    printf("[MM] Mapped VGA to %p.\n", VGA_BUFFER_VADDR);
}

void
setup_kernel_runtime() {
    // 为内核创建一个专属栈空间。
    for (size_t i = 0; i < (K_STACK_SIZE >> PG_SIZE_BITS); i++) {
        vmm_alloc_page((void*)(K_STACK_START + (i << PG_SIZE_BITS)), PG_PREM_RW, PG_PREM_RW);
    }
    printf("[MM] Allocated %d pages for stack start at %p\n", K_STACK_SIZE>>PG_SIZE_BITS, K_STACK_START);
}