## 准备工作

```sh
git checkout e141fd4dcd5effc2dbe59a498d7ea274b7199147
```

观看相关视频

## 代码分析

### libs/

这部分内容就比较多了

先来看看`printf`的实现

```c
void
printf(char* fmt, ...)
{
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    __sprintf_internal(buffer, fmt, args);
    va_end(args);

    // 啊哈，直接操纵framebuffer。日后须改进（FILE* ?）
    tty_put_str(buffer);
}
```

这个函数的参数是变长的，这里获得fmt后面的一个参数，然后传给__sprintf_internal进行了处理。可以推断，这里是一个个参数进行格式化处理的。而buffer就是格式化后的字符串存储的位置。

接下来看看参数是如何一个个放入buffer的。

循环fmt的每个字符

```c
    while ((c = *fmt)) {
```

如果遇到`%`，说明要进入格式化处理了，于是设置adv为1。

```c
        if (c != '%') {
            buffer[ptr] = c;
            adv = 1;
        } else {
```

如果我们的fmt是`hello %s`，那么接下来看看怎么处理`%`后面的`s`

```c
                case 's': {
                    char* str = va_arg(args, char*);
                    strcpy(buffer + ptr, str);
                    adv = strlen(str);
                    break;
                }
```

这里把args解析成一个字符指针，然后把str复制到buffer中。最后还用adv来存储字符串的长度。我们在不继续看代码的前提下，可以根据这个例子来知道其他情况也是根据类型解析args，最后复制到buffer。但是adv的作用无法推理，所以需要继续看代码。

我们可以找到一个adv被使用的地方来了解它的作用

```c
        fmt++;
        ptr += adv;
```

ptr需要在循环结束时加上adv，说明它的作用是设置buffer指针的偏移。因为我们格式化的结果保存到了buffer，buffer的尾部就会往后移动，所以ptr也要移动。下次保存结果到buffer就不需要移动指针。

代码中还有`__itoa_internal`这样的函数，它是用来把数字类型转换成字符串的。

```c
                case 'd': {
                    num = va_arg(args, int);
                    __itoa_internal(num, buffer + ptr, 10, &adv);
                    break;
                }
```

先根据value来判断要不要添加符号。最后都是转换成正整数（无符号整数）来传递给`__uitoa_internal`处理。

```c
char*
__itoa_internal(int value, char* str, int base, unsigned int* size)
{
    if (value < 0 && base == 10) {
        str[0] = '-';
        unsigned int _v = (unsigned int)(-value);
        __uitoa_internal(_v, str + 1, base, size);
    } else {
        __uitoa_internal(value, str, base, size);
    }

    return str;
}
```

如果值是0，直接格式化成`'0'`

```c
char*
__uitoa_internal(unsigned int value, char* str, int base, unsigned int* size)
{
    unsigned int ptr = 0;
    if (value == 0) {
        str[0] = '0';
        ptr++;
    }
```

这里是将value在循环中不断除以base。base就是进制。比如16进制，需要每次除以16。

```c
        while (value) {
            str[ptr] = base_char[value % base];
            value = value / base;
            ptr++;
        }
```

对于循环第一行，我们可以假设base为16，`value`值为`0x156`

```c
char base_char[] = "0123456789abcdefghijklmnopqrstuvwxyz";
```

`base_char[value % base]`就是`base_char[12]=='6'`（得到最低位的值），接着把value除以进制，再进行下一次循环，直到value为0。后面会依次得到`'5'`、`'1'`。得到下面数组内容。

```
[0] [1] [2]
'6' '5' '1'
```

实际上我们要输出`"156"`，而不是`"651"`，所以最后需要用下面代码把字符串排列顺序颠倒。

```c
        for (unsigned int i = 0; i < (ptr >> 1); i++) {
            char c = str[i];
            str[i] = str[ptr - i - 1];
            str[ptr - i - 1] = c;
        }
```

格式化字符串的尾部设置一下

```c
    str[ptr] = '\0';
```

接下来更新size，size实际上是另外一个返回值（adv的指针）。因为格式化后字符串往往会膨胀，所以要记录膨胀后的偏移（`unsigned int ptr = 0;`）。

```c
    if (size) {
        *size = ptr;
    }
```

总的来说，项目printf的实现是把fmt中代格式化字符串，进行格式化后写入buffer。adv的作用是告诉ptr膨胀后的大小（见代码` ptr += adv;`）。不过buffer可能会溢出，不知道会不会为后面的hack环节买下伏笔。

到这里libs/目录下的代码就分析完了。

### includes/hal/io.h

来一些硬件io相关的操作。

下面函数用于向一个port写入一个字节大小的value,使用的是扩展内联汇编。主要是使用`out`指令来写入。

```c
void io_port_wb(uint8_t port, uint8_t value) {
    asm volatile (
        "movb %0, %%al\n"
        "movb %1, %%dx\n"
        "out %%al, %%dx\n"
        :: "r"(value) "r"(port)
    );
}
```

其他函数也是如此，略过。

### includes/lunaix/mm/page.h

接下来看一些宏预热一下，马上分析虚拟内存相关代码。

lunaixOS的页目录和页表虚拟地址定义。也就是说，修改0xFFFFF000U~0xFFFFF000U+0xfff这个范围内的值就是在修改页目录。

```c
// 页目录的虚拟基地址，可以用来访问到各个PDE
#define PTD_BASE_VADDR                0xFFFFF000U

// 页表的虚拟基地址，可以用来访问到各个PTE
#define PT_BASE_VADDR                 0xFFC00000U
```

### includes/lunaix/constants.h

定义了一些虚拟地址和物理地址。

```c
#define K_STACK_SIZE            0x100000U
#define K_STACK_START           ((0xFFBFFFFFU - K_STACK_SIZE) + 1)
#define HIGHER_HLF_BASE         0xC0000000UL
#define MEM_1MB                 0x100000UL

#define VGA_BUFFER_VADDR        0xB0000000UL
#define VGA_BUFFER_PADDR        0xB8000UL
#define VGA_BUFFER_SIZE         4096
```

### kernel/mm/pmm.c

使用一个bit代表一个物理页，如果这个bit为1，表示该页已分配。4GB（2^32）总共包括2^20个4KB（2^12）。又因为uint8_t包含8个bits，所以数组大小为2^20/8（PM_BMP_MAX_SIZE）。

```c
#define PM_BMP_MAX_SIZE        (128 * 1024)
uint8_t pm_bitmap[PM_BMP_MAX_SIZE];
```

这个函数用于把某个bit置为0，表示该page被free了。

```c
void
pmm_mark_page_free(uintptr_t ppn)
{
    MARK_PG_AUX_VAR(ppn)
    pm_bitmap[group] = pm_bitmap[group] & ~msk;
}
```

其他函数也是这样。读者在自己实现时，可以先实现bitmap数据结构，方便进行位操作。

此时下面的函数不用细看也能知道是把连续的页表标记为占用，这样读代码可以节省很多时间。

```c
void
pmm_mark_chunk_occupied(uint32_t start_ppn, size_t page_count)
```

初始化物理内存管理器pmm（physical memory manager）。就是把所有bits设置为1。

```c
void
pmm_init(uintptr_t mem_upper_lim)
{
    max_pg = (PG_ALIGN(mem_upper_lim) >> 12);

    pg_lookup_ptr = LOOKUP_START;

    // mark all as occupied
    for (size_t i = 0; i < PM_BMP_MAX_SIZE; i++) {
        pm_bitmap[i] = 0xFFU;
    }
}
```

0号页一般不用，表示无效地址。总要有一个页的范围来表示一个指针的指向是无效的。如果一个指针指向哪里都是合法，那我们函数不能把指针作为NULL来返回错误。因为别人不会觉得NULL表示函数执行失败。

下面分析分配内存的`pmm_alloc_page()`函数实现，它会遍历pm_bitmap

```c
while (!good_page_found && pg_lookup_ptr < upper_lim) {
        chunk = pm_bitmap[pg_lookup_ptr >> 3];
```

直到找到第一个值为0的bit。找到后将该bit置为1，返回该页面的物理地址（`good_page_found`）。

```c
if (chunk != 0xFFU) {
            for (size_t i = pg_lookup_ptr % 8; i < 8; i++, pg_lookup_ptr++) {
                if (!(chunk & (0x80U >> i))) {
                    pmm_mark_page_occupied(pg_lookup_ptr);
                    good_page_found = pg_lookup_ptr << 12;
                    break;
                }
            }
        }
```

pg_lookup_ptr作为全局变量，它存储每次分配后搜索到的页号，赋值给old_pg_ptr。每次的搜索范围是`[old_pg_ptr, max_pg)`。

```c
    size_t old_pg_ptr = pg_lookup_ptr;
    size_t upper_lim = max_pg;
```

如果没找到会修改old_pg_ptr为LOOKUP_START。再在`[1, old_pg_ptr)`范围内查找。

```c
            // We've searched the interval [old_pg_ptr, max_pg) but failed
            //   may be chances in [1, old_pg_ptr) ?
            // Let's find out!
            if (pg_lookup_ptr >= upper_lim && old_pg_ptr != LOOKUP_START) {
                upper_lim = old_pg_ptr;
                pg_lookup_ptr = LOOKUP_START;
                old_pg_ptr = LOOKUP_START;
            }
```

### kernel/mm/vmm.c

实现页目录初始化。页目录有1024个entry，所以分配1个物理页，把其中的1024个4Byte置0。最后一个entry设置为PDE(T_SELF_REF_PERM, dir)，用于映射自己。

```c
ptd_t* vmm_init_pd() {
    ptd_t* dir = pmm_alloc_page();
    for (size_t i = 0; i < 1024; i++)
    {
        dir[i] = 0;
    }
    
    // 自己映射自己，方便我们在软件层面进行查表地址转换
    dir[1023] = PDE(T_SELF_REF_PERM, dir);

    return dir;
}
```

当我们访问虚拟地址`0xFFFFF000U`（PTD_BASE_VADDR），MMU先取出22到31位的bits值1023，即查看页目录的第1023个entry。再取出第12到21位值为1023。因为上面设置该entry存储的物理地址为dir，所以访问dir[1023]，取出里面的值作为最后的物理地址。该值为页目录起始物理地址。这样我们就可以通过虚拟地址来修改页目录了。

如果我们想把一个指定的虚拟地址映射到物理地址，要实现`vmm_map_page`。这个过程涉及到写页表，查找物理页。该函数最终会给我们一个映射到`pa`的虚拟地址，但这个虚拟地址不一定是我们期望的`va`。

```c
void* vmm_map_page(void* va, void* pa, pt_attr dattr, pt_attr tattr) {
    // 显然，对空指针进行映射没有意义。
    if (!pa || !va) {
        return NULL;
    }
```

先是根据参数va来得到需要修改的页目录项和页表项。ptd指向页目录第一个entry。

```c
    uintptr_t pd_offset = PD_INDEX(va);
    uintptr_t pt_offset = PT_INDEX(va);
    ptd_t* ptd = (ptd_t*)PTD_BASE_VADDR;
```

最好情况是这两个页目录项和页表项没有被占用。即pde取出ptd[pd_offset]存储的值为0，不会进入while循环。

```c
    // 在页表与页目录中找到一个可用的空位进行映射（位于va或其附近）
    ptd_t* pde = ptd[pd_offset];
	pt_t* pt = (uintptr_t)PT_VADDR(pd_offset);
    while (pde && pd_offset < 1024) {
```

项目这里选择的是分配一个新页作为新页表。

```c
    // 页目录有空位，需要开辟一个新的 PDE
    uint8_t* new_pt_pa = pmm_alloc_page();
```

设置页目录指向新页表基址。设置新页表的entry指向要映射的物理地址pa。

```c
    ptd[pd_offset] = PDE(dattr, new_pt_pa);
    memset((void*)PT_VADDR(pd_offset), 0, PM_PAGE_SIZE);
    pt[pt_offset] = PTE(tattr, pa);
```

最后根据offset获得虚拟地址。因为这个offset在非最好情况下会变，不能直接返回va，下面来看看为什么会改变。

```c
return V_ADDR(pd_offset, pt_offset, PG_OFFSET(va));
```

假如不是最佳情况，会进入while循环.

```c
    while (pde && pd_offset < 1024) {
        if (pt_offset == 1024) {
            pd_offset++;
            pt_offset = 0;
            pde = ptd[pd_offset];
            pt = (pt_t*)PT_VADDR(pd_offset);
        }
        // 页表有空位，只需要开辟一个新的 PTE
        if (pt && !pt[pt_offset]) {
            pt[pt_offset] = PTE(tattr, pa);
            return V_ADDR(pd_offset, pt_offset, PG_OFFSET(va));
        }
        pt_offset++;
    }
```

如果页目录中该项存在且页表项为空，进入while循环里面的`if (pt && !pt[pt_offset])`分支。直接写好这个页表项就行了。如果页目录该项和页表该项不为空，就没地方写了。所以要退而求其次`pt_offset++;`。

如果该页表项后面的项都没地方写，就换一个页表。

```c
        if (pt_offset == 1024) {
            pd_offset++;
```

更新一下pde和pt，继续循环

```c
            pde = ptd[pd_offset];
            pt = (pt_t*)PT_VADDR(pd_offset);
```

总的来说就是遍历页目录和页表，找到可写的地方来设置页目录和页表。

如果找不到，pd_offset会因为过大跳出循环

```
while (pde && pd_offset < 1024)
```

最后函数返回NULL，下面代码应该改成大于等于。

```c
    // 页目录与所有页表已满！
    if (pd_offset > 1024) {
        return NULL;
    }
```

`vmm_unmap_page`用于取消一个虚拟地址映射，涉及到清除对应页目录项和页表项的操作。

先是获得两个偏移和页目录指针。

```c
void vmm_unmap_page(void* vpn) {
    uintptr_t pd_offset = PD_INDEX(vpn);
    uintptr_t pt_offset = PT_INDEX(vpn);
    ptd_t* self_pde = PTD_BASE_VADDR;
```

然后检查是否pde的值存在。因为用户可能随便输入参数，如果参数对于的项为空，就不用写了。

```c
    ptd_t pde = self_pde[pd_offset];

    if (pde) {
	//...
    }
}
```

获得页表地址，再根据pt_offset偏移获得要写的entry的地址。TLB是虚拟地址到网络地址映射的缓存，相当于数据结构的map。修改entry前需要使用invlpg来刷新TLB缓存。

```c
        pt_t* pt = (pt_t*)PT_VADDR(pd_offset);
        uint32_t pte = pt[pt_offset];
        if (IS_CACHED(pte) && pmm_free_page(pte)) {
            // 刷新TLB
            #ifdef __ARCH_IA32
            __asm__("invlpg (%0)" :: "r"((uintptr_t)vpn) : "memory");
            #endif
        }
        pt[pt_offset] = 0;
```

其他函数就略过不分析了。

### arch/x86/boot.S arch/x86/hhk.c

从注释可以知道，有1个页目录、5个页表。并且`_k_ptd`表示页目录地址。页表和页目录放在一段连续的空间。

```c
/* 
    1 page directory, 
    5 page tables:
        1. Mapping reserved area and hhk_init
        2-5. Remapping the kernels
*/

.section .kpg
    .global _k_ptd
    _k_ptd:
        .skip KPG_SIZE, 0
```

初始化栈

```c
    start_: 
        movl $stack_top, %esp

        subl $16, %esp
```

把参数放入栈，调用函数`_save_multiboot_info`，`$mb_info`是参数`uint8_t* destination`，ebx是参数`multiboot_info_t* info`。

```c
        movl $mb_info, 4(%esp)
        movl %ebx, (%esp)
        call _save_multiboot_info
```

_save_multiboot_info用于保存multiboot_info_t结构体和结构体内部指针指向的区域。

接下来初始化高半核。为什么这里_k_ptd需要减去一个值呢？

```c
        /*
            _hhk_init用来初始化我们高半核：
                1. 初始化最简单的PD与PT（重新映射我们的内核至3GiB处，以及对相应的地方进行Identity Map）
        */

        movl $(KPG_SIZE), 4(%esp)
        movl $(_k_ptd - 0xC0000000), (%esp)    /* PTD物理地址 */
        call _hhk_init
```

linker.ld给出了答案。每个节都有一个虚拟地址（VMA）和加载地址（LMA）。默认情况下VMA和LMA相同。

```assembly
/* Relocation of our higher half kernel */
. += 0xC0000000;

/* 好了，我们的内核…… */
.text BLOCK(4K) : AT ( ADDR(.text) - 0xC0000000 ) {
//...
.kpg BLOCK(4K) : AT ( ADDR(.kpg) - 0xC0000000 ) {
    build/obj/arch/x86/*.o (.kpg)
}
```

VMA指的是文件中的地址，并不是内存中的地址。通过section信息可以知道这一点。

```c
$ readelf -S lunaix.bin 
There are 37 section headers, starting at offset 0x143f8:

Section Headers:
  [Nr] Name              Type            Addr     Off    Size   ES Flg Lk Inf Al
  [ 0]                   NULL            00000000 000000 000000 00      0   0  0
  [ 1] .hhk_init_text    PROGBITS        00100000 001000 000216 00  AX  0   0  1
  [ 2] .hhk_init_bss     NOBITS          00101000 002000 004fbe 00  WA  0   0 16
  [ 3] .text             PROGBITS        c0106000 002000 001274 00  AX  0   0  1
  [ 4] .text.__x86.[...] PROGBITS        c0107274 003274 000004 00  AX  0   0  1
  [ 5] .text.__x86.[...] PROGBITS        c0107278 003278 000004 00  AX  0   0  1
  [ 6] .text.__x86.[...] PROGBITS        c010727c 00627c 000004 00  AX  0   0  1
  [ 7] .text.__x86.[...] PROGBITS        c0107280 006280 000004 00  AX  0   0  1
  [ 8] .bss              NOBITS          c0108000 004000 020152 00  WA  0   0 32
```

LMA对于这个项目来说是物理地址，VMA就是符号的地址。`ADDR(.kpg) - 0xC0000000`表示实际页目录会存放在更低的地方，也就是说直接用`_k_ptd`这个符号不能得到存放地点，要减去0xC0000000。

需要动态调试来理解一下。

如果你想使用自带的gcc来编译**这个版本**的`lunaix-os`。需要修改一些文件。

config/make-cc

```makefile
CC := gcc -m32 -fno-pie
AS := as
...
LDFLAGS := -no-pie -ffreestanding $(O) -nostdlib
```

搜索-lgcc并删除，i686-elf-objcopy改成objcopy、i686-elf-objdump改成objdump

执行下面代码后会有三个窗口（原本窗口、qemu控制台、qemu）。

```c
make debug-qemu
```

先在qemu控制台输入c。

```c
Trying 127.0.0.1...
Connected to 127.0.0.1.
Escape character is '^]'.
QEMU 6.2.0 monitor - type 'help' for more information
(qemu) c
```

再到原本窗口进行调试。

```c
pwndbg> b start_ 
Breakpoint 1 at 0x10000c
pwndbg> c
```

最后点击qemu的界面enter启动OS。

可以看到$0x12b000是`$(_k_ptd - 0xC0000000)`真实结果。

```assembly
   0x100024 <start_+24>    movl   $0x6000, 4(%esp)
   0x10002c <start_+32>    movl   $0x12b000, (%esp)
   0x100033 <start_+39>    calll  _hhk_init                      <_hhk_init>
```

pwndbg中可以更方便的看到源代码信息。

```c
In file: /home/ffreestanding/Desktop/lunaixos-tutorial/check/lunaix-os/lunaix-os/arch/x86/hhk.c
   123 _hhk_init(ptd_t* ptd, uint32_t kpg_size) {
   124 
   125     // 初始化 kpg 全为0
   126     //      P.s. 真没想到GRUB会在这里留下一堆垃圾！ 老子的页表全乱套了！
   127     uint8_t* kpg = (uint8_t*) ptd;
 ► 128     for (uint32_t i = 0; i < kpg_size; i++)
   129     {
   130         *(kpg + i) = 0;
   131     }
   132     
   133     _init_page(ptd);
```

接着分析`_hhk_init`函数，它调用了_init_page。

```c
void 
_hhk_init(ptd_t* ptd, uint32_t kpg_size) {

    // 初始化 kpg 全为0
    //      P.s. 真没想到GRUB会在这里留下一堆垃圾！ 老子的页表全乱套了！
    uint8_t* kpg = (uint8_t*) ptd;
    for (uint32_t i = 0; i < kpg_size; i++)
    {
        *(kpg + i) = 0;
    }
    
    _init_page(ptd);
}
```

`SET_PDE`取出以ptd为基地址的页表0号entry，写入`PDE(PG_PRESENT, ptd + PG_MAX_ENTRIES)`到entry中。

```c
void 
_init_page(ptd_t* ptd) {
    SET_PDE(ptd, 0, PDE(PG_PRESENT, ptd + PG_MAX_ENTRIES))
```

`PDE`宏用于构造一个页目录entry。这里写入的是第一个页表的物理地址（ptd + PG_MAX_ENTRIES），因为页目录后面紧接着的是页表。它们被设计在一段连续的内存。

看看如何对等映射hhk_init。

```c
    // 对等映射我们的hhk_init，这样一来，当分页与地址转换开启后，我们依然能够照常执行最终的 jmp 指令来跳转至
    //  内核的入口点
    for (uint32_t i = 0; i < HHK_PAGE_COUNT; i++)
    {
        SET_PTE(ptd, PG_TABLE_IDENTITY, 256 + i, PTE(PG_PREM_RW, 0x100000 + (i << 12)))
    }
```

我们可以看看linker.ld。可以知道我们的__init_hhk_end是初始化代码和数据存储的上边界，0x100000是下边界。

```assembly
    . = 0x100000;

    /* 这里是我们的高半核初始化代码段和数据段 */

    .hhk_init_text BLOCK(4K) : {
    //...
    __init_hhk_end = ALIGN(4K);
```

那么我们可以计算出所需page的数量。`__init_hhk_end`符号由链接脚本提供。C代码通过`extern uint8_t __init_hhk_end;`来获得。

```c
#define HHK_PAGE_COUNT              ((sym_val(__init_hhk_end) - 0x100000 + 0x1000 - 1) >> 12)
```

for循环次数就得到了，接下来写入页表项即可。

为什么要对等映射hhk_init呢？如果有下面这条指令

```assembly
jmp init_func
```

init_func的值是固定的。如果没开启分页，就会把init_func作为线性地址（此时相当于物理地址）来执行物理地址init_func处的代码。如果开启了分页，就会把它作为虚拟地址来看待。虚拟地址转换成物理地址再执行物理地址处的代码。所以虚拟地址要映射到相等的物理地址。系统才能正常运转。如果没有对等映射，跳转到的也是无意义的物理地址。

如果没有设置好虚拟地址到物理地址的映射，分页后的下一行代码都走不到。

为什么下面内核代码不需要对等映射呢？

```c
	// --- 将内核重映射至高半区 ---
    
    // 这里是一些计算，主要是计算应当映射进的 页目录 与 页表 的条目索引（Entry Index）
    uint32_t kernel_pde_index = PD_INDEX(sym_val(__kernel_start));
    uint32_t kernel_pte_index = PT_INDEX(sym_val(__kernel_start));
    uint32_t kernel_pg_counts = KERNEL_PAGE_COUNT;
```

因为我们把程序代码里符号地址放到了高地址，加载地址（物理地址）在低地址。如果我们不分页调用内核函数会跳转到无意义的更高地址。真正的内核在低地址。

```
.text BLOCK(4K) : AT ( ADDR(.text) - 0xC0000000 ) {
```

所以内核映射要做的是映射函数符号或者其他内核符号到真正的物理地址。

```c
    // 将内核所需要的页表注册进页目录
    //  当然，就现在而言，我们的内核只占用不到50个页（每个页表包含1024个页）
    //  这里分配了3个页表（12MiB），未雨绸缪。
	for (uint32_t i = 0; i < PG_TABLE_STACK - PG_TABLE_KERNEL; i++)
    {
        SET_PDE(
            ptd, 
            kernel_pde_index + i,   
            PDE(PG_PREM_RW, PT_ADDR(ptd, PG_TABLE_KERNEL + i))
        )
    }

    // ...
    
    // 重映射内核至高半区地址（>=0xC0000000）
    for (uint32_t i = 0; i < kernel_pg_counts; i++)
    {
        SET_PTE(
            ptd, 
            PG_TABLE_KERNEL, 
            kernel_pte_index + i, 
            PTE(PG_PREM_RW, kernel_pm + (i << 12))
        )
    }
```

循环映射在`kernel/mm/vmm.c`中已经分析过了

```c
    // 最后一个entry用于循环映射
    SET_PDE(
        ptd,
        1023,
        PDE(T_SELF_REF_PERM, ptd)
    );
```

其余代码视频里已经讲解了。

### kernel/asm/x86/prologue.S

实际高半核起始虚拟地址是0x100000+hhk_init大小+0xC0000000

```assembly
/* 高半核入口点 - 0xC0000000 */

.section .text
    .global hhk_entry_
    hhk_entry_:
```

安装好GDT后，主要调用了三个函数。接下来看看这三个函数。

### kernel/k_main.c

第一个函数是`_kernel_init`，先是安装了IDT。

```c
void
_kernel_init(multiboot_info_t* mb_info)
{
    _init_idt();
```

然后根据传入的mb_info来标记哪些页为占用，哪些为空闲。

我觉得最值得注意的是映射VGA地址。因为在此前低1MiB对等映射包含了VGA地址。后面_kernel_post_init函数会清除对等映射。所以这里对VGA再映射一次。

```c
    // 首先，标记VGA部分为已占用
    pmm_mark_chunk_occupied(VGA_BUFFER_PADDR >> 12, vga_buf_pgs);
    
    // 重映射VGA文本缓冲区（以后会变成显存，i.e., framebuffer）
    for (size_t i = 0; i < vga_buf_pgs; i++)
    {
        vmm_map_page(VGA_BUFFER_VADDR + (i << 12), VGA_BUFFER_PADDR + (i << 12), PG_PREM_RW, PG_PREM_RW);
    }
    
    // 更新VGA缓冲区位置至虚拟地址
    tty_set_buffer(VGA_BUFFER_VADDR);
```

它将0xB0000000UL映射到了0xB8000UL。

```c
#define VGA_BUFFER_VADDR        0xB0000000UL
#define VGA_BUFFER_PADDR        0xB8000UL
```

看看`_kernel_post_init`这个函数。

这个函数用于清理之前高半核初始化代码，将它们标记为可用物理页。

```c
void
_kernel_post_init() {
    printf("[KERNEL] === Post Initialization === \n");
    size_t hhk_init_pg_count = ((uintptr_t)(&__init_hhk_end)) >> 12;
    printf("[MM] Releaseing %d pages from 0x0.\n", hhk_init_pg_count);

    // 清除 hhk_init 与前1MiB的映射
    for (size_t i = 0; i < hhk_init_pg_count; i++) {
        vmm_unmap_page((i << 12));
    }
    printf("[KERNEL] === Post Initialization Done === \n\n");
}
```

最后是`_kernel_main`函数，它打印了一些信息。

### hal/cpu.c

传入0x80000000UL和0，如果返回值大于0x80000004UL则说明支持[1]。

```c
int cpu_brand_string_supported() {
    reg32 supported = __get_cpuid_max(BRAND_LEAF, 0);
    return (supported >= 0x80000004UL);
}
```

从__get_cpuid_max定义上面的注释中可以知道它的作用。它使用cpuid指令来实现的，库函数这里就不分析了。

```c
/* Return highest supported input value for cpuid instruction.  ext can
   be either 0x0 or 0x80000000 to return highest supported value for
   basic or extended cpuid information.  Function returns 0 if cpuid
   is not supported or whatever cpuid returns in eax register.  If sig
   pointer is non-null, then first four bytes of the signature
   (as found in ebx register) are returned in location pointed by sig.  */
static __inline unsigned int
__get_cpuid_max (unsigned int __ext, unsigned int *__sig)
```

`cpu_get_brand`中如果不支持，就设置成问号。

```c
void cpu_get_brand(char* brand_out) {
    if(!cpu_brand_string_supported()) {
        brand_out[0] = '?';
        brand_out[1] = '\0';
    }
    //...
}
```

接下来调用cpuid指令参数分别是0x80000002UL、0x80000003UL、0x80000004UL。

```c
    uint32_t* out = (uint32_t*) brand_out;
    reg32 eax, ebx, edx, ecx;
    for (uint32_t i = 2, j = 0; i < 5; i++)
    {
        __get_cpuid(BRAND_LEAF + i, &eax, &ebx, &ecx, &edx);
        out[j] = eax;
        out[j + 1] = ebx;
        out[j + 2] = ecx;
        out[j + 3] = edx;
        j+=4;
    }
    brand_out[48] = '\0';
```

可以看看下面这个cpuid参数和其对应的返回值[2]。总共可以返回48字节的处理器信息。

```tex
EAX = 80000002H:
EAX := Processor Brand String;
EBX := Processor Brand String, continued;
ECX := Processor Brand String, continued;
EDX := Processor Brand String, continued;
BREAK;
EAX = 80000003H:
EAX := Processor Brand String, continued;
EBX := Processor Brand String, continued;
ECX := Processor Brand String, continued;
EDX := Processor Brand String, continued;
BREAK;
EAX = 80000004H:
EAX := Processor Brand String, continued;
EBX := Processor Brand String, continued;
ECX := Processor Brand String, continued;
EDX := Processor Brand String, continued;
BREAK;
EAX = 80000005H:
EAX := Reserved = 0;
EBX := Reserved = 0;
ECX := Reserved = 0;
EDX := Reserved = 0;
```

cpu_get_brand会把处理器信息复制到参数所指位置，被_kernel_main调用。

```c
    cpu_get_brand(buf);
    printf("CPU: %s\n\n", buf);
```

如果用qemu运行OS可以看到打印的是

```bash
CPU： QEMU Virtual CPU version 2.5+
```

## 参考

[1]Intel 手册，Vol. 2A，Figure 3-9. Determination of Support for the Processor Brand String

[2]Intel 手册，Vol. 2A，Chapter 3，CPUID—CPU Identification，3-255
