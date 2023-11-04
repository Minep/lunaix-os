#include <lunaix/fs/twifs.h>
#include <lunaix/fs/twimap.h>
#include <lunaix/syscall.h>
#include <lunaix/syslog.h>

#include <lunaix/lxconsole.h>

#include <klibc/strfmt.h>

#include "kp_records.h"

#define MAX_BUFSZ 512
#define MAX_BUFSZ_HLF 256
#define MAX_KPENT_NUM 1024

static char tmp_buf[MAX_BUFSZ];

static struct kp_records kprecs = {
    .kp_ents = { .ents = { .next = &kprecs.kp_ents.ents,
                           .prev = &kprecs.kp_ents.ents } },
    .max_recs = MAX_KPENT_NUM,
    .kp_ent_wp = &kprecs.kp_ents.ents
};

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
kprintf_ml(const char* component, int level, const char* fmt, va_list args)
{
    char* buf = &tmp_buf[MAX_BUFSZ_HLF];
    ksnprintf(buf, MAX_BUFSZ_HLF, "%s: %s", component, fmt);

    size_t sz = ksnprintfv(tmp_buf, buf, MAX_BUFSZ_HLF, args);
    kprec_put(&kprecs, level, tmp_buf, sz);

    // FIXME temp measure, get some output
    console_write_str(tmp_buf);
    console_write_char('\n');
}

void
kprintf_m(const char* component, const char* fmt, va_list args)
{
    int level;
    fmt = shift_level(fmt, &level);

    kprintf_ml(component, level, fmt, args);
}

static void
__twimap_kprintf_read(struct twimap* map)
{
    struct kp_records* kprecs = twimap_data(map, struct kp_records*);

    /*
        XXX we can foreach all records in a single twimap read call,
            as records is monotonic increasing by design.
    */
    struct kp_entry *pos, *n;
    llist_for_each(pos, n, kprecs->kp_ent_wp, ents)
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

__DEFINE_LXSYSCALL3(void, syslog, int, level, const char*, fmt, va_list, args)
{
    kprintf_ml("syslog", level, fmt, args);
}