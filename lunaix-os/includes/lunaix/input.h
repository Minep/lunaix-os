#ifndef __LUNAIX_INPUT_H
#define __LUNAIX_INPUT_H

#include <lunaix/clock.h>
#include <lunaix/device.h>
#include <lunaix/ds/llist.h>
#include <lunaix/ds/waitq.h>
#include <lunaix/types.h>

// event should propagate further
#define INPUT_EVT_NEXT 0
// event should be captured here
#define INPUT_EVT_CATCH 1

// key pressed (key-type device)
#define PKT_PRESS 0x1
// key released (key-type device)
#define PKT_RELEASE 0x2
// vector (e.g. mice wheel scroll, mice maneuver)
#define PKT_VECTOR 0x3

struct input_evt_pkt
{
    u32_t pkt_type;   // packet type
    u32_t scan_code;  // hardware raw code
    u32_t sys_code;   // driver translated code
    time_t timestamp; // event timestamp
};

struct input_device
{
    struct device* dev_if;            // device interface
    struct input_evt_pkt current_pkt; // recieved event packet
    waitq_t readers;                  // reader wait queue
};

typedef int (*input_evt_cb)(struct input_device* dev);

struct input_evt_chain
{
    struct llist_header chain;
    input_evt_cb evt_cb;
};

void
input_init();

void
input_fire_event(struct input_device* idev, struct input_evt_pkt* pkt);

void
input_add_listener(input_evt_cb listener);

struct input_device*
input_add_device(char* name_fmt, ...);

#endif /* __LUNAIX_INPUT_H */
