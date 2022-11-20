#include <cpuid.h>
#include <hal/cpu.h>
#include <hal/rnd.h>
#include <stdint.h>

void
cpu_get_model(char* model_out)
{
    u32_t* out = (u32_t*)model_out;
    reg32 eax = 0, ebx = 0, edx = 0, ecx = 0;

    __get_cpuid(0, &eax, &ebx, &ecx, &edx);

    out[0] = ebx;
    out[1] = edx;
    out[2] = ecx;
    model_out[12] = '\0';
}

#define BRAND_LEAF 0x80000000UL

int
cpu_brand_string_supported()
{
    reg32 supported = __get_cpuid_max(BRAND_LEAF, 0);
    return (supported >= 0x80000004UL);
}

void
cpu_get_brand(char* brand_out)
{
    if (!cpu_brand_string_supported()) {
        brand_out[0] = '?';
        brand_out[1] = '\0';
    }
    u32_t* out = (u32_t*)brand_out;
    reg32 eax = 0, ebx = 0, edx = 0, ecx = 0;
    for (u32_t i = 2, j = 0; i < 5; i++) {
        __get_cpuid(BRAND_LEAF + i, &eax, &ebx, &ecx, &edx);
        out[j] = eax;
        out[j + 1] = ebx;
        out[j + 2] = ecx;
        out[j + 3] = edx;
        j += 4;
    }
    brand_out[48] = '\0';
}

int
cpu_has_apic()
{
    // reference: Intel manual, section 10.4.2
    reg32 eax = 0, ebx = 0, edx = 0, ecx = 0;
    __get_cpuid(1, &eax, &ebx, &ecx, &edx);

    return (edx & 0x100);
}

void
cpu_rdmsr(u32_t msr_idx, u32_t* reg_high, u32_t* reg_low)
{
    u32_t h = 0, l = 0;
    asm volatile("rdmsr" : "=d"(h), "=a"(l) : "c"(msr_idx));

    *reg_high = h;
    *reg_low = l;
}

void
cpu_wrmsr(u32_t msr_idx, u32_t reg_high, u32_t reg_low)
{
    asm volatile("wrmsr" : : "d"(reg_high), "a"(reg_low), "c"(msr_idx));
}

int
rnd_is_supported()
{
    reg32 eax, ebx, ecx, edx;
    __get_cpuid(0x01, &eax, &ebx, &ecx, &edx);
    return (ecx & (1 << 30));
}