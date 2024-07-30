#include <lunaix/mm/page.h>
#include <lunaix/process.h>
#include <lunaix/spike.h>
#include <lunaix/syslog.h>
#include <lunaix/trace.h>

#include <sys/abi.h>
#include <sys/mm/mm_defs.h>
#include <sys/trace.h>

#include <klibc/string.h>

#define NB_TRACEBACK 16

LOG_MODULE("TRACE")

weak struct ksyms __lunaix_ksymtable[] = { };
extern struct ksyms __lunaix_ksymtable[];

static struct trace_context trace_ctx;

void
trace_log(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    kprintf_m("TRACE", fmt, args);

    va_end(args);
}

void
trace_modksyms_init(struct boot_handoff* bhctx)
{
    trace_ctx.ksym_table = __lunaix_ksymtable;
}

struct ksym_entry*
trace_sym_lookup(ptr_t addr)
{
    unsigned long c = trace_ctx.ksym_table->ksym_count;
    struct ksym_entry* ksent = trace_ctx.ksym_table->syms;

    int i = c - 1, j = 0, m = 0;

    if (addr > ksent[i].pc || addr < ksent[j].pc || !kernel_addr(addr)) {
        return NULL;
    }

    while (i - j != 1) {
        m = (i + j) / 2;
        if (ksent[m].pc > addr) {
            i = m;
        } else if (ksent[m].pc < addr) {
            j = m;
        } else {
            break;
        }
    }

    struct ksym_entry* result = &ksent[MIN(i, j)];
    if (result->pc > addr) {
        return NULL;
    }

    return result;
}

static char*
ksym_getstr(struct ksym_entry* sym)
{
    if (!sym) {
        return NULL;
    }

    return sym->label;
}

static inline bool valid_fp(ptr_t ptr) {
    ptr_t start = ROUNDUP(current_thread->kstack - KSTACK_SIZE, PAGE_SIZE);

    return (start < ptr && ptr < current_thread->kstack) 
           || arch_valid_fp(ptr);
}

int
trace_walkback(struct trace_record* tb_buffer,
               ptr_t fp,
               int limit,
               ptr_t* last_fp)
{
    ptr_t* frame = (ptr_t*)fp;
    struct ksym_entry* current = NULL;
    int i = 0;

    while (valid_fp((ptr_t)frame) && i < limit) {
        ptr_t pc = abi_get_retaddrat((ptr_t)frame);

        current = trace_sym_lookup(pc);
        tb_buffer[i] =
          (struct trace_record){ .pc = pc,
                                 .sym_pc = current ? current->pc : 0,
                                 .symbol = ksym_getstr(current) };

        frame = (ptr_t*)*frame;
        i++;
    }

    if (!valid_fp((ptr_t)frame)) {
        frame = NULL;
    }

    if (last_fp) {
        *last_fp = (ptr_t)frame;
    }

    return i;
}

static inline void
trace_print_code_entry(ptr_t sym_pc, ptr_t inst_pc, char* sym)
{
    if (sym) {
        trace_log("%s+%p", sym, inst_pc - sym_pc);
    } else {
        trace_log("??? [%p]", inst_pc);
    }
}

void
trace_printstack_of(ptr_t fp)
{
    struct trace_record tbs[NB_TRACEBACK];

    // Let's get our Stackwalker does his job ;)
    int n = trace_walkback(tbs, fp, NB_TRACEBACK, &fp);

    if (fp) {
        trace_log("...<truncated>");
    }

    for (int i = 0; i < n; i++) {
        struct trace_record* tb = &tbs[i];
        trace_print_code_entry(tb->sym_pc, tb->pc, tb->symbol);
    }
}

void
trace_printstack()
{
    if (current_thread) {
        trace_printstack_isr(current_thread->hstate);
    }
    else {
        trace_printstack_of(abi_get_callframe());
    }
}

static void
trace_printswctx(const struct hart_state* hstate, bool from_usr, bool to_usr)
{

    struct ksym_entry* sym = trace_sym_lookup(hart_pc(hstate));

    trace_log("^^^^^ --- %s", to_usr ? "user" : "kernel");
    trace_print_transistion_short(hstate);
    trace_log("vvvvv --- %s", from_usr ? "user" : "kernel");

    ptr_t sym_pc = sym ? sym->pc : hart_pc(hstate);
    trace_print_code_entry(sym_pc, hart_pc(hstate), ksym_getstr(sym));
}

void
trace_printstack_isr(const struct hart_state* hstate)
{
    struct hart_state* p = hstate;
    ptr_t fp = abi_get_callframe();
    int prev_usrctx = 0;

    trace_log("stack trace (pid=%d)\n", __current->pid);

    trace_printstack_of(fp);

    while (p) {
        if (!prev_usrctx) {
            if (!kernel_context(p)) {
                trace_printswctx(p, true, false);
            } else {
                trace_printswctx(p, false, false);
            }
        } else {
            trace_printswctx(p, false, true);
        }

        fp = hart_stack_frame(p);
        if (!valid_fp(fp)) {
            trace_log("??? invalid frame: %p", fp);
            break;
        }

        trace_printstack_of(fp);

        prev_usrctx = !kernel_context(p);

        p = hart_parent_state(p);
    }

    trace_log("----- [trace end] -----\n");
}