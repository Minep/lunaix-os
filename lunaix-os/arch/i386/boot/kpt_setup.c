#define __BOOT_CODE__

#include <lunaix/mm/page.h>

#include <sys/boot/bstage.h>
#include <sys/mm/mempart.h>

#define PT_ADDR(ptd, pt_index) ((ptd_t*)ptd + (pt_index + 1) * 1024)
#define SET_PDE(ptd, pde_index, pde) *((ptd_t*)ptd + pde_index) = pde;
#define SET_PTE(ptd, pt_index, pte_index, pte)                                 \
    *(PT_ADDR(ptd, pt_index) + pte_index) = pte;
#define sym_val(sym) (ptr_t)(&sym)

#define KERNEL_PAGE_COUNT                                                      \
    ((sym_val(__kexec_end) - sym_val(__kexec_start) + 0x1000 - 1) >> 12);
#define HHK_PAGE_COUNT                                                         \
    ((sym_val(__kexec_boot_end) - 0x100000 + 0x1000 - 1) >> 12)

#define V2P(vaddr) ((ptr_t)(vaddr)-KERNEL_EXEC)

// use table #1
#define PG_TABLE_IDENTITY 0

// use table #2-8
// hence the max size of kernel is 8MiB
#define PG_TABLE_KERNEL 1

// use table #9
#define PG_TABLE_STACK 8

// Provided by linker (see linker.ld)
extern u8_t __kexec_start;
extern u8_t __kexec_end;
extern u8_t __kexec_text_start;
extern u8_t __kexec_text_end;

extern u8_t __kexec_boot_end;

void boot_text
_init_page(x86_page_table* ptd)
{
    ptd->entry[0] = NEW_L1_ENTRY(PG_PREM_RW, (ptd_t*)ptd + PG_MAX_ENTRIES);

    // 对低1MiB空间进行对等映射（Identity
    // mapping），也包括了我们的VGA，方便内核操作。
    x86_page_table* id_pt =
      (x86_page_table*)GET_PG_ADDR(ptd->entry[PG_TABLE_IDENTITY]);

    for (u32_t i = 0; i < 256; i++) {
        id_pt->entry[i] = NEW_L2_ENTRY(PG_PREM_RW, (i << PG_SIZE_BITS));
    }

    // 对等映射我们的hhk_init，这样一来，当分页与地址转换开启后，我们依然能够照常执行最终的
    // jmp 指令来跳转至
    //  内核的入口点
    for (u32_t i = 0; i < HHK_PAGE_COUNT; i++) {
        id_pt->entry[256 + i] =
          NEW_L2_ENTRY(PG_PREM_RW, 0x100000 + (i << PG_SIZE_BITS));
    }

    // --- 将内核重映射至高半区 ---

    // 这里是一些计算，主要是计算应当映射进的 页目录 与 页表 的条目索引（Entry
    // Index）
    u32_t kernel_pde_index = L1_INDEX(sym_val(__kexec_start));
    u32_t kernel_pte_index = L2_INDEX(sym_val(__kexec_start));
    u32_t kernel_pg_counts = KERNEL_PAGE_COUNT;

    // 将内核所需要的页表注册进页目录
    //  当然，就现在而言，我们的内核只占用不到50个页（每个页表包含1024个页）
    //  这里分配了3个页表（12MiB），未雨绸缪。
    for (u32_t i = 0; i < PG_TABLE_STACK - PG_TABLE_KERNEL; i++) {
        ptd->entry[kernel_pde_index + i] =
          NEW_L1_ENTRY(PG_PREM_RW, PT_ADDR(ptd, PG_TABLE_KERNEL + i));
    }

    // 首先，检查内核的大小是否可以fit进我们这几个表（12MiB）
    if (kernel_pg_counts >
        (PG_TABLE_STACK - PG_TABLE_KERNEL) * PG_MAX_ENTRIES) {
        // ERROR: require more pages
        //  here should do something else other than head into blocking
        asm("ud2");
    }

    // 计算内核.text段的物理地址
    ptr_t kernel_pm = V2P(&__kexec_start);
    ptr_t ktext_start = V2P(&__kexec_text_start);
    ptr_t ktext_end = V2P(&__kexec_text_end);

    // 重映射内核至高半区地址（>=0xC0000000）
    for (u32_t i = 0; i < kernel_pg_counts; i++) {
        ptr_t paddr = kernel_pm + (i << PG_SIZE_BITS);
        u32_t flags = PG_PREM_RW;

        if (paddr >= ktext_start && paddr <= ktext_end) {
            flags = PG_PREM_R;
        }

        SET_PTE(ptd,
                PG_TABLE_KERNEL,
                kernel_pte_index + i,
                NEW_L2_ENTRY(flags, paddr))
    }

    // 最后一个entry用于循环映射
    ptd->entry[PG_MAX_ENTRIES - 1] = NEW_L1_ENTRY(T_SELF_REF_PERM, ptd);
}

void boot_text
kpg_init(x86_page_table* ptd, u32_t kpg_size)
{

    // 初始化 kpg 全为0
    //      P.s. 真没想到GRUB会在这里留下一堆垃圾！ 老子的页表全乱套了！
    u8_t* kpg = (u8_t*)ptd;
    for (u32_t i = 0; i < kpg_size; i++) {
        *(kpg + i) = 0;
    }

    _init_page(ptd);
}