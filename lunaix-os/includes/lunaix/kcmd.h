#ifndef __LUNAIX_KCMD_H
#define __LUNAIX_KCMD_H

#include <lunaix/compiler.h>
#include <lunaix/ds/hashtable.h>
#include <lunaix/ds/hstr.h>

struct align(64) koption {
    struct hlist_node node;
    struct hstr hashkey;
    char* value;
    char buf[0];
};

void kcmd_parse_cmdline(char* cmd_line);

bool kcmd_get_option(char* key, char** out_value);



#endif /* __LUNAIX_KCMD_H */
