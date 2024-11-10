#include <lunaix/spike.h>
#include <lunaix/device.h>
#include <asm/x86_isrm.h>

#include "asm/x86.h"
#include "asm/soc/ioapic.h"
#include "asm/soc/apic.h"

/*
    total: 256 ivs
    0~31: reserved for sys use (x32)
    32~47: reserved for os use (x16)
    48~  : free to allocate for external hardware use. (x208)
*/

static char iv_bmp[(IV_EX_END - IV_BASE_END) / 8];
static isr_cb handlers[TOTAL_IV];
static ptr_t ivhand_payload[TOTAL_IV];

static struct x86_intc arch_intc_ctx;

extern void
intr_routine_fallback(const struct hart_state* state);

void
isrm_init()
{
    for (size_t i = 0; i < TOTAL_IV; i++) {
        handlers[i] = intr_routine_fallback;
    }
}

static inline int
__ivalloc_within(size_t a, size_t b, isr_cb handler)
{
    a = (a - IV_BASE_END);
    b = (b - IV_BASE_END);
    u8_t j = a % 8;
    u8_t k = 0;

    for (size_t i = a / 8; i < b / 8; i++, k += 8) {
        u8_t chunk = iv_bmp[i];

        if (chunk == 0xff)
            continue;

        chunk >>= j;
        while ((chunk & 0x1) && k <= b) {
            chunk >>= 1;
            j++;
            k++;
        }

        if (j == 8) {
            j = 0;
            continue;
        }

        if (k > b) {
            break;
        }

        iv_bmp[i] |= 1 << j;

        int iv = IV_BASE_END + i * 8 + j;
        handlers[iv] = handler ? handler : intr_routine_fallback;

        return iv;
    }

    return 0;
}

int
isrm_ivosalloc(isr_cb handler)
{
    return __ivalloc_within(IV_BASE_END, IV_EX_BEGIN, handler);
}

int
isrm_ivexalloc(isr_cb handler)
{
    return __ivalloc_within(IV_EX_BEGIN, IV_EX_END, handler);
}

void
isrm_ivfree(int iv)
{
    assert(iv < 256);

    if (iv >= IV_BASE_END) {
        iv_bmp[(iv - IV_BASE_END) / 8] &= ~(1 << ((iv - IV_BASE_END) % 8));
    }

    handlers[iv] = intr_routine_fallback;
}

int
isrm_bindirq(int irq, isr_cb irq_handler)
{
    int iv;
    if (!(iv = isrm_ivexalloc(irq_handler))) {
        fail("out of IV resource.");
        return 0; // never reach
    }

    // fixed, edge trigged, polarity=high
    isrm_irq_attach(irq, iv, 0, IRQ_DEFAULT);

    return iv;
}

void
isrm_bindiv(int iv, isr_cb handler)
{
    assert(iv < 256);

    if (iv >= IV_BASE_END) {
        iv_bmp[(iv - IV_BASE_END) / 8] |= 1 << ((iv - IV_BASE_END) % 8);
    }

    handlers[iv] = handler;
}

isr_cb
isrm_get(int iv)
{
    assert(iv < 256);

    return handlers[iv];
}

ptr_t
isrm_get_payload(const struct hart_state* state)
{
    int iv = state->execp->vector;
    assert(iv < 256);

    return ivhand_payload[iv];
}

void
isrm_set_payload(int iv, ptr_t payload)
{
    assert(iv < 256);

    ivhand_payload[iv] = payload;
}

void
isrm_irq_attach(int irq, int iv, cpu_t dest, u32_t flags)
{
    arch_intc_ctx.irq_attach(&arch_intc_ctx, irq, iv, dest, flags);
}

void
isrm_notify_eoi(cpu_t id, int iv)
{
    arch_intc_ctx.notify_eoi(&arch_intc_ctx, id, iv);
}

void
isrm_notify_eos(cpu_t id)
{
    isrm_notify_eoi(id, LUNAIX_SCHED);
}

msienv_t
isrm_msi_start(struct device* dev)
{
    /*
     *  In x86, the MSI topology is rather simple, as the only
     *  source is the PCI itself, and the write destination is
     *  explictly defined in specification, so we don't need the
     *  msienv to hold dynamically probed address
     */
    return NULL;
}

int
isrm_msi_avaliable(msienv_t msienv)
{
    return 1;
}

msi_vector_t
isrm_msi_alloc(msienv_t msienv, cpu_t cpu, int index, isr_cb handler)
{
    unsigned int iv = isrm_ivexalloc(handler);

    // we ignore the cpu redirection for now.
    return (msi_vector_t){ 
        .msi_addr  = __APIC_BASE_PADDR,
        .msi_data  = iv,
        .mapped_iv = iv
    };
}

void
isrm_msi_set_sideband(msienv_t msienv, ptr_t sideband)
{
    return;
}

void
isrm_msi_done(msienv_t msienv)
{
    return;
}



int
isrm_bind_dtnode(struct dt_intr_node* node)
{
    fail("not supported");
}


static int
__intc_create(struct device_def* devdef, morph_t* obj)
{
    apic_init();
    ioapic_init();

    arch_intc_ctx.name = "i386_apic";
    arch_intc_ctx.irq_attach = ioapic_irq_remap;
    arch_intc_ctx.notify_eoi = apic_on_eoi;

    return 0;
}


static struct device_def i386_intc = {
    def_device_class(INTEL, CFG, INTC),
    def_device_name("i386 apic"),
    def_on_create(__intc_create)
};
EXPORT_DEVICE(i386_intc, &i386_intc, load_sysconf);