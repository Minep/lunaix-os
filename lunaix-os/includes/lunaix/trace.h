#ifndef __LUNAIX_TRACE_H
#define __LUNAIX_TRACE_H

#include <lunaix/boot_generic.h>

struct ksym_entry
{
    ptr_t pc;
    u32_t label_off;
};

struct ksyms
{
    u32_t ksym_count;
    u32_t ksym_label_off;
    struct ksym_entry syms[0];
};

struct trace_context
{
    struct ksyms* ksym_table;
};

void
trace_modksyms_init(struct boot_handoff* bhctx);

struct ksym_entry*
trace_sym_lookup(ptr_t pc);

void
trace_walkback(ptr_t fp);

#endif /* __LUNAIX_TRACE_H */
