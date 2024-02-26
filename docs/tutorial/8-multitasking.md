## 准备工作

```sh
git checkout 088403ac98acf7991507715d29a282dcba222053
```

观看对应视频

### 实现多进程的要素

连续性：记录切换进程的CS:IP

正确性：记录进程的EFLAGS、各种寄存器、栈、堆、程序本身

公平性：时间分配合理

在正确性中：栈、堆、程序本身和页表关联，进程的EFLAGS、各种寄存器和中断上下文关联

### 中断上下文

根据Intel IA32手册[1]，有两种情况

第一种是特权级不变，也就是内核代码切换到内核，或者Ring3用户代码切换到Ring3

栈上由高到低存放EFLAGS、CS、EIP、ERROR CODE，并且ESP指向ERROR CODE

lunaix使用中断来进行进程切换，那么看看怎么处理栈

中断程序如下，注：中断程序执行第一句时，栈空间已经是上述状态

可以看到，这个代码不管有没有权限改变都把除了ss的段寄存器push入栈（cs已经入栈了）

```assembly
        pushl %esp

        subl $16, %esp
        movw %gs, 12(%esp)
        movw %fs,  8(%esp)
        movw %es,  4(%esp)
        movw %ds,   (%esp)

        pushl %esi
        pushl %ebp
        pushl %edi
        pushl %edx
        pushl %ecx
        pushl %ebx
        pushl %eax
```

下面这段代码则是通过60(%esp)取出cs，假设为0x10

```assembly
        movl 60(%esp), %eax
        andl $0x3, %eax
        jz somewheref
```

`eax`与0x3可以获得第2号段描述符，实际上设置了第二号为ring0代码段，所以这样可以判断是不是发生了权限切换

如果从ring3到ring0，则把ring0相关段地址填充一下

```assembly
        movw $segment, %ax
        movw %ax, %gs
        movw %ax, %fs
        movw %ax, %ds
        movw %ax, %es
```

### 中断函数派发

子函数其实是一个dispatcher，派发到不同的函数

例如我们派发到系统调用函数

这个函数又会根据栈上的eax的值，即陷入前根据eax的值来判断调用哪个系统调用

这样我们可以用变化的eax和一个中断向量号来执行多个系统调用（当然可以通过其他寄存器传参数，反正都保存在栈上了）

### fork函数实现

这是一个非常经典的函数

还是介绍一下它：fork用于创建一个进程，所创建的进程复制父进程的代码段/数据段/BSS段/堆/栈等所有用户空间信息；在内核中操作系统重新为其申请了一个PCB，并使用父进程的PCB进行初始化。

先看看第一部分代码

```c
struct proc_info {
    pid_t pid;
    struct proc_info* parent;
    isr_param intr_ctx;//存储中断上下文
    struct llist_header siblings;//与遍历兄弟姊妹有关
    struct llist_header children;//与遍历孩子进程有关
    struct proc_mm mm;//描述该进程的占用的虚拟内存范围和属性
    void* page_table;//指向页目录
    time_t created;//创建时的时间戳
    uint8_t state;
    int32_t exit_code;
    int32_t k_status;
    struct lx_timer* timer;//与时间控制有关
};
```

下面是fork实现的第一段，用于初始化pcb

```c
    struct proc_info pcb;
    init_proc(&pcb);
    pcb.mm = __current->mm;
    pcb.intr_ctx = __current->intr_ctx;
    pcb.parent = __current;
```

根据上面的描述，初始化就是复制父进程的代码段/数据段/BSS段/堆/栈、标记进程为创建状态等

```c
setup_proc_mem(&pcb, PD_REFERENCED);
```

PD_REFERENCED是页表虚拟地址

这个函数作用是复制页表相关操作，稍后分析

TODO:setup_proc_mem

```c
    llist_init_head(&pcb.mm.regions);
    struct mm_region *pos, *n;
    llist_for_each(pos, n, &__current->mm.regions->head, head)
    {
```

llish_for_each是一个遍历regions的宏，接下来就遍历__current的各种区域（regions也可视为一个链表）

```c
        region_add(&pcb, pos->start, pos->end, pos->attr);
```

这个就是复制了，不多说

```c
if ((pos->attr & REGION_WSHARED)) {
            continue;
        }
```

如果这个区域共享写，就没什么要处理的了

```c
        uintptr_t start_vpn = PG_ALIGN(pos->start) >> 12;
        uintptr_t end_vpn = PG_ALIGN(pos->end) >> 12;
        for (size_t i = start_vpn; i < end_vpn; i++) {
```

后面又是一个循环，用于遍历该区域，如果不是共享写的话

```c
            x86_pte_t* curproc = &PTE_MOUNTED(PD_MOUNT_1, i);
            x86_pte_t* newproc = &PTE_MOUNTED(PD_MOUNT_2, i);
            cpu_invplg(newproc);

            if (pos->attr == REGION_RSHARED) {
                cpu_invplg(curproc);
                *curproc = *curproc & ~PG_WRITE;
                *newproc = *newproc & ~PG_WRITE;
            } else {
                *newproc = 0;
            }
```

PTE_MOUNTED 这个宏意思就是获得页目录的第几号页表项

PD_MOUNT_1这个页表就是当前进程的地址

```c
            if (pos->attr == REGION_RSHARED) {
                cpu_invplg(curproc);
                *curproc = *curproc & ~PG_WRITE;[2]
                *newproc = *newproc & ~PG_WRITE;
```

如果是共享读，就根据Intel manual 把页表设置成Read

```c
            } else {
                *newproc = 0;
            }
```

否则视为私有，子进程不继承该页表，直接清空

上面这个阶段就是复制页表

最后看看TODO:setup_proc_mem(&pcb, PD_REFERENCED);

```c
void
setup_proc_mem(struct proc_info* proc, uintptr_t usedMnt)
{
    pid_t pid = proc->pid;
    void* pt_copy = __dup_pagetable(pid, usedMnt);
    vmm_mount_pd(PD_MOUNT_2, pt_copy);
    // copy the kernel stack
    for (size_t i = KSTACK_START >> 12; i <= KSTACK_TOP >> 12; i++) {
        volatile x86_pte_t* ppte = &PTE_MOUNTED(PD_MOUNT_2, i);
        cpu_invplg(ppte);
        x86_pte_t p = *ppte;
        void* ppa = vmm_dup_page(pid, PG_ENTRY_ADDR(p));
        *ppte = (p & 0xfff) | (uintptr_t)ppa;
    }
    proc->page_table = pt_copy;
}
```

__dup_pagetable(pid, usedMnt);这个函数是复制全部kernel页表

先不看，加为TODO:__dup_pagetable(pid, usedMnt);

vmm_mount_pd(PD_MOUNT_2, pt_copy);复制到二号页目录挂载点

TODO:`__dup_pagetable(pid, usedMnt);` `vmm_mount_pd(PD_MOUNT_2, pt_copy);`

`vmm_dup_page(pid, PG_ENTRY_ADDR(p));`这个也加为TODO，最后的循环暂且知道是处理了每个页目录项即可。

最后执行not cpoy段

```c
not_copy:
    vmm_unmount_pd(PD_MOUNT_2);
    pcb.intr_ctx.registers.eax = 0;
    push_process(&pcb);//把子进程放入执行队列，轮询制
    return pcb.pid;
```

### __dup_pagetable

解决第一个TODO

这个函数作用是复制全部kernel页表、页目录



```c
    void* ptd_pp = pmm_alloc_page(pid, PP_FGPERSIST);
```

先调用pmm_alloc_page给当前pid分配一个物理页，篇幅有限，就不说了，后面的一些函数又是如此



```c
    x86_page_table* ptd = vmm_fmap_page(pid, PG_MOUNT_1, ptd_pp, PG_PREM_RW);
    x86_page_table* pptd = (x86_page_table*)(mount_point | (0x3FF << 12));
```

把分配的物理页映射到这个挂载点，其实就是给挂载点分配物理空间，拿到页目录指针ptd

pptd指向kernel页目录第一项（这个kernel页表使用了循环引用，页目录最后一项指向页目录基址）



```c
    for (size_t i = 0; i < PG_MAX_ENTRIES - 1; i++) {
```

遍历所有page entry，除了最后一个

```c
    	x86_pte_t ptde = pptd->entry[i];
        if (!ptde || !(ptde & PG_PRESENT)) {
            ptd->entry[i] = ptde;
            continue;
        }
```

取出kernel page directory entry，简单判断并复制页目录的每一项

```c
    	x86_page_table* ppt = (x86_page_table*)(mount_point | (i << 12));
        void* pt_pp = pmm_alloc_page(pid, PP_FGPERSIST);
        x86_page_table* pt = vmm_fmap_page(pid, PG_MOUNT_2, pt_pp, PG_PREM_RW);
```

ppt指向 kernel page table entry，接着给二号页表挂载点分配物理页并映射

```c
        for (size_t j = 0; j < PG_MAX_ENTRIES; j++) {
            x86_pte_t pte = ppt->entry[j];
            pmm_ref_page(pid, PG_ENTRY_ADDR(pte));
            pt->entry[j] = pte;
        }
        ptd->entry[i] = (uintptr_t)pt_pp | PG_PREM_RW;
```

上面是这个循环中最后一段，复制页表，最后设置权限

pmm_ref_page(pid, PG_ENTRY_ADDR(pte));这个是增加页面引用计数，和内存共享有关



循环结束后

```c
    ptd->entry[PG_MAX_ENTRIES - 1] = NEW_L1_ENTRY(T_SELF_REF_PERM, ptd_pp);
    return ptd_pp;
```

同样做一个循环映射

返回挂载点1的虚拟地址

### vmm_mount_pd

回顾：它的第二个参数是页目录

```c
void* pt_copy = __dup_pagetable(pid, usedMnt);
    vmm_mount_pd(PD_MOUNT_2, pt_copy);
```

下面是它的代码

```c
void*
vmm_mount_pd(uintptr_t mnt, void* pde) {
    x86_page_table* l1pt = (x86_page_table*)L1_BASE_VADDR;
    l1pt->entry[(mnt >> 22)] = NEW_L1_ENTRY(T_SELF_REF_PERM, pde);
    cpu_invplg(mnt);
    return mnt;
}
```

很简单，就是把kernel的页目录指向新页目录，于是我们可以通过虚拟地址访问这个页目录了

### vmm_dup_page



先回顾一下：它是在复制kernel栈的情况下使用的

```c
    // copy the kernel stack
    for (size_t i = KSTACK_START >> 12; i <= KSTACK_TOP >> 12; i++) {
        volatile x86_pte_t* ppte = &PTE_MOUNTED(PD_MOUNT_2, i);
        cpu_invplg(ppte);
        x86_pte_t p = *ppte;
        void* ppa = vmm_dup_page(pid, PG_ENTRY_ADDR(p));
        *ppte = (p & 0xfff) | (uintptr_t)ppa;
    }
```



```c
void* vmm_dup_page(pid_t pid, void* pa) {    
    void* new_ppg = pmm_alloc_page(pid, 0);
    vmm_fmap_page(pid, PG_MOUNT_3, new_ppg, PG_PREM_RW);
    vmm_fmap_page(pid, PG_MOUNT_4, pa, PG_PREM_RW);

    asm volatile (
        "movl %1, %%edi\n"
        "movl %2, %%esi\n"
        "rep movsl\n"
        :: "c"(1024), "r"(PG_MOUNT_3), "r"(PG_MOUNT_4)
        : "memory", "%edi", "%esi");

    vmm_unset_mapping(PG_MOUNT_3);
    vmm_unset_mapping(PG_MOUNT_4);

    return new_ppg;
}
```

参数pa是页表项的值取高20位

最后实现 把pa指向页面值复制到新页面

其实实现的就是页面复制到新地址，并返回



### exit系统调用实现

先来看看exit的说明
void exit(int status) 立即终止调用进程。任何属于该进程的打开的文件描述符都会被关闭，该进程的子进程由进程 1 继承，初始化，且会向父进程发送一个 SIGCHLD 信号。

我们的exit还是有一些不一样的

exit定义如下，依旧是通过中断陷入TRAP
```c
static void _exit(int status) {
    // 使用汇编语句设置寄存器ebx的值为status
    asm("" :: "b"(status));
    int v;
    // 使用汇编语句触发一个中断，中断号为33（通常用于系统调用），功能号为8
    asm volatile("int %1\n" : "=a"(v) : "i"(33), "a"(8));
    // 返回中断处理后的结果
    return (void)v;
}
```
为什么要设置ebx的值为status呢，因为上一次我们的中断框架规定了ebx传递第一个参数

```c
terminate_proc(status);
void
terminate_proc(int exit_code)
{
    __current->state = PROC_TERMNAT;
    __current->exit_code = exit_code;
    schedule();
}
```
这个代码很简单，设置返回值为status和状态为终止，最后调用schedule

```c
    if (!sched_ctx.ptable_len) {
        return;
    }
```
在schedule函数中，首先检查进程表长度，如果为空就不需要schedule了

```c
    cpu_disable_interrupt();
    struct proc_info* next;
    int prev_ptr = sched_ctx.procs_index;
    int ptr = prev_ptr;

    do {
        ptr = (ptr + 1) % sched_ctx.ptable_len;
        next = &sched_ctx._procs[ptr];
    } while (next->state != PROC_STOPPED && ptr != prev_ptr);

    sched_ctx.procs_index = ptr;

    run(next);
```
如果不为空，则（以轮询算法的方式）获得下一个进程指针存入next，而且该进程不能为停止状态
更新index，最后run

```c
    if (!(__current->state & ~PROC_RUNNING)) {
        __current->state = PROC_STOPPED;
    }
    proc->state = PROC_RUNNING;
```
run函数也是先检查状态

```c
    if (__current->page_table != proc->page_table) {
        __current = proc;
        cpu_lcr3(__current->page_table);
        // from now on, the we are in the kstack of another process
    } else {
        __current = proc;
    }
```
更新__current的信息，然后更新CR3寄存器

```c
static inline void
cpu_lcr3(reg32 v)
{
    asm("mov %0, %%cr3" ::"r"(v));
}
```
更新CR3寄存器很简单

```c
    apic_done_servicing();
    asm volatile("pushl %0\n"
                 "jmp soft_iret\n" ::"r"(&__current->intr_ctx)
                 : "memory");
```
将当前中断上下文push，然后中断返回
我们在中断context中已经保存了需要切换的进程的信息

```c
    .global soft_iret
    soft_iret:
        popl %esp
```
先pop，也就是让esp等于(&__current->intr_ctx)

esp此时指向__current->intr_ctx这个结构体
回顾一下这个结构体
```c
typedef struct
{
    struct
    {
        reg32 eax;
        reg32 ebx;
        reg32 ecx;
        reg32 edx;
        reg32 edi;
        reg32 ebp;
        reg32 esi;
        reg32 ds;
        reg32 es;
        reg32 fs;
        reg32 gs;
        reg32 esp;
    } registers;
    unsigned int vector;
    unsigned int err_code;
    unsigned int eip;
    unsigned int cs;
    unsigned int eflags;
    unsigned int esp;
    unsigned int ss;
} __attribute__((packed)) isr_param;
```
就是一堆寄存器，继续看soft_iret代码

```c
        popl %eax
        popl %ebx
        popl %ecx
        popl %edx
        popl %edi
        popl %ebp
        popl %esi
```
这个就是把保存的寄存器都恢复出来

```c
        movw   (%esp), %ds
        movw  4(%esp), %es
        movw  8(%esp), %fs
        movw 12(%esp), %gs
```
恢复段寄存器

```c
        movl 16(%esp), %esp
        addl $8, %esp  
        iret
```
这个过程是和中断wrapper调用时完全相反的，不多说了

总的来说，exit会返回exit code 并且中断跳到sched_ctx._procs数组中的一个进程

那么，接下来看看这个数组有哪些相关操作

```c
void push_process(struct proc_info* process);
```
挑push_process来说，它的作用是把一个进程放入轮询调度器

之前实现了timer就能很快实现调度器了，略过

push_process内容是对proc_info内容做一些检查最后写入数组，就没了

最后看看push_process的使用

然后是创建0号进程的时候调用了push_process

那么看看为什么0号进程的创建和创建意义

### 0号进程创建及其意义
```c
    struct proc_info proc0;
    init_proc(&proc0);
    proc0.intr_ctx = (isr_param){ .registers = { .ds = KDATA_SEG,
                                                 .es = KDATA_SEG,
                                                 .fs = KDATA_SEG,
                                                 .gs = KDATA_SEG },
                                  .cs = KCODE_SEG,
                                  .eip = (void*)__proc0,
                                  .ss = KDATA_SEG,
                                  .eflags = cpu_reflags() };
```
上面这些是PCB的初始化，略过

```c
setup_proc_mem(&proc0, PD_REFERENCED);
```
复制内核的页目录到0号进程信息里面

```c
    asm volatile("movl %%cr3, %%eax\n"
                 "movl %%esp, %%ebx\n"
                 "movl %1, %%cr3\n"
                 "movl %2, %%esp\n"
                 "pushf\n"
                 "pushl %3\n"
                 "pushl %4\n"
                 "pushl $0\n"
                 "pushl $0\n"
                 "movl %%esp, %0\n"
                 "movl %%eax, %%cr3\n"
                 "movl %%ebx, %%esp\n"
                 : "=m"(proc0.intr_ctx.registers.esp)
                 : "r"(proc0.page_table),
                   "i"(KSTACK_TOP),
                   "i"(KCODE_SEG),
                   "r"(proc0.intr_ctx.eip)
                 : "%eax", "%ebx", "memory");
```
布置好中断栈，依次push eflags、代码段地址（cs）、eip、0（error code）、0（vector）

```c
    // 向调度器注册进程。
    push_process(&proc0);

    // 由于时钟中断未就绪，我们需要手动通知调度器进行第一次调度。这里也会同时隐式地恢复我们的eflags.IF位
    schedule();
```
最后进行注册和调度


### 进程调用过程细节
进程调用是由调度器管理
假设我们调用fork执行了一个进程，之后调度器触发了进程切换

```c

static void
timer_update(const isr_param* param)
{
    /*...*/
    sched_ticks_counter++;

    if (sched_ticks_counter >= sched_ticks) {
        sched_ticks_counter = 0;
        schedule();
    }
}
```
timer会每隔固定时间利用外部中断执行timer_update
timer_update函数会调用schedule

schedule也分析过了，它会取出其中一个PCB信息，并调用run函数
run函数上面也分析了
**就此进程管理大部分实现**

### sleep实现

```c
    if (!seconds) {
        return 0;
    }

    if (__current->timer) {
        return __current->timer->counter / timer_context()->running_frequency;
    }

    struct lx_timer* timer =
      timer_run_second(seconds, proc_timer_callback, __current, 0);
    __current->timer = timer;
    __current->intr_ctx.registers.eax = seconds;
    __current->state = PROC_BLOCKED;
    schedule();
```
主要逻辑就是每秒运行proc_timer_callback
同时设置阻塞状态等，最后调用schedule函数

```c
static void
proc_timer_callback(struct proc_info* proc)
{
    proc->timer = NULL;
    proc->state = PROC_STOPPED;
}
```
proc_timer_callback就是清空timer，设置为STOP

```c
    if (__current->timer) {
        return __current->timer->counter / timer_context()->running_frequency;
    }
```
sleep函数还有这一段没解释，当调用了proc_timer_callback，timer为空就不会返回

## 参考

[1]Intel Manual, Vol 1, 6-15, Figure 6-7. Stack Usage on Transfers to Interrupt and Exception Handling Routines

[2]Intel Manual, Vol 3A, 4-13, Table 4-6. Format of a 32-Bit Page-Table Entry that Maps a 4-KByte Page
