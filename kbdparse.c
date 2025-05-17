#include "sdcc_keywords.h"
#include "kbdparse.h"
#include "util.h"


void kbdparse_init(KbdState *kbd)
{
    __xdata uint8_t *p = (__xdata uint8_t*)kbd;
    for (uint8_t i = 0; i < sizeof(KbdState); i++) {
        *p++ = 0;
    }
}

void kbdparse_hid_swapcaps(uint8_t __xdata *src, uint8_t len)
{
    if (len < 3) return;

    uint8_t dn_caps = 0;
    uint8_t dn_ctrl = (src[0] & 1);
    src[0] &= 0xfe;

    uint8_t di = 2;
    for (uint8_t si = 2; si < len && src[si] != 0; si++) {
        if (src[si] == 0x39) {
            dn_caps = 1;
            src[si] = 0;
        } else if (di != si) {
            src[di++] = src[si];
        } else {
            di++;
        }
    }
    if (dn_ctrl) {
        if (di < len) {
            src[di++] = 0x39;
        } else {
            src[len - 1] = 0x39;
        }
    }
    for (; di < len; di++) {
        src[di] = 0;
    }
    if (dn_caps) {
        src[0] |= 1;
    }
}

uint8_t kbdparse_hidinput(KbdState *kbd, uint16_t tm, uint8_t __xdata *src, uint8_t len, KeyEvent *evt_dst, uint8_t evt_dst_len)
{
#define append_event(EVTTYPE, ISMOD, KEYNUM, MODIFIER) \
{ \
    if (dst_pos < evt_dst_len) { \
        evt_dst[dst_pos].evttype = (EVTTYPE); \
        evt_dst[dst_pos].is_modifier = (ISMOD); \
        evt_dst[dst_pos].keynum = (KEYNUM); \
        evt_dst[dst_pos].modifier = (MODIFIER); \
        dst_pos++; \
    } \
}

    if (len != MAX_HID_PACKET_LEN) return (0);

    uint8_t dst_pos = 0;
    uint8_t modifier = src[0];
    kbd->lastDownModifier = modifier;
    kbd->lastDownKey = 0;
    kbd->lastKeyDownTm = tm;
    kbd->lastRepeatTm = 0;

    // see if modifier has changed
    if (src[0] != kbd->lastState[0]) {
        for (uint8_t i = 0; i < 8; i++) {
            uint8_t b = 1 << i;
            if ((src[0] & b) != (kbd->lastState[0] & b)) {
                if (src[0] & b) {
                    append_event(KEYEVENT_DOWN, 1, b, modifier);
                } else {
                    append_event(KEYEVENT_UP, 1, b, modifier);
                }
            }
        }
        kbd->lastState[0] = src[0];
    }

    // ignore phantom state
    if (src[2] != 0 && src[2] < 4) return dst_pos;

    // remove leading zeros (for compatibility with QMK firmware)
    uint8_t di = 2;
    uint8_t i;
    for (i = 2; i < len; i++) {
        if (src[i] != 0) {
            src[di++] = src[i];
        }
    }
    for (; di < len; di++) {
        src[di] = 0;
    }

    // get keys
    uint8_t pos_cur = 2;
    uint8_t pos_last = 2;
/*
    DEBUG_OUT("  LAST:");
    for (i = 0; i < 8; i++) DEBUG_OUT(" %02x", kbd->lastState[i]);
    DEBUG_OUT("\n");
    DEBUG_OUT("   CUR:");
    for (i = 0; i < 8; i++) DEBUG_OUT(" %02x", src[i]);
    DEBUG_OUT("\n");
*/

    while (pos_last < MAX_HID_PACKET_LEN && pos_cur < MAX_HID_PACKET_LEN) {
        if (src[pos_cur] != 0) {
            kbd->lastDownKey = src[pos_cur];
        }
        if (src[pos_cur] == kbd->lastState[pos_last]) {
            pos_cur++;
            pos_last++;
            continue;
        }

        uint8_t keycur = src[pos_cur];
        uint8_t keylast = kbd->lastState[pos_last];

        if (keycur == 0 && keylast == 0) {
            // end of key state
            break;
        } else if (keycur != 0 && keylast == 0) {
            // key was newly pressed
            append_event(KEYEVENT_DOWN, 0, keycur, modifier);
            pos_cur++;
        } else if (keycur == 0 && keylast != 0) {
            // last key was released
            append_event(KEYEVENT_UP, 0, keylast, modifier);
            pos_last++;
        } else {
            // some key have changed, see what's happened...
            while (pos_last < MAX_HID_PACKET_LEN && keycur != kbd->lastState[pos_last]) {
                // find matching key, unmatched key was released
                if (kbd->lastState[pos_last] != 0) {
                    append_event(KEYEVENT_UP, 0, kbd->lastState[pos_last], modifier);
                }
                pos_last++;
            }
        }
    }
    while (pos_cur < MAX_HID_PACKET_LEN && src[pos_cur] != 0) {
        kbd->lastDownKey = src[pos_cur];
        append_event(KEYEVENT_DOWN, 0, src[pos_cur++], modifier);
    }
    for (uint8_t i = 0; i < len; i++) {
        kbd->lastState[i] = src[i];
    }
    return dst_pos;
}

uint8_t kbdparse_getrepeat(KbdState *kbd, uint16_t tm, uint8_t delay, uint8_t intvl, KeyEvent *dst)
{
    if (intvl == 0) return 0;
    if (kbd->lastDownKey == 0) return 0;
    if (kbd->lastRepeatTm == 0) {
        uint16_t twait = tm - kbd->lastKeyDownTm;
        if (twait < delay) return 0;
    } else if (tm - kbd->lastRepeatTm < intvl) {
        return 0;
    }

    kbd->lastRepeatTm = tm;
    dst->evttype = KEYEVENT_REPEAT;
    dst->is_modifier = 0;
    dst->keynum = kbd->lastDownKey;
    dst->modifier = kbd->lastDownModifier;
    return 1;
}
