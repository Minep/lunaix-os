#ifndef __LUNAIX_MC146818A_H
#define __LUNAIX_MC146818A_H

/*
    FIXME the drivers should go into ldga
*/

#include <hal/hwrtc.h>

struct hwrtc*
mc146818a_rtc_context();

#endif /* __LUNAIX_MC146818A_H */
