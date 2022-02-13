#pragma pack(push, 1)
typedef struct {
    unsigned int vector;
    unsigned int err_code;
    unsigned int eip;
    unsigned short cs;
    unsigned int eflags;
} isr_param;
#pragma pack(pop)

void
_asm_isr0();

void
interrupt_handler(isr_param* param);