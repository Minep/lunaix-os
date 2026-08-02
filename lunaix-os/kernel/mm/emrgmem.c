#include <lunaix/spike.h>
#include <lunaix/compiler.h>
#include <lunaix/types.h>
#include <lunaix/mm/emrgmem.h>
#include <lunaix/sections.h>
#include <lunaix/syslog.h>
#include <lunaix/kpreempt.h>
#include <lunaix/owloysius.h>

#define MIN_ALIGNMENT   sizeof(int)
#define NR_CHUNKS       (CONFIG_EMRGMEM_SIZE * 1024) / MIN_ALIGNMENT

struct emframe;

struct emrg_mem {
    union {
        u8_t* _mem;
        ptr_t addr;
    };
    struct emframe* prev;
};

struct emframe {
    unsigned int metadata;
    u8_t chunk[0];
} align(4);

static u8_t __emrgmem[CONFIG_EMRGMEM_SIZE * 1024] emrgmem;
static struct emrg_mem __emem;

static inline unsigned int
__emframe_size(struct emframe* frame)
{
    return frame->metadata >> 1;
}

static inline bool
__emframe_used(struct emframe* frame)
{
    return frame->metadata & 1;
}

static inline void
__emframe_set_size(struct emframe* frame, unsigned int size)
{
    frame->metadata = size << 1;
}

static inline void
__emframe_set_used(struct emframe* frame, bool used)
{
    frame->metadata = (frame->metadata & ~1) | (used & 1);
}

static inline struct emframe*
__emframe_next(struct emframe* frame) {
    ptr_t next;

    next = (ptr_t)frame;
    next += __emframe_size(frame) * MIN_ALIGNMENT + sizeof(struct emframe);

    if (next >= __emem.addr + NR_CHUNKS)
        return NULL;

    return (struct emframe*)next;
}

static inline struct emframe*
__emframe_of(ptr_t addr)
{
    if (addr <= __emem.addr || addr >= __emem.addr + NR_CHUNKS)
        return NULL;

    return (struct emframe*)(addr - offsetof(struct emframe, chunk));
}

static inline void
__emframe_split(struct emframe* frame, struct emframe** other,
                size_t size)
{
    struct emframe* next;
    size_t orig_size;

    orig_size = __emframe_size(frame);
    __emframe_set_size(frame, size);

    next = __emframe_next(frame);
    if (next) {
        __emframe_set_size(next, orig_size - size);
        __emframe_set_used(next, false);
    }
        
    *other = next;
}

static inline void
__emframe_merge_forward(struct emframe* frame) 
{
    struct emframe* next;

    next = __emframe_next(frame);
    if (!next)
        return;

    if (__emframe_used(next))
        return;

    __emframe_set_size(frame, __emframe_size(frame) + __emframe_size(next));
}

static struct emframe*
__alloc_frame(size_t size)
{
    struct emframe *frame, *other;

    size = ICEIL(size, MIN_ALIGNMENT);

    frame = __emem.prev;

    while (frame && (__emframe_used(frame) || __emframe_size(frame) < size))
    {
        frame = __emframe_next(frame);
    }

    if (!frame)
        return NULL;

    if (__emframe_size(frame) == size)
        return frame;

    __emframe_split(frame, &__emem.prev, size);

    return frame;
}

static void
__free_frame(struct emframe* frame)
{
    __emframe_set_used(frame, false);
    __emframe_merge_forward(frame);

    if ((ptr_t)__emem.prev < (ptr_t)frame)
        __emem.prev = frame;
}

void 
emrgmem_init() 
{
    struct emframe* frame;

    __emem.addr = (ptr_t)__emrgmem;
    frame = (struct emframe*)__emem.addr;

    __emframe_set_size(frame, NR_CHUNKS);
    __emframe_set_used(frame, false);

    __emem.prev = frame;
}

void*
emalloc(size_t size)
{
    irqflag_t flag;
    struct emframe* frame;
    void* result;

    no_preemption_saved(&flag);

    result = NULL;
    frame = __alloc_frame(size);
    if (!frame) 
        goto done;

    __emframe_set_used(frame, true);
    result = (void*)&frame->chunk;

done:
    preempt_restore(&flag);
    return result;
}

void
emfree(void *ptr)
{
    struct emframe* frame;
    irqflag_t flag;

    no_preemption_saved(&flag);

    frame = __emframe_of((ptr_t)ptr);
    if (frame)
        __free_frame(frame);

    preempt_restore(&flag);
}

LOG_MODULE("emrgmem")

static void
emrgmem_report()
{
    struct emframe* frame;
    int nr_frame;
    unsigned long max_size, min_size, size;
    unsigned long remaining;

    frame = (struct emframe*)__emem.addr;

    nr_frame = 0;
    remaining = 0;
    max_size = 0;
    min_size = -1UL;

    while (frame) {
        if (!__emframe_used(frame)) {
            remaining += __emframe_size(frame);
            goto cont;
        }

        size = __emframe_size(frame);
        if (size > max_size)
            max_size = size;
        else if (size < min_size)
            min_size = size;
        
cont:
        nr_frame++;
        frame = __emframe_next(frame);
    }

    INFO("used: %d frames (max=%lu, min=%lu); %luk free.",
         nr_frame, 
         max_size * MIN_ALIGNMENT, 
         min_size * MIN_ALIGNMENT,
         remaining * MIN_ALIGNMENT / 1024);
}
owloysius_fetch_init(emrgmem_report, on_boot);
