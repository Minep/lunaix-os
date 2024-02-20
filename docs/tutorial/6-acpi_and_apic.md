## 准备工作

```sh
git checkout 287a5f7ae6a3bec3d679a5de394e915b56c7367d
```

观看对应视频。本次涉及到四个视频，内容很多。建议反复观看加深印象。

## 源码分析

### 新增代码

我们可以看到kernel/k_init.c的`_kernel_pre_init`添加了`intr_routine_init();`、`rtc_init();`，`_kernel_post_init`添加了`acpi_init`、`apic_init();`、`ioapic_init();`、`timer_init(SYS_TIMER_FREQUENCY_HZ);`。本次就主要分析这些代码。

### kernel/asm/x86/interrupt.S

lunaix-os的中断处理有了一些改进。来看看kernel/asm/x86/interrupt.S里面的`interrupt_wrapper`。

```assembly
    interrupt_wrapper:
        pushl %esp
        pushl %esi
        pushl %ebp
        pushl %edi
        pushl %edx
        pushl %ecx
        pushl %ebx
        pushl %eax

        movl %esp, %eax
        andl $0xfffffff0, %esp
        subl $16, %esp
        movl %eax, (%esp)

        call intr_handler
        popl %esp

        popl %eax
        popl %ebx
        popl %ecx
        popl %edx
        popl %edi
        popl %ebp
        popl %esi
        popl %esp

        addl $8, %esp

        iret
```

首先保存寄存器，然后继续栈对齐，接着调用`intr_handler`，最后恢复栈从而`iret`。注意interrupt_wrapper第一条指令前的栈是push了error code的中断栈。上面代码是在中断栈的顶上来保持寄存器的。所以pop出保存的寄存器后就可以iret。

`intr_handler`参数是`isr_param`类型，根据默认`cdecl`调用约定，参数由调用者布置到栈上。所以`param`指向的是之前push的寄存器值和中断栈内容。

```c
typedef struct {
    gp_regs registers;
    unsigned int vector;
    unsigned int err_code;
    unsigned int eip;
    unsigned int cs;
    unsigned int eflags;
    unsigned int esp;
    unsigned int ss;
} __attribute__((packed)) isr_param;

typedef struct
{
    reg32 eax;
    reg32 ebx;
    reg32 ecx;
    reg32 edx;
    reg32 edi;
    reg32 ebp;
    reg32 esi;
    reg32 esp;
} __attribute__((packed)) gp_regs;
```

### kernel/asm/x86/interrupts.c

这个函数会根据vector来调用不同中断处理函数。这样改动的好处是可以切换同一个中断号的中断处理函数。最后是`apic`的特殊处理，见注释。

```c
void
intr_handler(isr_param* param)
{
    if (param->vector <= 255) {
        int_subscriber subscriber = subscribers[param->vector];
        if (subscriber) {
            subscriber(param);
            goto done;
        }
    }

    if (fallback) {
        fallback(param);
        goto done;
    }
    
    kprint_panic("INT %u: (%x) [%p: %p] Unknown",
            param->vector,
            param->err_code,
            param->cs,
            param->eip);

done:
    // for all external interrupts except the spurious interrupt
    //  this is required by Intel Manual Vol.3A, section 10.8.1 & 10.8.5
    if (param->vector >= EX_INTERRUPT_BEGIN && param->vector != APIC_SPIV_IV) {
        apic_done_servicing();
    }
    return;
}
```

`intr_routine_init`就是用来注册不同vector对应函数的。

```c
void
intr_routine_init() 
{
    intr_subscribe(FAULT_DIVISION_ERROR,     intr_routine_divide_zero);
    intr_subscribe(FAULT_GENERAL_PROTECTION, intr_routine_general_protection);
    intr_subscribe(FAULT_PAGE_FAULT,         intr_routine_page_fault);
    intr_subscribe(LUNAIX_SYS_PANIC,         intr_routine_sys_panic);
    intr_subscribe(APIC_SPIV_IV,             intr_routine_apic_spi);
    intr_subscribe(APIC_ERROR_IV,            intr_routine_apic_error);

    intr_set_fallback_handler(intr_set_fallback_handler);
}

void
intr_subscribe(const uint8_t vector, int_subscriber subscriber) {
    subscribers[vector] = subscriber;
}
```

现在可以写一个中断的小结。如果我们要添加新的中断处理，需要

1、写好中断处理函数，参数为`const isr_param* param`。因为subscribers是类型`typedef void (*int_subscriber)(isr_param*);`的数组

2、在`intr_routine_init`注册

3、`isr_template xxx_xxx`利用模板来push error code

### rtc_init

下面实现了RTC寄存器的读写操作，本质是对端口的读写操作。

```c
uint8_t
rtc_read_reg(uint8_t reg_selector)
{
    io_outb(RTC_INDEX_PORT, reg_selector);
    return io_inb(RTC_TARGET_PORT);
}

void
rtc_write_reg(uint8_t reg_selector, uint8_t val)
{
    io_outb(RTC_INDEX_PORT, reg_selector);
    io_outb(RTC_TARGET_PORT, val);
}

static inline void
io_outb(int port, uint8_t data)
{
    asm volatile("outb %0, %w1" : : "a"(data), "d"(port));
}

static inline uint8_t
io_inb(int port)
{
    uint8_t data;
    asm volatile("inb %w1,%0" : "=a"(data) : "d"(port));
    return data;
}
```

根据资料

> The RTC contains two sets of indexed registers that are accessed using the two
> separate Index and Target registers (70h/71h or 72h/73h)[1]

寄存器的端口要通过基址和偏移获得。rtc_read_reg会先往端口`RTC_INDEX_PORT`（0x70）写入想操作的寄存器偏移（reg_selector），再读RTC_TARGET_PORT（0x71）。这样才能读到寄存器的值。这样设计可能是为了节省端口数量。如果一个寄存器占用一个端口就太浪费了。Register A实际上可分为8个bits。其中低四位bits可控制频率[2]。WITH_NMI_DISABLED视频也说了，是因为某些遗留问题。RTC_FREQUENCY_1024HZ用于控制频率。RTC_DIVIDER_33KHZ用于频率计算。具体可查看mc146818a(cmos-rtc)。

```c
void
rtc_init() {
    uint8_t regA = rtc_read_reg(RTC_REG_A | WITH_NMI_DISABLED);
    regA = (regA & ~0x7f) | RTC_FREQUENCY_1024HZ | RTC_DIVIDER_33KHZ;
    rtc_write_reg(RTC_REG_A | WITH_NMI_DISABLED, regA);

    // Make sure the rtc timer is disabled by default
    rtc_disable_timer();
}
```

关闭timer也是一个位操作。可以看资料里面寄存器B的结构。

```c
void
rtc_disable_timer() {
    uint8_t regB = rtc_read_reg(RTC_REG_B | WITH_NMI_DISABLED);
    rtc_write_reg(RTC_REG_B | WITH_NMI_DISABLED, regB & ~RTC_TIMER_ON);
}
```

### acpi_init

这个函数用于找到`ACPI`信息，保存信息到结构体`acpi_context`。

```c
int
acpi_init(multiboot_info_t* mb_info)
{
    acpi_rsdp_t* rsdp = acpi_locate_rsdp(mb_info);

    assert_msg(rsdp, "Fail to locate ACPI_RSDP");
    assert_msg(acpi_rsdp_validate(rsdp), "Invalid ACPI_RSDP (checksum failed)");

    kprintf(KINFO "RSDP found at %p, RSDT: %p\n", rsdp, rsdp->rsdt);

    acpi_rsdt_t* rsdt = rsdp->rsdt;
```

对于`acpi_rsdp_t`和`acpi_rsdt_t`可以在一些网站上找到它们的结构[3]。

**RSDP Structure**第一个字段是`Signature`，即字符串`"RSD PTR"`。因为之前低地址对等映射了，我们可以在低1MiB的空间暴力搜索这个字符串。如果找到了，那么大概了说明找到了RSDP。

还要通过acpi_rsdp_validate进行验证是否有效。这里无效的话可以再往后搜索，不过这种情况概率很小。

视频中提到virtual box启动内核时，RSDP会存储在0xe0000，上面网站有提到。

> #### 5.2.5.1. Finding the **RSDP** on IA-PC Systems
>
> ...
>
> - The BIOS read-only memory space between 0E0000h and 0FFFFFh.

复制信息到`acpi_context`

```c
	toc = lxcalloc(1, sizeof(acpi_context));
    assert_msg(toc, "Fail to create ACPI context");

    strncpy(toc->oem_id, rsdt->header.oem_id, 6);
    toc->oem_id[6] = '\0';
```

`acpi_sdthdr_t`、`acpi_madt_t`的结构也能从上面的网站找到。

```c
size_t entry_n = (rsdt->header.length - sizeof(acpi_sdthdr_t)) >> 2;
    for (size_t i = 0; i < entry_n; i++) {
        acpi_sdthdr_t* sdthdr = ((acpi_apic_t**)&(rsdt->entry))[i];
        switch (sdthdr->signature) {
            case ACPI_MADT_SIG:
                madt_parse((acpi_madt_t*)sdthdr, toc);
                break;
            default:
                break;
        }
    }
```

保存MADT中的APIC信息到context。简单来说就是保存acpi_apic_t、acpi_ioapic_t、acpi_intso_t这三个结构体信息。

```c
madt_parse((acpi_madt_t*)sdthdr, toc);
```

打印保存的信息

```c
    kprintf(KINFO "OEM: %s\n", toc->oem_id);
    kprintf(KINFO "IOAPIC address: %p\n", toc->madt.ioapic->ioapic_addr);
    kprintf(KINFO "APIC address: %p\n", toc->madt.apic_addr);

    for (size_t i = 0; i < 24; i++) {
        acpi_intso_t* intso = toc->madt.irq_exception[i];
        if (!intso)
            continue;

        kprintf(KINFO "IRQ #%u -> GSI #%u\n", intso->source, intso->gsi);
    }
```

标记为占用，再映射一下地址。

```c
acpi_init(_k_init_mb_info);
    uintptr_t ioapic_addr = acpi_get_context()->madt.ioapic->ioapic_addr;

    pmm_mark_page_occupied(FLOOR(__APIC_BASE_PADDR, PG_SIZE_BITS));
    pmm_mark_page_occupied(FLOOR(ioapic_addr, PG_SIZE_BITS));

    vmm_set_mapping(APIC_BASE_VADDR, __APIC_BASE_PADDR, PG_PREM_RW);
    vmm_set_mapping(IOAPIC_BASE_VADDR, ioapic_addr, PG_PREM_RW);
```

### apic_init

寄存器操作需要基址加偏移

```c
#define apic_read_reg(reg)           (*(uint32_t*)(APIC_BASE_VADDR + (reg)))
#define apic_write_reg(reg, val)     (*(uint32_t*)(APIC_BASE_VADDR + (reg)) = (val))
```

基址的物理地址是0xFEE00000，见Intel手册Figure 10-8. Local Vector Table (LVT)中右下角[4]。

```C
#define APIC_BASE_VADDR 0x1000
#define __APIC_BASE_PADDR 0xFEE00000
```

该函数的具体操作见视频**7.1 外中断与APIC（P1）**讲解。

### ioapic_init

初始化后RTC_TIMER_IV这个vector属于timer了，这里要用到之前保存的acpi_ctx。

```c
void
ioapic_init() {
    // Remapping the IRQs
    
    acpi_context* acpi_ctx = acpi_get_context();

    // Remap the IRQ 8 (rtc timer's vector) to RTC_TIMER_IV in ioapic
    //       (Remarks IRQ 8 is pin INTIN8)
    //       See IBM PC/AT Technical Reference 1-10 for old RTC IRQ
    //       See Intel's Multiprocessor Specification for IRQ - IOAPIC INTIN mapping config.
    
    // The ioapic_get_irq is to make sure we capture those overriden IRQs

    // PC_AT_IRQ_RTC -> RTC_TIMER_IV, fixed, edge trigged, polarity=high, physical, APIC ID 0
    ioapic_redirect(ioapic_get_irq(acpi_ctx, PC_AT_IRQ_RTC), RTC_TIMER_IV, 0, IOAPIC_DELMOD_FIXED);
}
```

### timer_init

接下来大概看看timer相关结构体。

```c
struct lx_timer_context {
    struct lx_timer *active_timers;
    uint32_t base_frequency;
    uint32_t running_frequency;
    uint32_t tick_interval;
};

struct lx_timer {
    struct llist_header link;
    uint32_t deadline;
    uint32_t counter;
    void* payload;
    void (*callback)(void*);
    uint8_t flags;
};
```

`lx_timer_context`可以链接`lx_timer`，`lx_timer`可以通过`llist_header`链接其他`lx_timer`。每个`lx_timer`有一个回调函数。

`timer_init_context`会动态分配`lx_timer_context`，然后挂一个`lx_timer`

```c
void
timer_init(uint32_t frequency)
{
    timer_init_context();
    //...
    
void
timer_init_context()
{
    timer_ctx =
      (struct lx_timer_context*)lxmalloc(sizeof(struct lx_timer_context));

    assert_msg(timer_ctx, "Fail to initialize timer contex");

    timer_ctx->active_timers =
      (struct lx_timer*)lxmalloc(sizeof(struct lx_timer));
    llist_init_head(timer_ctx->active_timers);
}
```

接下来配置timer。LVT的Timer结构见Figure 10-8. Local Vector Table (LVT)[4]上面部分。APIC_TIMER_DIV64见Figure 10-10. Divide Configuration Register[5]，作用在视频中讲过。

> In one-shot mode, the
> timer is started by programming its initial-count register. The initial count value is then copied into the current-
> count register and count-down begins. After the timer reaches zero, a timer interrupt is generated and the timer
> remains at its 0 value until reprogrammed.[6]

```c
	cpu_disable_interrupt();

    // Setup APIC timer

    // Setup a one-shot timer, we will use this to measure the bus speed. So we
    // can
    //   then calibrate apic timer to work at *nearly* accurate hz
    apic_write_reg(APIC_TIMER_LVT,
                   LVT_ENTRY_TIMER(APIC_TIMER_IV, LVT_TIMER_ONESHOT));

    // Set divider to 64
    apic_write_reg(APIC_TIMER_DCR, APIC_TIMER_DIV64);
```

接下来是计算和保存timer的频率。`temp_intr_routine_apic_timer`、`temp_intr_routine_rtc_tick`需要单独分析。

```c
    timer_ctx->base_frequency = 0;
    rtc_counter = 0;
    apic_timer_done = 0;

    intr_subscribe(APIC_TIMER_IV, temp_intr_routine_apic_timer);
    intr_subscribe(RTC_TIMER_IV, temp_intr_routine_rtc_tick);

    rtc_enable_timer();                                     	// start RTC timer
    apic_write_reg(APIC_TIMER_ICR, APIC_CALIBRATION_CONST); 	// start APIC timer

    // enable interrupt, just for our RTC start ticking!
    cpu_enable_interrupt();

    wait_until(apic_timer_done);

    // cpu_disable_interrupt();

    assert_msg(timer_ctx->base_frequency, "Fail to initialize timer (NOFREQ)");

    kprintf(KINFO "Base frequency: %u Hz\n", timer_ctx->base_frequency);

    timer_ctx->running_frequency = frequency;
    timer_ctx->tick_interval = timer_ctx->base_frequency / frequency;
```

取消注册

```c
    // cleanup
    intr_unsubscribe(APIC_TIMER_IV, temp_intr_routine_apic_timer);
    intr_unsubscribe(RTC_TIMER_IV, temp_intr_routine_rtc_tick);
```

LVT_TIMER_PERIODIC作用是可以周期性地产生中断。LVT_TIMER_ONESHOT是一次性的。

> In periodic mode, the timer is started by writing to the initial-count register (as in one-shot mode), and the value
> written is copied into the current-count register, which counts down. The current-count register is automatically
> reloaded from the initial-count register when the count reaches 0 and a timer interrupt is generated, and the count-
> down is repeated. If during the count-down process the initial-count register is set, counting will restart, using the
> new initial-count value. The initial-count register is a read-write register; the current-count register is read only.[6]

`APIC_TIMER_IV`的handler换成`timer_update`。

```c
    apic_write_reg(APIC_TIMER_LVT,
                   LVT_ENTRY_TIMER(APIC_TIMER_IV, LVT_TIMER_PERIODIC));
    intr_subscribe(APIC_TIMER_IV, timer_update);
```

设置timer触发中断所需要的tick次数。initial-count register结构见Figure 10-11. Initial Count and Current Count Registers[7]。

```c
apic_write_reg(APIC_TIMER_ICR, timer_ctx->tick_interval);
```

### 测量CPU时钟频率

`temp_intr_routine_apic_timer`、`temp_intr_routine_rtc_tick`

```c
    intr_subscribe(APIC_TIMER_IV, temp_intr_routine_apic_timer);
    intr_subscribe(RTC_TIMER_IV, temp_intr_routine_rtc_tick);
```

根据资料，为了保证timer中断被处理，要读取Register C一次，才能清除里面的值，才能发生下一次中断。发生一次中断`temp_intr_routine_rtc_tick`会计数一次。希望读者遇到疑问后能通过查文档解决它。

> All bits which are high when read by the program are cleared, and new interrupts (on any bits) are held after the read cycle.[8]

```c
static void
temp_intr_routine_rtc_tick(const isr_param* param)
{
    rtc_counter++;

    // dummy read on register C so RTC can send anther interrupt
    //  This strange behaviour observed in virtual box & bochs
    (void)rtc_read_reg(RTC_REG_C);
}
```

手动测量需要一个固定且已知频率时钟（RTC Timer）作为参考。用RTC Timer来推测LAPIC Timer。

开启测量后，ICR会随CPU Timer tick一次减小一次，`ICR`减到0后，`APIC_TIMER_IV`触发。这时获得RTC Timer中断次数`rtc_counter`。我们知道RTC Timer中断一次所需时间`RTC_TIMER_BASE_FREQUENCY`，那么就能知道CPU Timer tick ICR 次所花时间。

`temp_intr_routine_apic_timer`用于计算CPU Timer每秒tick次数timer_ctx->base_frequency。

```c
static void
temp_intr_routine_apic_timer(const isr_param* param)
{
    timer_ctx->base_frequency =
      APIC_CALIBRATION_CONST / rtc_counter * RTC_TIMER_BASE_FREQUENCY;
    apic_timer_done = 1;

    rtc_disable_timer();
}
```

如果我们把`timer_ctx->base_frequency`写入到`APIC_TIMER_ICR`，就会一秒一次中断。但是我们要设置一秒`SYS_TIMER_FREQUENCY_HZ`次中断。

```c
timer_init(SYS_TIMER_FREQUENCY_HZ);
```

所以就要写入`timer_ctx->tick_interval`到`APIC_TIMER_ICR`。

```c
    timer_ctx->running_frequency = frequency;
    timer_ctx->tick_interval = timer_ctx->base_frequency / frequency;
```

### timer

`timer_run_second`可以每秒运行一次callback。如果我们callback是获取并打印时间，那么就能实现时间显示的功能。相信相关代码读者有能力分析。

```c
int
timer_run_second(uint32_t second, void (*callback)(void*), void* payload, uint8_t flags)
{
    return timer_run(second * timer_ctx->running_frequency, callback, payload, flags);
}
```

`timer_run`把callback链接到timer_ctx->active_timers，ticks用于控制callback执行频率。

```c
int
timer_run(uint32_t ticks, void (*callback)(void*), void* payload, uint8_t flags)
{
    struct lx_timer* timer = (struct lx_timer*)lxmalloc(sizeof(struct lx_timer));

    if (!timer) return 0;

    timer->callback = callback;
    timer->counter = ticks;
    timer->deadline = ticks;
    timer->payload = payload;
    timer->flags = flags;

    llist_append(timer_ctx->active_timers, &timer->link);

    return 1;
}
```

`llist_for_each`宏读者可以自己看看。`timer_update`是timer中断handler。

`pos->counter`表示花多少tick执行一次。`pos->flags`设置这个callback是一次性的。

```c
static void
timer_update(const isr_param* param)
{
    struct lx_timer *pos, *n;
    struct lx_timer* timer_list_head = timer_ctx->active_timers;

    llist_for_each(pos, n, &timer_list_head->link, link)
    {
        if (--pos->counter) {
            continue;
        }

        pos->callback ? pos->callback(pos->payload) : 1;

        if (pos->flags & TIMER_MODE_PERIODIC) {
            pos->counter = pos->deadline;
        } else {
            llist_delete(&pos->link);
            lxfree(pos);
        }
    }
}
```

## 参考

[1]intel-500-pch, 31.1 RTC Indexed Registers Summary

[2]mc146818a(cmos-rtc), Table 5

[3]https://uefi.org/htmlspecs/ACPI_Spec_6_4_html/05_ACPI_Software_Programming_Model/ACPI_Software_Programming_Model.html?highlight=rsdp#root-system-description-pointer-rsdp-structure

[4]Intel Manual,Vol 3A, 10-13, Figure 10-8. Local Vector Table (LVT)

[5]Intel Manual, Vol. 3A, 10-17, Figure 10-10. Divide Configuration Register

[6]Intel Manual, Vol. 3A, 10-16, 10.5.4 APIC Timer

[7]Intel Manual, Vol. 3A, 10-17, Figure 10-11. Initial Count and Current Count Registers

[8]intel-500-pch, p12, INTERRUPTS
