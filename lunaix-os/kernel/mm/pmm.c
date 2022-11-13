#include <lunaix/mm/page.h>
#include <lunaix/mm/pmm.h>
#include <lunaix/status.h>

// This is a very large array...
static struct pp_struct pm_table[PM_BMP_MAX_SIZE];

static uintptr_t max_pg;

void
pmm_mark_page_free(uintptr_t ppn)
{
    pm_table[ppn].ref_counts = 0;
}

void
pmm_mark_page_occupied(pid_t owner, uintptr_t ppn, pp_attr_t attr)
{
    pm_table[ppn] =
      (struct pp_struct){ .owner = owner, .ref_counts = 1, .attr = attr };
}

void
pmm_mark_chunk_free(uintptr_t start_ppn, size_t page_count)
{
    for (size_t i = start_ppn; i < start_ppn + page_count && i < max_pg; i++) {
        pm_table[i].ref_counts = 0;
    }
}

void
pmm_mark_chunk_occupied(pid_t owner,
                        u32_t start_ppn,
                        size_t page_count,
                        pp_attr_t attr)
{
    for (size_t i = start_ppn; i < start_ppn + page_count && i < max_pg; i++) {
        pm_table[i] =
          (struct pp_struct){ .owner = owner, .ref_counts = 1, .attr = attr };
    }
}

// 我们跳过位于0x0的页。我们不希望空指针是指向一个有效的内存空间。
#define LOOKUP_START 1

volatile size_t pg_lookup_ptr;

void
pmm_init(uintptr_t mem_upper_lim)
{
    max_pg = (PG_ALIGN(mem_upper_lim) >> 12);

    pg_lookup_ptr = LOOKUP_START;

    // mark all as occupied
    for (size_t i = 0; i < PM_BMP_MAX_SIZE; i++) {
        pm_table[i] =
          (struct pp_struct){ .owner = 0, .attr = 0, .ref_counts = 1 };
    }
}

void*
pmm_alloc_cpage(pid_t owner, size_t num_pages, pp_attr_t attr)
{
    size_t p1 = 0;
    size_t p2 = 0;

    while (p2 < max_pg && p2 - p1 < num_pages) {
        (!(&pm_table[p2])->ref_counts) ? (p2++) : (p1 = ++p2);
    }

    if (p2 == max_pg && p2 - p1 < num_pages) {
        return NULL;
    }

    pmm_mark_chunk_occupied(owner, p1, num_pages, attr);

    return p1 << 12;
}

void*
pmm_alloc_page(pid_t owner, pp_attr_t attr)
{
    // Next fit approach. Maximize the throughput!
    uintptr_t good_page_found = (uintptr_t)NULL;
    size_t old_pg_ptr = pg_lookup_ptr;
    size_t upper_lim = max_pg;
    struct pp_struct* pm;
    while (!good_page_found && pg_lookup_ptr < upper_lim) {
        pm = &pm_table[pg_lookup_ptr];

        if (!pm->ref_counts) {
            *pm = (struct pp_struct){ .attr = attr,
                                      .owner = owner,
                                      .ref_counts = 1 };
            good_page_found = pg_lookup_ptr << 12;
            break;
        } else {
            pg_lookup_ptr++;

            // We've searched the interval [old_pg_ptr, max_pg) but failed
            //   may be chances in [1, old_pg_ptr) ?
            // Let's find out!
            if (pg_lookup_ptr >= upper_lim && old_pg_ptr != LOOKUP_START) {
                upper_lim = old_pg_ptr;
                pg_lookup_ptr = LOOKUP_START;
                old_pg_ptr = LOOKUP_START;
            }
        }
    }
    if (!good_page_found) {
        __current->k_status = LXOUTOFMEM;
    }
    return (void*)good_page_found;
}

int
pmm_free_page(pid_t owner, void* page)
{
    struct pp_struct* pm = &pm_table[(intptr_t)page >> 12];

    // Is this a MMIO mapping or double free?
    if (((intptr_t)page >> 12) >= max_pg || !(pm->ref_counts)) {
        return 0;
    }

    // 如果是锁定页，则不作处理
    if ((pm->attr & PP_FGLOCKED)) {
        return 0;
    }

    // TODO: 检查权限，保证：1) 只有正在使用该页（包括被分享者）的进程可以释放；
    // 2) 内核可释放所有页。
    pm->ref_counts--;
    return 1;
}

int
pmm_ref_page(pid_t owner, void* page)
{
    (void*)owner; // TODO: do smth with owner

    u32_t ppn = (uintptr_t)page >> 12;

    if (ppn >= PM_BMP_MAX_SIZE) {
        return 0;
    }

    struct pp_struct* pm = &pm_table[ppn];
    if (ppn >= max_pg || !pm->ref_counts) {
        return 0;
    }

    pm->ref_counts++;
    return 1;
}

struct pp_struct*
pmm_query(void* pa)
{
    u32_t ppn = (uintptr_t)pa >> 12;

    if (ppn >= PM_BMP_MAX_SIZE) {
        return NULL;
    }

    return &pm_table[ppn];
}