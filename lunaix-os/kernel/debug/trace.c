#include <lunaix/mm/mmio.h>
#include <lunaix/mm/page.h>
#include <lunaix/spike.h>
#include <lunaix/trace.h>

#include <klibc/string.h>

static struct trace_context trace_ctx;

void
trace_modksyms_init(struct boot_handoff* bhctx)
{
    struct boot_modent* modents = bhctx->mods.entries;
    for (size_t i = 0; i < bhctx->mods.mods_num; i++) {
        struct boot_modent* mod = &bhctx->mods.entries[i];
        if (streq(mod->str, "modksyms")) {
            // In case boot loader does not place our ksyms on page boundary
            ptr_t start = PG_ALIGN(mod->start);
            ptr_t end = ROUNDUP(mod->end, PG_SIZE);
            ptr_t ksym_va = (ptr_t)ioremap(start, (end - start));

            trace_ctx.ksym_table =
              (struct ksyms*)(ksym_va + (mod->start - start));
        }
    }
}

struct ksym_entry*
trace_sym_lookup(ptr_t pc)
{
    return NULL;
}

void
trace_walkback(ptr_t fp)
{
}