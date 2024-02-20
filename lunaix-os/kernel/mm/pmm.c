#include <lunaix/mm/pmm.h>
#include <lunaix/status.h>
#include <lunaix/mm/pagetable.h>
#include <lunaix/spike.h>

// This is a very large array...
static struct pp_struct pm_table[PM_BMP_MAX_SIZE];
export_symbol(debug, pmm, pm_table);

static ptr_t max_pg;
export_symbol(debug, pmm, max_pg);

void
pmm_mark_page_free(ptr_t ppn)
{
    if ((pm_table[ppn].attr & PP_FGLOCKED)) {
        return;
    }
    pm_table[ppn].ref_counts = 0;
}

void
pmm_mark_page_occupied(ptr_t ppn, pp_attr_t attr)
{
    pm_table[ppn] =
      (struct pp_struct){ .ref_counts = 1, .attr = attr };
}

void
pmm_mark_chunk_free(ptr_t start_ppn, size_t page_count)
{
    for (size_t i = start_ppn; i < start_ppn + page_count && i < max_pg; i++) {
        if ((pm_table[i].attr & PP_FGLOCKED)) {
            continue;
        }
        pm_table[i].ref_counts = 0;
    }
}

void
pmm_mark_chunk_occupied(u32_t start_ppn,
                        size_t page_count,
                        pp_attr_t attr)
{
    for (size_t i = start_ppn; i < start_ppn + page_count && i < max_pg; i++) {
        pm_table[i] =
          (struct pp_struct){ .ref_counts = 1, .attr = attr };
    }
}

// 我们跳过位于0x0的页。我们不希望空指针是指向一个有效的内存空间。
#define LOOKUP_START 1

volatile size_t pg_lookup_ptr;

void
pmm_init(ptr_t mem_upper_lim)
{
    max_pg = pfn(mem_upper_lim);

    pg_lookup_ptr = LOOKUP_START;

    // mark all as occupied
    for (size_t i = 0; i < PM_BMP_MAX_SIZE; i++) {
        pm_table[i] =
          (struct pp_struct){ .attr = 0, .ref_counts = 1 };
    }
}

ptr_t
pmm_alloc_cpage(size_t num_pages, pp_attr_t attr)
{
    size_t p1 = 0;
    size_t p2 = 0;

    while (p2 < max_pg && p2 - p1 < num_pages) {
        (!(&pm_table[p2])->ref_counts) ? (p2++) : (p1 = ++p2);
    }

    if (p2 == max_pg && p2 - p1 < num_pages) {
        return NULLPTR;
    }

    pmm_mark_chunk_occupied(p1, num_pages, attr);

    return p1 << 12;
}

ptr_t
pmm_alloc_page(pp_attr_t attr)
{
    // Next fit approach. Maximize the throughput!
    ptr_t good_page_found = (ptr_t)NULL;
    size_t old_pg_ptr = pg_lookup_ptr;
    size_t upper_lim = max_pg;
    struct pp_struct* pm;
    while (!good_page_found && pg_lookup_ptr < upper_lim) {
        pm = &pm_table[pg_lookup_ptr];

        if (!pm->ref_counts) {
            *pm = (struct pp_struct){ .attr = attr,
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
    return good_page_found;
}

int
pmm_free_one(ptr_t page, pp_attr_t attr_mask)
{
    pfn_t ppfn = pfn(page);
    struct pp_struct* pm = &pm_table[ppfn];

    assert(ppfn < max_pg && pm->ref_counts);
    if (pm->attr && !(pm->attr & attr_mask)) {
        return 0;
    }

    pm->ref_counts--;
    return 1;
}

int
pmm_ref_page(ptr_t page)
{
    u32_t ppn = pfn(page);

    if (ppn >= PM_BMP_MAX_SIZE) {
        return 0;
    }

    struct pp_struct* pm = &pm_table[ppn];
    assert(ppn < max_pg && pm->ref_counts);

    pm->ref_counts++;
    return 1;
}

void
pmm_set_attr(ptr_t page, pp_attr_t attr)
{
    struct pp_struct* pp = &pm_table[pfn(page)];
    
    if (pp->ref_counts) {
        pp->attr = attr;
    }
}

struct pp_struct*
pmm_query(ptr_t pa)
{
    u32_t ppn = pa >> 12;

    if (ppn >= PM_BMP_MAX_SIZE) {
        return NULL;
    }

    return &pm_table[ppn];
}