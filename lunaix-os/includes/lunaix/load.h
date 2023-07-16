#ifndef __LUNAIX_LOAD_H
#define __LUNAIX_LOAD_H

#include <lunaix/exec.h>
#include <lunaix/fs.h>

int
load_executable(struct load_context* context, const struct v_file* exefile);

#endif /* __LUNAIX_LOAD_H */
