## 准备工作

```sh
git checkout 4c6d990440cdba6c7dd294adb7e435770ffcbcc4
```

观看对应视频

这次内容比较少。

## 代码分析

### kernel/mm/dmm.c 一些宏

堆的单位元素可以称为chunk。一个chunk包含头部空间和实际存储数据的空间。先看看下面几个宏定义。0b1表示当前chunk已分配，0b10表示前一个chunk未被分配。

```c
#define M_ALLOCATED 0x1
#define M_PREV_FREE 0x2

#define M_NOT_ALLOCATED 0x0
#define M_PREV_ALLOCATED 0x0
```

chunk的头大小为4字节，如果把低三bit清零，那么它的值就表示chunk的大小。`CHUNK_S`就是这样。

```c
#define CHUNK_S(header) ((header) & ~0x3)
```

LW表示load one word（SW表示save one word）。NEXT_CHK可以获得一个指向下一个chunk头部的指针。

```c
#define LW(p) (*((uint32_t*)(p)))
#define NEXT_CHK(hp) ((uint8_t*)(hp) + CHUNK_S(LW(hp)))
```

PACK方便我们构造一个chunk头。

```c
#define PACK(size,flags) (((size) & ~0x3) | (flags))
```

### 堆初始化

堆的初始化函数`kalloc_init`被`_kernel_post_init`函数调用。

```c
void 
_kernel_post_init() {
    printf("[KERNEL] === Post Initialization === \n");
    size_t hhk_init_pg_count = ((uintptr_t)(&__init_hhk_end)) >> PG_SIZE_BITS;
    printf("[MM] Releaseing %d pages from 0x0.\n", hhk_init_pg_count);

    // 清除 hhk_init 与前1MiB的映射
    for (size_t i = 0; i < hhk_init_pg_count; i++) {
        vmm_unmap_page((void*)(i << PG_SIZE_BITS));
    }

    assert_msg(kalloc_init(), "Fail to initialize heap");
}
```

下面结构体用于表示以分配的堆空间的范围。start表示起始地址，brk表示结束地址。

```c
typedef struct 
{
    void* start;
    void* brk;
} heap_context_t;
```

初始化操作会设置start和brk，最后执行`dmm_init`。符号`__kernel_heap_start`由linker.ld提供。

```c
extern uint8_t __kernel_heap_start;
heap_context_t __kalloc_kheap;
int
kalloc_init() {
    __kalloc_kheap.start = &__kernel_heap_start;
    __kalloc_kheap.brk = 0;

    return dmm_init(&__kalloc_kheap);
}
```

### kernel/mm/dmm.c 一些函数

lxbrk用于增加堆大小。这里可以忽略分配大小、对齐等细节。重点在于它使用了`vmm_alloc_pages`分配新页给新增加的空间，并且更新了brk。

```c
void*
lxbrk(heap_context_t* heap, size_t size)
{
    if (size == 0) {
        return heap->brk;
    }

    // The upper bound of our next brk of heap given the size.
    // This will be used to calculate the page we need to allocate.
    // The "+ WSIZE" capture the overhead for epilogue marker
    void* next = heap->brk + ROUNDUP(size + WSIZE, WSIZE);

    if ((uintptr_t)next >= K_STACK_START) {
        return NULL;
    }

    uintptr_t heap_top_pg = PG_ALIGN(heap->brk);
    if (heap_top_pg != PG_ALIGN(next)) {
        // if next do require new pages to be allocated
        if (!vmm_alloc_pages((void*)(heap_top_pg + PG_SIZE),
                             ROUNDUP(size, PG_SIZE),
                             PG_PREM_RW)) {
            return NULL;
        }
    }

    void* old = heap->brk;
    heap->brk += size;
    return old;
}
```

接下来看看**Dynamic memory manager**相关代码。

```c
int
dmm_init(heap_context_t* heap)
{
    assert((uintptr_t)heap->start % BOUNDARY == 0);
    heap->brk = heap->start;
```

给skb（start）分配一个可读可写的页

```c
    vmm_alloc_page(heap->brk, PG_PREM_RW);
```

按照视频所说的放两个chunk头部，理由见视频。读者也可以有不同设计。

```c
    SW(heap->start, PACK(4, M_ALLOCATED));
    SW(heap->start + WSIZE, PACK(0, M_ALLOCATED));
    heap->brk += WSIZE;

    return lx_grow_heap(heap, HEAP_INIT_SIZE) != NULL;
}
```

接着调用`lx_grow_heap`，sz大小是HEAP_INIT_SIZE。调用lxbrk来分配新页。

```c
void*
lx_grow_heap(heap_context_t* heap, size_t sz)
{
    void* start;

    if (!(start = lxbrk(heap, sz))) {
        return NULL;
    }
    sz = ROUNDUP(sz, BOUNDARY);
```

构造chunk的头部和尾部，下一个chunk的头部。调用`coalesce`。

```c
    uint32_t old_marker = *((uint32_t*)start);
    uint32_t free_hdr = PACK(sz, CHUNK_PF(old_marker));
    SW(start, free_hdr);
    SW(FPTR(start, sz), free_hdr);
    SW(NEXT_CHK(start), PACK(0, M_ALLOCATED | M_PREV_FREE));

    return coalesce(start);
}
```

获得头部信息（hdr）、是否前一个chunk可使用（pf）、当前chunk大小（sz）、n_hdr（下一个头部信息）

```c
void*
coalesce(uint8_t* chunk_ptr)
{
    uint32_t hdr = LW(chunk_ptr);
    uint32_t pf = CHUNK_PF(hdr);
    uint32_t sz = CHUNK_S(hdr);

    uint32_t n_hdr = LW(chunk_ptr + sz);
```

如果前一个chunk可用，而且下一个chunk不可用（已被分配）那么合并前一个chunk。下面只分析一种情况，其他情况类似。

```c
	if (CHUNK_A(n_hdr) && pf) {
        // case 1: prev is free
        uint32_t prev_ftr = LW(chunk_ptr - WSIZE);//获得前一个chunk的尾部
        size_t prev_chunk_sz = CHUNK_S(prev_ftr);//获得前一个chunk的大小
        uint32_t new_hdr = PACK(prev_chunk_sz + sz, CHUNK_PF(prev_ftr));//构造新头部
        SW(chunk_ptr - prev_chunk_sz, new_hdr);//修改前一个chunk头部
        SW(FPTR(chunk_ptr, sz), new_hdr);//修改当前chunk的尾部
        chunk_ptr -= prev_chunk_sz;//修改合并后chunk指针
    //...
    // case 4: prev and next are not free
    return chunk_ptr;
}
```

place_chunk用于从一个chunk中分配出一个小chunk。如果diff等于0，就表示刚好满足大小，设置下一个chunk头即可。另外一种情况就是，小chunk比原来chunk小，此时要分割原来的chunk，最后调用coalesce合并chunk。

```c
void
place_chunk(uint8_t* ptr, size_t size)
{
    uint32_t header = *((uint32_t*)ptr);
    size_t chunk_size = CHUNK_S(header);
    *((uint32_t*)ptr) = PACK(size, CHUNK_PF(header) | M_ALLOCATED);
    uint8_t* n_hdrptr = (uint8_t*)(ptr + size);
    uint32_t diff = chunk_size - size;
    if (!diff) {
        // if the current free block is fully occupied
        uint32_t n_hdr = LW(n_hdrptr);
        // notify the next block about our avaliability
        SW(n_hdrptr, n_hdr & ~0x2);
    } else {
        // if there is remaining free space left
        uint32_t remainder_hdr = PACK(diff, M_NOT_ALLOCATED | M_PREV_ALLOCATED);
        SW(n_hdrptr, remainder_hdr);
        SW(FPTR(n_hdrptr, diff), remainder_hdr);

        coalesce(n_hdrptr);
    }
}
```

最后看看`lx_malloc`函数，先是使用一个循环来遍历堆，查看是否有可用的chunk。如果存在，则调用`place_chunk`，最后返回`BPTR(ptr)`指向chunk头部后面真正的数据储存地址。

```c
void*
lx_malloc(heap_context_t* heap, size_t size)
{
    // Simplest first fit approach.

    uint8_t* ptr = heap->start;
    // round to largest 4B aligned value
    //  and space for header
    size = ROUNDUP(size, BOUNDARY) + WSIZE;
    while (ptr < (uint8_t*)heap->brk) {
        uint32_t header = *((uint32_t*)ptr);
        size_t chunk_size = CHUNK_S(header);
        if (chunk_size >= size && !CHUNK_A(header)) {
            // found!
            place_chunk(ptr, size);
            return BPTR(ptr);
        }
        ptr += chunk_size;
    }
```

如果没有找到就扩大堆空间。

```c
    // if heap is full (seems to be!), then allocate more space (if it's
    // okay...)
    if ((ptr = lx_grow_heap(heap, size))) {
        place_chunk(ptr, size);
        return BPTR(ptr);
    }

    // Well, we are officially OOM!
    return NULL;
}
```

其他函数略
