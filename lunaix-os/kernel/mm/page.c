#include <lunaix/mm/pmm.h>
#include <lunaix/mm/page.h>
#include <lunaix/spike.h>

struct leaflet*
leaflet_alloc_order(pgpol_t alloc_pol, int order)
{
    struct pmalloc_pol pol;
    struct leaflet* allocated;
    int retries = 0;

    pmm_explain_policy(&pol, alloc_pol);
    assert(pol.src_pool);

    do {
        allocated = (struct leaflet*)pmm_try_alloc_one(order, &pol);

        if (allocated)
            break;

        retries++;

        /*
         * TODO [2026-PMEM] reclaim, sleep, and retry
         *
         * Reclaim: go through the lru list and reclaim one with 
         *           highest watermark position
         *
         * Sleep:   yield the current execution time slot. Need to 
         *           ensure we are out of early boot. Although it 
         *           is unlikely we get a fail allocation in that
         *           stage, some thing could go wrong anyway.
         */
    } while (retries < pol.max_attempt);

    if (!allocated) {
        if (pol.panic_on_fail)
            fail("failed to allocate a leaflet");

        return NULL;
    }

    if (pol.should_zero) {
        memset((void*)leaflet_va(allocated), 0, leaflet_size(allocated));
    }

    struct ppage* pg;
    foreach_page_in_leaflet(allocated, pg) {
        pg->pol = pol.policy;
        pg->pool = pol.src_pool->type;
    }

    assert(leaflet_refcount(allocated) == 0);
    allocated->lead_page.refs = 1;

    return allocated;
}
