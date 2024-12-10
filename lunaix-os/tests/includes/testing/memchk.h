#ifndef __COMMON_TEST_MEMCHK_H
#define __COMMON_TEST_MEMCHK_H

struct valloc_stats
{
    unsigned long alloced;
    unsigned long freed;
    unsigned long nr_vfree_calls;
    
    unsigned int nr_valloc_calls;
};

extern struct valloc_stats valloc_stat;

void
memchk_log_alloc(unsigned long addr, unsigned long size);

void
memchk_log_free(unsigned long addr);

void
memchk_print_stats();

#endif /* __COMMON_TEST_MEMCHK_H */
