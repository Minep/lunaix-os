#ifdef CONFIG_ARCH_X86_64

#include <lunaix/process.h>
#include <lunaix/hart_state.h>
#include <lunaix/mm/vmm.h>
#include <klibc/string.h>

#include <sys/mm/mempart.h>
#include <sys/abi.h>

volatile struct x86_tss _tss = { };

void
hart_prepare_transition(struct hart_transition* ht, 
                      ptr_t kstack_tp, ptr_t ustack_pt, 
                      ptr_t entry, bool to_user) 
{
    ptr_t offset = (ptr_t)&ht->transfer.eret - (ptr_t)&ht->transfer;
    ht->inject = align_stack(kstack_tp - sizeof(ht->transfer));

    ht->transfer.state = (struct hart_state){ 
                            .registers = { },
                            .execp = (struct exec_param*)(ht->inject + offset)
                        };
    
    int code_seg = KCODE_SEG, data_seg = KDATA_SEG;
    int mstate = cpu_ldstate();
    if (to_user) {
        code_seg = UCODE_SEG, data_seg = UDATA_SEG;
        mstate |= 0x200;   // enable interrupt
    }

    ht->transfer.eret = (struct exec_param) {
                            .cs = code_seg, .rip = entry, 
                            .ss = data_seg, .rsp = align_stack(ustack_pt),
                            .rflags = mstate
                        };
}

#endif