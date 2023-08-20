#ifndef __LUNAIX_TRACE_H
#define __LUNAIX_TRACE_H

#include <lunaix/boot_generic.h>
#include <sys/interrupts.h>

struct ksym_entry
{
    ptr_t pc;
    u32_t label_off;
};

struct trace_record
{
    ptr_t pc;
    char* symbol;
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

int
trace_walkback(struct trace_record* tb_buffer,
               ptr_t fp,
               int limit,
               ptr_t* last_fp);

void
trace_printstack_of(ptr_t fp);

void
trace_printstack_isr(const isr_param* isrm);

void
trace_printstack();

#endif /* __LUNAIX_TRACE_H */
