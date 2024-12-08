#include <lunaix/spike.h>
#include <asm/x86_isrm.h>

#include "asm/x86.h"
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