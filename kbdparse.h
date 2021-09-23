#ifndef __KBDPARSE_H__
#define __KBDPARSE_H__

#include <stdint.h>

#define MAX_KEYPRESS 6
#define MAX_HID_PACKET_LEN (2 + MAX_KEYPRESS)

struct kbdstate_t {
    uint8_t lastState[MAX_HID_PACKET_LEN];
    uint8_t lastKeyDown;
    uint16_t lastKeyDownTm;
    uint16_t lastRepeatTm;
    uint8_t lastDownKey;
    uint8_t lastDownModifier;
    uint8_t locks;
};
typedef struct kbdstate_t __xdata KbdState;

enum {
    KEYEVENT_NONE = 0,
    KEYEVENT_DOWN = 1,
    KEYEVENT_UP = 2,
    KEYEVENT_REPEAT = 4
};

#define LOCK_NUM 1
#define LOCK_CAPS 2
#define LOCK_SCROLL 4

struct keyevent_t {
    uint8_t evttype;
    uint8_t is_modifier;
    uint8_t keynum; // scan code or single modifier
    uint8_t modifier; // current modifier state (if this event is about a modifier, right after this event)
};
typedef struct keyevent_t __xdata KeyEvent;

void kbdparse_init(KbdState *kbd);
void kbdparse_hid_swapcaps(uint8_t __xdata *src, uint8_t len);

uint8_t kbdparse_hidinput(KbdState *kbd, uint16_t tm, const uint8_t __xdata *src, uint8_t len, KeyEvent *evt_dst, uint8_t evt_dst_len);
uint8_t kbdparse_getrepeat(KbdState *kbd, uint16_t tm, uint8_t delay, uint8_t intvl, KeyEvent *dst);


#endif
