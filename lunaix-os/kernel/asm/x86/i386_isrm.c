#include <hal/acpi/acpi.h>
#include <hal/ioapic.h>

#include <lunaix/isrm.h>
#include <lunaix/spike.h>

/*
    total: 256 ivs
    0~31: reserved for sys use (x32)
    32~47: reserved for os use (x16)
    48~  : free to allocate for external hardware use. (x208)
*/

static char iv_bmp[(IV_MAX - IV_BASE) / 8];
static isr_cb handlers[IV_MAX];

extern void
intr_routine_fallback(const isr_param* param);

void
isrm_init()
{
    for (size_t i = 0; i < 256; i++) {
        handlers[i] = intr_routine_fallback;
    }
}

static inline int
__ivalloc_within(size_t a, size_t b, isr_cb handler)
{
    a = (a - IV_BASE) / 8;
    b = (b - IV_BASE) / 8;
    for (size_t i = a; i < b; i++) {
        char chunk = iv_bmp[i], j = 0;
        if (chunk == 0xff)
            continue;
        while ((chunk & 0x1)) {
            chunk >>= 1;
            j++;
        }
        iv_bmp[i] |= 1 << j;
        int iv = IV_BASE + i * 8 + j;
        handlers[iv] = handler ? handler : intr_routine_fallback;
        return iv;
    }
    return 0;
}

int
isrm_ivosalloc(isr_cb handler)
{
    return __ivalloc_within(IV_BASE, IV_EX, handler);
}

int
isrm_ivexalloc(isr_cb handler)
{
    return __ivalloc_within(IV_EX, IV_MAX, handler);
}

void
isrm_ivfree(int iv)
{
    assert(iv < 256);
    if (iv >= IV_BASE) {
        iv_bmp[(iv - IV_BASE) / 8] &= ~(1 << ((iv - IV_BASE) % 8));
    }
    handlers[iv] = intr_routine_fallback;
}

int
isrm_bindirq(int irq, isr_cb irq_handler)
{
    int iv;
    if (!(iv = isrm_ivexalloc(irq_handler))) {
        panickf("out of IV resource. (irq=%d)", irq);
        return 0; // never reach
    }

    // PC_AT_IRQ_RTC -> RTC_TIMER_IV, fixed, edge trigged, polarity=high,
    // physical, APIC ID 0
    ioapic_redirect(acpi_gistranslate(irq), iv, 0, IOAPIC_DELMOD_FIXED);
    return iv;
}

int
isrm_bindiv(int iv, isr_cb handler)
{
    assert(iv < 256);
    if (iv >= IV_BASE) {
        iv_bmp[(iv - IV_BASE) / 8] |= 1 << ((iv - IV_BASE) % 8);
    }
    handlers[iv] = handler;
}

isr_cb
isrm_get(int iv)
{
    assert(iv < 256);
    return handlers[iv];
}