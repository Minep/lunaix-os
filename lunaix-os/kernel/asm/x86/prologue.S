/* 高半核入口点 - 0xC0000000 */

.section .text
    .global hhk_entry_
    hhk_entry_:
        /*
            欢迎来到虚拟内存的世界！ :D
         */

        subl $16, %esp
        /* 
            最终还是决定将IDT&GDT的初始化和安装放在这里
            注意：由于已开启分页，GDTR与IDTR里面放的不是物理地址，是线性地址！ 
        */
        /* 
            加载 GDT 
            P.s. 虽然GDT在分页后已变得不重要，甚至可以忽略不作。但为了保持完整性，还是选择加载他
                    这主要是为了保险起见，让GDTR有一个合法的值，否则多咱的粗心大意，容易出#GP
        */
        call _init_gdt
        movl $_gdt, 2(%esp)
        movw _gdt_limit, %ax
        movw %ax, (%esp)
        lgdt (%esp)

        /* 更新段寄存器 */
        movw $0x10, %cx
        movw %cx, %es
        movw %cx, %ds
        movw %cx, %fs
        movw %cx, %gs
        movw %cx, %ss

        /* 更新 CS:EIP */
        pushw $0x08
        pushl $_after_gdt
        retf

    _after_gdt:

        movl $mb_info, (%esp)
        call _kernel_init

        /* 
            加载新的栈指针，位于0xffbfffff，但因为16字节对齐的需求，低四位清零。
            为什么不是0xffffffff？ 因为0xffc00000 - 0xffffffff 这4MiB的空间用于
            对页表与页目录的循环映射。
        */
        mov $0xffbffff0, %esp

        call _kernel_post_init

        subl $6, %esp

        # 加载 IDT
        movl $_idt, 2(%esp)
        movw _idt_limit, %ax
        movw %ax, (%esp)
        lidt (%esp)

        addl $6, %esp

        /* 进入内核 */  
        call _kernel_main

        cli
    j_:
        hlt
        jmp j_