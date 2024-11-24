#ifndef __STUB_LUNAIX_OWLOYSIUS_H
#define __STUB_LUNAIX_OWLOYSIUS_H


#define owloysius_fetch_init(func, call_stage)  \
    void init_export_##func() { func(); }

#define invoke_init_function(stage)

static inline void
initfn_invoke_sysconf()
{
    return;
}

static inline void
initfn_invoke_earlyboot()
{
    return;
}

static inline void
initfn_invoke_boot()
{
    return;
}

static inline void
initfn_invoke_postboot()
{
    return;
}


#endif /* __STUB_LUNAIX_OWLOYSIUS_H */
