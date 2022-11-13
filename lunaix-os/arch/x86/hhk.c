#include <arch/x86/boot/multiboot.h>
#include <arch/x86/idt.h>
#include <lunaix/common.h>
#include <lunaix/mm/page.h>

#define PT_ADDR(ptd, pt_index) ((ptd_t*)ptd + (pt_index + 1) * 1024)
#define SET_PDE(ptd, pde_index, pde) *((ptd_t*)ptd + pde_index) = pde;
#define SET_PTE(ptd, pt_index, pte_index, pte)                                 \
    *(PT_ADDR(ptd, pt_index) + pte_index) = pte;
#define sym_val(sym) (uintptr_t)(&sym)

#define KERNEL_PAGE_COUNT                                                      \
    ((sym_val(__kernel_end) - sym_val(__kernel_start) + 0x1000 - 1) >> 12);
#define HHK_PAGE_COUNT ((sym_val(__init_hhk_end) - 0x100000 + 0x1000 - 1) >> 12)

// use table #1
#define PG_TABLE_IDENTITY 0

// use table #2-8
// hence the max size of kernel is 8MiB
#define PG_TABLE_KERNEL 1

// use table #9
#define PG_TABLE_STACK 8

// Provided by linker (see linker.ld)
extern uint8_t __kernel_start;
extern uint8_t __kernel_end;
extern uint8_t __init_hhk_end;
extern uint8_t _k_stack;

void
_init_page(ptd_t* ptd)
{
    SET_PDE(ptd, 0, NEW_L1_ENTRY(PG_PREM_RW, ptd + PG_MAX_ENTRIES))

    // 对低1MiB空间进行对等映射（Identity
    // mapping），也包括了我们的VGA，方便内核操作。
    for (u32_t i = 0; i < 256; i++) {
        SET_PTE(ptd,
                PG_TABLE_IDENTITY,
                i,
                NEW_L2_ENTRY(PG_PREM_RW, (i << PG_SIZE_BITS)))
    }

    // 对等映射我们的hhk_init，这样一来，当分页与地址转换开启后，我们依然能够照常执行最终的
    // jmp 指令来跳转至
    //  内核的入口点
    for (u32_t i = 0; i < HHK_PAGE_COUNT; i++) {
        SET_PTE(ptd,
                PG_TABLE_IDENTITY,
                256 + i,
                NEW_L2_ENTRY(PG_PREM_RW, 0x100000 + (i << PG_SIZE_BITS)))
    }

    // --- 将内核重映射至高半区 ---

    // 这里是一些计算，主要是计算应当映射进的 页目录 与 页表 的条目索引（Entry
    // Index）
    u32_t kernel_pde_index = L1_INDEX(sym_val(__kernel_start));
    u32_t kernel_pte_index = L2_INDEX(sym_val(__kernel_start));
    u32_t kernel_pg_counts = KERNEL_PAGE_COUNT;

    // 将内核所需要的页表注册进页目录
    //  当然，就现在而言，我们的内核只占用不到50个页（每个页表包含1024个页）
    //  这里分配了3个页表（12MiB），未雨绸缪。
    for (u32_t i = 0; i < PG_TABLE_STACK - PG_TABLE_KERNEL; i++) {
        SET_PDE(ptd,
                kernel_pde_index + i,
                NEW_L1_ENTRY(PG_PREM_URW, PT_ADDR(ptd, PG_TABLE_KERNEL + i)))
    }

    // 首先，检查内核的大小是否可以fit进我们这几个表（12MiB）
    if (kernel_pg_counts >
        (PG_TABLE_STACK - PG_TABLE_KERNEL) * PG_MAX_ENTRIES) {
        // ERROR: require more pages
        //  here should do something else other than head into blocking
        asm("ud2");
    }

    // 计算内核.text段的物理地址
    uintptr_t kernel_pm = V2P(&__kernel_start);

    // 重映射内核至高半区地址（>=0xC0000000）
    for (u32_t i = 0; i < kernel_pg_counts; i++) {
        // FIXME: 只是用作用户模式（R3）测试！
        //        在实际中，内核代码除了极少部分需要暴露给R3（如从信号返回），其余的应为R0。
        SET_PTE(ptd,
                PG_TABLE_KERNEL,
                kernel_pte_index + i,
                NEW_L2_ENTRY(PG_PREM_URW, kernel_pm + (i << PG_SIZE_BITS)))
    }

    // 最后一个entry用于循环映射
    SET_PDE(ptd, PG_MAX_ENTRIES - 1, NEW_L1_ENTRY(T_SELF_REF_PERM, ptd));
}

u32_t
__save_subset(uint8_t* destination, uint8_t* base, unsigned int size)
{
    unsigned int i = 0;
    for (; i < size; i++) {
        *(destination + i) = *(base + i);
    }
    return i;
}

void
_save_multiboot_info(multiboot_info_t* info, uint8_t* destination)
{
    u32_t current = 0;
    uint8_t* info_b = (uint8_t*)info;
    for (; current < sizeof(multiboot_info_t); current++) {
        *(destination + current) = *(info_b + current);
    }

    ((multiboot_info_t*)destination)->mmap_addr =
      (uintptr_t)destination + current;
    current += __save_subset(
      destination + current, (uint8_t*)info->mmap_addr, info->mmap_length);

    if (present(info->flags, MULTIBOOT_INFO_DRIVE_INFO)) {
        ((multiboot_info_t*)destination)->drives_addr =
          (uintptr_t)destination + current;
        current += __save_subset(destination + current,
                                 (uint8_t*)info->drives_addr,
                                 info->drives_length);
    }
}

void
_hhk_init(ptd_t* ptd, u32_t kpg_size)
{

    // 初始化 kpg 全为0
    //      P.s. 真没想到GRUB会在这里留下一堆垃圾！ 老子的页表全乱套了！
    uint8_t* kpg = (uint8_t*)ptd;
    for (u32_t i = 0; i < kpg_size; i++) {
        *(kpg + i) = 0;
    }

    _init_page(ptd);
}