#ifndef __DTTEST_COMMON_H
#define __DTTEST_COMMON_H

#define __off_t_defined
#include "dut/devtree.h"

#include <lunaix/types.h>

bool
my_load_dtb();

ptr_t
load_fdt();

#endif /* __DTTEST_COMMON_H */
