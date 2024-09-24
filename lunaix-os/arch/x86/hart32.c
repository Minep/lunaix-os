#ifdef CONFIG_ARCH_I386

#include <lunaix/process.h>
#include <lunaix/hart_state.h>
#include <lunaix/mm/vmm.h>
#include <klibc/string.h>

#include <asm/mempart.h>
#include <asm/abi.h>

volatile struct x86_tss _tss = { .link = 0,
                                 .esp0 = 0,
                                 .ss0 = KDATA_SEG };

void
hart_prepare_transition(struct hart_transition* ht, 
                      ptr_t kstack_tp, ptr_t ustack_pt, 
                      ptr_t entry, bool to_user) 
{
    ptr_t offset = (ptr_t)&ht->transfer.eret - (ptr_t)&ht->transfer;
    ht->inject = align_stack(kstack_tp - sizeof(ht->transfer));

    ht->transfer.state = (struct hart_state){ 
                            .registers = { 
                                .ds = KDATA_SEG,
                                .es = KDATA_SEG,
                                .fs = KDATA_SEG,
                                .gs = KDATA_SEG 
                            },
                            .execp = (struct exec_param*)(ht->inject + offset)
                        };
    
    int code_seg = KCODE_SEG, data_seg = KDATA_SEG;
    int mstate = cpu_ldstate();
    if (to_user) {
        code_seg = UCODE_SEG, data_seg = UDATA_SEG;
        mstate |= 0x200;   // enable interrupt
    }

    ht->transfer.eret = (struct exec_param) {
                            .cs = code_seg, .eip = entry, 
                            .ss = data_seg, .esp = align_stack(ustack_pt),
                            .eflags = mstate
                        };
}

#endif