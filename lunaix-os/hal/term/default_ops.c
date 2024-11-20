#include <hal/term.h>

static void
__tpcap_default_set_speed(struct device* dev, speed_t speed)
{
    
}

static void
__tpcap_default_set_baseclk(struct device* dev, unsigned int base)
{
    
}

static void
__tpcap_default_set_cntrl_mode(struct device* dev, tcflag_t cflag)
{
    
}

struct termport_pot_ops default_termport_pot_ops = {
    .set_cntrl_mode = __tpcap_default_set_cntrl_mode,
    .set_clkbase = __tpcap_default_set_baseclk,
    .set_speed = __tpcap_default_set_speed
};