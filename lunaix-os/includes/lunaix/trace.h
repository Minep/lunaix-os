#ifndef __LUNAIX_TRACE_H
#define __LUNAIX_TRACE_H

#include <lunaix/boot_generic.h>
#include <lunaix/pcontext.h>

struct ksym_entry
{
    ptr_t pc;
    u32_t label_off;
};

struct trace_record
{
    ptr_t pc;
    ptr_t sym_pc;
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

/**
 * @brief Init the trace service using loaded modksyms module
 *
 * @param bhctx
 */
void
trace_modksyms_init(struct boot_handoff* bhctx);

/**
 * @brief Locate the symbol which is the closest to given pc.
 *
 * @param pc
 * @return struct ksym_entry*
 */
struct ksym_entry*
trace_sym_lookup(ptr_t pc);

/**
 * @brief Walk the stack backwards to generate stack trace
 *
 * @param tb_buffer
 * @param fp
 * @param limit
 * @param last_fp
 * @return int
 */
int
trace_walkback(struct trace_record* tb_buffer,
               ptr_t fp,
               int limit,
               ptr_t* last_fp);

/**
 * @brief Print the stack trace starting from the given frame pointer
 *
 * @param fp
 */
void
trace_printstack_of(ptr_t fp);

/**
 * @brief Print the stack trace given the current interrupt context. In addition
 * to the stacktrace, this will also print all context switches happened
 * beforehand, and all stack trace in each context. Recommended for verbose
 * debugging.
 *
 * @param isrm
 */
void
trace_printstack_isr(const isr_param* isrm);

/**
 * @brief Print the stack trace starting from caller's frame pointer.
 *
 */
void
trace_printstack();

#endif /* __LUNAIX_TRACE_H */
