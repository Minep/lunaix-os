#include <lunaix/process.h>
#include <lunaix/pcontext.h>
#include <lunaix/mm/vmm.h>
#include <klibc/string.h>

#include <sys/mm/mempart.h>
#include <sys/abi.h>

volatile struct x86_tss _tss = { .link = 0,
                                 .esp0 = 0,
                                 .ss0 = KDATA_SEG };

bool
inject_transfer_context(ptr_t vm_mnt, struct transfer_context* tctx)
{
    pte_t pte;
    if (!vmm_lookupat(vm_mnt, tctx->inject, &pte)) {
        return false;
    }

    mount_page(PG_MOUNT_4, pte_paddr(pte));

    ptr_t mount_inject = PG_MOUNT_4 + va_offset(tctx->inject);
    memcpy((void*)mount_inject, &tctx->transfer, sizeof(tctx->transfer));
    
    unmount_page(PG_MOUNT_4);
    return true;
}

void
thread_setup_trasnfer(struct transfer_context* tctx, 
                      ptr_t kstack_tp, ptr_t ustack_pt, 
                      ptr_t entry, bool to_user) 
{
    ptr_t offset = (ptr_t)&tctx->transfer.eret - (ptr_t)&tctx->transfer;
    tctx->inject = align_stack(kstack_tp - sizeof(tctx->transfer));

    tctx->transfer.isr = (isr_param){ 
                            .registers = { 
                                .ds = KDATA_SEG,
                                .es = KDATA_SEG,
                                .fs = KDATA_SEG,
                                .gs = KDATA_SEG 
                            },
                            .execp = (struct exec_param*)(tctx->inject + offset)
                        };
    
    int code_seg = KCODE_SEG, data_seg = KDATA_SEG;
    int mstate = cpu_ldstate();
    if (to_user) {
        code_seg = UCODE_SEG, data_seg = UDATA_SEG;
        mstate |= 0x200;   // enable interrupt
    }

    tctx->transfer.eret = (struct exec_param) {
                            .cs = code_seg, .eip = entry, 
                            .ss = data_seg, .esp = align_stack(ustack_pt),
                            .eflags = mstate
                        };
}