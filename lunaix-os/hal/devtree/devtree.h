#ifndef __LUNAIX_DEVTREE_INTERNAL_H
#define __LUNAIX_DEVTREE_INTERNAL_H

#include <hal/devtree.h>

bool
parse_stdintr_prop(struct fdt_iter* it, struct dt_intr_node* node);

#endif /* __LUNAIX_DEVTREE_H */
