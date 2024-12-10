#include <testing/memchk.h>
#include <stdio.h>

#include <lunaix/ds/llist.h>

extern void *malloc(size_t);
extern void *calloc(size_t, size_t);
extern void free(void*);

struct valloc_stats valloc_stat = { };
static DEFINE_LLIST(records);

struct addr_record {
    struct llist_header link;
    unsigned long addr;
    unsigned long size;
};

void
memchk_log_alloc(unsigned long addr, unsigned long size)
{
    valloc_stat.alloced += size;
    valloc_stat.nr_valloc_calls++;

    struct addr_record *record;

    record = malloc(sizeof(struct addr_record));
    record->addr = addr;
    record->size = size;

    llist_append(&records, &record->link);
}

void
memchk_log_free(unsigned long addr)
{
    valloc_stat.nr_vfree_calls++;

    struct addr_record *pos, *n;
    llist_for_each(pos, n, &records, link)
    {
        if (pos->addr == addr) {
            valloc_stat.freed += pos->size;
            llist_delete(&pos->link);

            free(pos);
            return;
        }
    }

    printf("[\x1b[33;49mWARN\x1b[0m] freeing undefined pointer: 0x%lx\n", addr);
}


void
memchk_print_stats()
{
    long leaked;

    leaked = valloc_stat.alloced - valloc_stat.freed;
    printf("valloc() statistics:\n");
    printf("  allocated:   %lu bytes\n", valloc_stat.alloced);
    printf("  freed:       %lu bytes\n", valloc_stat.freed);
    printf("  leaked:      %lu bytes\n", leaked);
    printf("  vfree_call:  %u  times\n", valloc_stat.nr_vfree_calls);
    printf("  valloc_call: %u  times\n", valloc_stat.nr_valloc_calls);

    if (leaked)
    {
        printf("[\x1b[33;49mWARN\x1b[0m] memory leakage detected\n");
    }
}