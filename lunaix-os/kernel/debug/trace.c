#include <lunaix/mm/page.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/process.h>
#include <lunaix/spike.h>
#include <lunaix/syslog.h>
#include <lunaix/trace.h>

#include <sys/cpu.h>
#include <sys/mm/mempart.h>

#include <klibc/string.h>

#define NB_TRACEBACK 16

LOG_MODULE("TRACE")

static struct trace_context trace_ctx;

void
trace_modksyms_init(struct boot_handoff* bhctx)
{
    struct boot_modent* modents = bhctx->mods.entries;
    for (size_t i = 0; i < bhctx->mods.mods_num; i++) {
        struct boot_modent* mod = &bhctx->mods.entries[i];
        if (streq(mod->str, "modksyms")) {
            assert(PG_ALIGNED(mod->start));

            ptr_t end = ROUNDUP(mod->end, PG_SIZE);
            ptr_t ksym_va =
              (ptr_t)vmap(mod->start, (end - mod->start), PG_PREM_R, 0);

            assert(ksym_va);
            trace_ctx.ksym_table = (struct ksyms*)ksym_va;
        }
    }
}

struct ksym_entry*
trace_sym_lookup(ptr_t addr)
{
    int c = trace_ctx.ksym_table->ksym_count;
    struct ksym_entry* ksent = trace_ctx.ksym_table->syms;

    int i = c - 1, j = 0, m = 0;

    if (addr > ksent[i].pc || addr < ksent[j].pc || addr < KERNEL_EXEC) {
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
        return "???";
    }

    return (char*)((ptr_t)trace_ctx.ksym_table +
                   trace_ctx.ksym_table->ksym_label_off + sym->label_off);
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

    while (frame && i < limit) {
        ptr_t pc = *(frame + 1);

        current = trace_sym_lookup(pc);
        tb_buffer[i] = (struct trace_record){ .pc = current ? current->pc : pc,
                                              .symbol = ksym_getstr(current) };

        frame = (ptr_t*)*frame;
        i++;
    }

    if (last_fp) {
        *last_fp = (ptr_t)frame;
    }

    return i;
}

void
trace_printstack_of(ptr_t fp)
{
    struct trace_record tbs[NB_TRACEBACK];

    int n = trace_walkback(tbs, fp, NB_TRACEBACK, &fp);

    if (fp) {
        kprintf(KDEBUG "...<truncated>\n");
    }

    for (int i = 0; i < n; i++) {
        kprintf(KDEBUG "%p: %s\n", tbs[i].pc, tbs[i].symbol);
    }
}

void
trace_printstack()
{
    trace_printstack_of(cpu_get_fp());
}

static void
trace_printswctx(const isr_param* p, char* direction)
{

    struct ksym_entry* sym = trace_sym_lookup(p->execp->eip);

    kprintf(KDEBUG ">> (sw:%s) iv:%d, errno:%p <<\n",
            direction,
            p->execp->vector,
            p->execp->err_code);
    kprintf(KDEBUG "%p:%s\n", p->execp->eip, ksym_getstr(sym));
}

void
trace_printstack_isr(const isr_param* isrm)
{
    isr_param* p = isrm;
    ptr_t fp = cpu_get_fp();
    int prev_fromusr = uspace_context(p);

    kprintf(KDEBUG "\n");
    kprintf(KDEBUG "stack trace (pid=%d)\n", __current->pid);

    trace_printstack_of(fp);

    while (p) {
        if (!prev_fromusr) {
            if (uspace_context(p)) {
                trace_printswctx(p, "s/u");
            } else {
                trace_printswctx(p, "s/s");
            }
        } else {
            trace_printswctx(p, "u/s");
        }

        fp = saved_fp(p);
        trace_printstack_of(fp);

        prev_fromusr = uspace_context(p);

        p = p->execp->saved_prev_ctx;
    }
    kprintf(KDEBUG "\n");
}