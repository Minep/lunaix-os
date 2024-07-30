#ifndef __LUNAIX_LCNTL_H
#define __LUNAIX_LCNTL_H

#include <hal/term.h>

#define LCNTLF_SPECIAL_CHAR      0b000001
#define LCNTLF_CLEAR_INBUF       0b000010
#define LCNTLF_CLEAR_OUTBUF      0b000100
#define LCNTLF_STOP              0b001000

enum lcntl_dir {
    INBOUND,
    OUTBOUND
};

struct lcntl_state {
    struct term* tdev;
    tcflag_t _if;   // iflags
    tcflag_t _of;   // oflags
    tcflag_t _lf;   // local flags
    tcflag_t _cf;   // control flags
    tcflag_t _sf;   // state flags
    enum lcntl_dir direction;

    struct linebuffer* active_line;
    struct rbuffer* inbuf;
    struct rbuffer* outbuf;
    struct rbuffer* echobuf;
};

int  
lcntl_put_char(struct lcntl_state* state, char c);

static inline void
lcntl_set_flag(struct lcntl_state* state, int flags)
{
    state->_sf |= flags;
}

static inline void
lcntl_raise_line_event(struct lcntl_state* state, int event)
{
    state->active_line->sflags |= event;
}

static inline void
lcntl_unset_flag(struct lcntl_state* state, int flags)
{
    state->_sf &= ~flags;
}

static inline bool
lcntl_test_flag(struct lcntl_state* state, int flags)
{
    return !!(state->_sf & flags);
}

static inline bool
lcntl_outbound(struct lcntl_state* state)
{
    return (state->direction == OUTBOUND);
}

static inline bool
lcntl_inbound(struct lcntl_state* state)
{
    return (state->direction == INBOUND);
}

static inline bool
lcntl_check_echo(struct lcntl_state* state, int echo_type)
{
    return lcntl_inbound(state) && (state->_lf & echo_type);
}


#endif /* __LUNAIX_LCNTL_H */
