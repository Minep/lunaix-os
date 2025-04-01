#include <lunaix/fs/twifs.h>
#include <lunaix/fs/twimap.h>
#include <lunaix/syscall.h>
#include <lunaix/syscall_utils.h>
#include <lunaix/syslog.h>
#include <lunaix/device.h>
#include <lunaix/owloysius.h>
#include <lunaix/ds/flipbuf.h>

#include <hal/term.h>

#include <klibc/strfmt.h>

#include "kp_records.h"

#define MAX_BUFSZ_HLF 256
#define MAX_KPENT_NUM 1024

static char tmp_buf[MAX_BUFSZ_HLF * 2];
static DEFINE_FLIPBUF(fmtbuf, MAX_BUFSZ_HLF, tmp_buf);

static struct kp_records kprecs = {
    .kp_ents = { .ents = { .next = &kprecs.kp_ents.ents,
                           .prev = &kprecs.kp_ents.ents } },
    .max_recs = MAX_KPENT_NUM,
    .kp_ent_wp = &kprecs.kp_ents.ents
};
export_symbol(debug, kprintf, kprecs);

static char*
shift_level(const char* str, int* level)
{
    if (str[0] == KMSG_LVLSTART) {
        *level = KMSG_LOGLEVEL(str[1]);

        return str += 2;
    }

    *level = KLOG_INFO;
    return str;
}

static inline void
__put_console(const struct kp_entry* ent)
{
    char* buf;
    time_t s, ms;
    size_t sz;

    if (unlikely(!sysconsole)) {
        return;
    }

    s   = ent->time / 1000;
    ms  = ent->time % 1000;
    buf = flipbuf_flip(&fmtbuf);
    sz  = ksnprintf(buf, MAX_BUFSZ_HLF, 
                    "[%04d.%03d] %s", s, ms, ent->content);
    
    sysconsole->ops.write(sysconsole, buf, 0, sz);
}

static inline void
kprintf_put(int level, const char* buf, size_t sz)
{
    __put_console(kprec_put(&kprecs, level, buf, sz));
}

static inline void
kprintf_ml(const char* component, int level, const char* fmt, va_list args)
{
    char* buf;
    size_t sz;

    buf = flipbuf_top(&fmtbuf);
    ksnprintf(buf, MAX_BUFSZ_HLF, "%s: %s\n", component, fmt);

    sz = ksnprintfv(flipbuf_flip(&fmtbuf), buf, MAX_BUFSZ_HLF, args);
    
    kprintf_put(level, flipbuf_top(&fmtbuf), sz);
}

void
kprintf_m(const char* component, const char* fmt, va_list args)
{
    int level;
    fmt = shift_level(fmt, &level);

    kprintf_ml(component, level, fmt, args);
}

void
kprintf_v(const char* component, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    kprintf_m(component, fmt, args);
    va_end(args);
}

static void
__twimap_kprintf_read(struct twimap* map)
{
    struct kp_records* __kprecs = twimap_data(map, struct kp_records*);

    /*
        XXX we can foreach all records in a single twimap read call,
            as records is monotonic increasing by design.
    */
    struct kp_entry *pos, *n;
    llist_for_each(pos, n, __kprecs->kp_ent_wp, ents)
    {
        time_t s = pos->time / 1000;
        time_t ms = pos->time % 1000;
        twimap_printf(map, "[%05d.%03d] %s\n", s, ms, pos->content);
    }
}

static void
kprintf_mapping_init()
{
    twimap_entry_simple(NULL, "kmsg", &kprecs, __twimap_kprintf_read);
}
EXPORT_TWIFS_PLUGIN(kprintf, kprintf_mapping_init);


void 
kprintf_dump_logs() {
    if (unlikely(!sysconsole)) {
        return;
    }

    struct kp_entry *pos, *n;
    llist_for_each(pos, n, kprecs.kp_ent_wp, ents)
    {
        __put_console(pos);
    }
}

__DEFINE_LXSYSCALL3(void, syslog, int, level, 
                    const char*, buf, unsigned int, size)
{
    kprintf_put(level, buf, size);
}