#include "common.h"
#include <testing/basic.h>

static void
test_fdt_memory_node(ptr_t dtb)
{
    struct fdt_blob fdt;
    struct fdt_memscan mscan;
    struct dt_memory_node mnode;

    fdt_load(&fdt, dtb);
    fdt_memscan_begin(&mscan, &fdt);

    testcase("initial-state", {
        expect_int(mscan.root_addr_c, 2);
        expect_int(mscan.root_size_c, 1);
    });

    testcase("memory-node-1", {
        expect_true(fdt_memscan_nextnode(&mscan, &fdt));

        expect_str(mscan.found.node->name, "memory@1000000");
        expect_true(mscan.node_type == FDT_MEM_FREE);

        expect_true(fdt_memscan_nextrange(&mscan, &mnode));
        expect_ulong(mnode.base, 0x1000000UL);
        expect_ulong(mnode.size, 0x10000UL);

        expect_true(fdt_memscan_nextrange(&mscan, &mnode));
        expect_ulong(mnode.base, 0x8000000UL);
        expect_ulong(mnode.size, 0x10000UL);

        expect_true(fdt_memscan_nextrange(&mscan, &mnode));
        expect_ulong(mnode.base, 0x1002000000UL);
        expect_ulong(mnode.size, 0x200000UL);

        expect_false(fdt_memscan_nextrange(&mscan, &mnode));
    });

    testcase("memory-node-2", {
        expect_true(fdt_memscan_nextnode(&mscan, &fdt));

        expect_str(mscan.found.node->name, "memory@f000000");
        expect_true(mscan.node_type == FDT_MEM_FREE);

        expect_true(fdt_memscan_nextrange(&mscan, &mnode));
        expect_ulong(mnode.base, 0xf000000UL);
        expect_ulong(mnode.size, 0xff000UL);

        expect_false(fdt_memscan_nextrange(&mscan, &mnode));
    });

    testcase("reserved-node-1", {
        expect_true(fdt_memscan_nextnode(&mscan, &fdt));

        expect_str(mscan.found.node->name, "hwrom@0");
        expect_true(mscan.node_type == FDT_MEM_RSVD);
        expect_true(mscan.node_attr.nomap);

        expect_true(fdt_memscan_nextrange(&mscan, &mnode));
        expect_ulong(mnode.base, 0x0);
        expect_ulong(mnode.size, 0x1000000);

        expect_false(fdt_memscan_nextrange(&mscan, &mnode));
    });

    testcase("reserved-node-2", {
        expect_true(fdt_memscan_nextnode(&mscan, &fdt));

        expect_str(mscan.found.node->name, "cma");
        expect_true(mscan.node_type == FDT_MEM_RSVD_DYNAMIC);
        expect_true(mscan.node_attr.reusable);
        expect_false(mscan.node_attr.nomap);
        expect_ulong(mscan.node_attr.alignment, 0x400000);
        expect_ulong(mscan.node_attr.total_size, 0x10000000);

        expect_true(fdt_memscan_nextrange(&mscan, &mnode));
        expect_ulong(mnode.base, 0x10000000);
        expect_ulong(mnode.size, 0x10000000);

        expect_false(fdt_memscan_nextrange(&mscan, &mnode));
    });

    testcase("reserved-node-out-of-bound", {
        expect_false(fdt_memscan_nextnode(&mscan, &fdt));
    });
}

void
run_test(int argc, const char* argv[])
{
    ptr_t dtb = load_fdt();

    test_fdt_memory_node(dtb);
}