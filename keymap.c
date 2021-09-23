#include "sdcc_keywords.h"
#include <stdint.h>
#include <string.h>
#include "kbdparse.h"
#include "keymap.h"
//#include "USBHost.h"

uint8_t __code s_keymap_us[30] = {
    '1', '2', '3', '4', '5', '6', '7', '8', '9', '0',
    0x0a, 0x1b, 0x08, 0x09, 0x20, '-', '=', '[', ']', '\\', 
    0, ';', '\'', '`', ',', '.', '/', 0, 0, 0
};
uint8_t __code s_keymap_us_s[30] = {
    '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', 
    0x0a, 0x1b, 0x08, 0xff, 0x20, '_', '+', '{', '}', '|', 
    0, ':', '"', '~', '<', '>', '?', 0, 0, 0
};
uint8_t __code s_keymap_jp[30] = {
    '1', '2', '3', '4', '5', '6', '7', '8', '9', '0',
    0x0a, 0x1b, 0x08, 0x09, 0x20, '-', '^', '@', '[', ']', 
    0, ';', ':', 0, ',', '.', '/', 0, 0, 0
};
uint8_t __code s_keymap_jp_s[30] = {
    '!', '"', '#', '$', '%', '&', '\'', '(', ')', 0, 
    0x0a, 0x1b, 0x08, 0xff, 0x20, '=', '~', '`', '{', '}', 
    0, '+', '*', 0, '<', '>', '?', 0, 0, 0
};
uint8_t __code s_keymap_num[16] = {
    '/', '*', '-', '+', 0x0a, '1', '2', '3', 
    '4', '5', '6', '7', '8', '9', '0', '.'
};
uint8_t __code s_keymap_num_ctrl[11] = {
    0x4d, 0x51, 0x4e, 0x50, 0x5d, 0x4f, 0x4a, 0x52,
    0x4b, 0x49, 0x4c
};

uint8_t __code *s_keymap_n = s_keymap_jp;
uint8_t __code *s_keymap_s = s_keymap_jp_s;

void keymap_setmap(uint8_t mapid)
{
    switch (mapid) {
        case KEYMAP_US:
            s_keymap_n = s_keymap_us;
            s_keymap_s = s_keymap_us_s;
            break;
        case KEYMAP_JP:
        default:
            s_keymap_n = s_keymap_jp;
            s_keymap_s = s_keymap_jp_s;
            break;
    }
}

uint8_t keymap_fill_escape(char __xdata *dst, const char __code *sequence)
{
    __xdata char *p = dst;
    *p++ = 0x1b;
    while (*sequence != 0) {
        *p++ = *sequence++;
    }
    return (p - dst);
}

uint8_t keymap_keynum_to_char(uint8_t keynum, uint8_t modifier, uint8_t locks, char __xdata *dst, uint8_t dstlen)
{
    if (dstlen < 8) return 0;

    uint8_t cn = 0;
    uint8_t rt = 0;
    uint8_t shifted = (modifier & (MODIFIER_SHIFT_L | MODIFIER_SHIFT_R)) != 0;
    uint8_t ctrld = (modifier & (MODIFIER_CTRL_L | MODIFIER_CTRL_R)) != 0;
    if (0x04 <= keynum && keynum <= 0x1d) {
        cn = 'a' - 4 + keynum;
        if (shifted ^ ((locks & LOCK_CAPS) != 0)) {
            cn -= 'a' - 'A';
        }
    } else if (0x1e <= keynum && keynum <= 0x39) {
        uint8_t __code *map = (shifted) ? s_keymap_s : s_keymap_n;
        cn = map[keynum - 0x1e];
        if (cn == 0xff) { // reverse tab
            rt = keymap_fill_escape(dst, "[Z");
            cn = 0;
        }
    } else if (0x54 <= keynum && keynum <= 0x63) { 
        // numpad
        if ((locks & LOCK_NUM) || (keynum <= 0x58)) {
            cn = s_keymap_num[keynum - 0x54];
        } else {
            keynum = s_keymap_num_ctrl[keynum - 0x59];
        }
    } else if (keynum == 0x87) {
        cn = '_';
    } else if (keynum == 0x89) {
        cn = (shifted) ? '|' : '\\';
    }
    if (cn != 0 && ctrld) {
        if ('a' <= cn && cn <= 'z') {
            cn -= 0x60;
        } else if ('A' <= cn && cn <= '_') {
            cn -= 0x40;
        } else switch (cn) {
            case '@': 
                // fill params and return here because null char cannot be handled correctly in normal path
                *dst = 0;
                return 1;
                break;
            case '?': 
                cn = 0x7f;
                break;
            default:
                cn = 0;
                break;
        }
    }
    if (cn == 0) {
        const char __code *p = "";
        switch (keynum) {
            case 0x3a: p = "OP"; break; // F1
            case 0x3b: p = "OQ"; break;
            case 0x3c: p = "OR"; break;
            case 0x3d: p = "OS"; break;
            case 0x3e: p = "[15~"; break;
            case 0x3f: p = "[17~"; break;
            case 0x40: p = "[18~"; break;
            case 0x41: p = "[19~"; break;
            case 0x42: p = "[20~"; break;
            case 0x43: p = "[21~"; break;
            case 0x44: p = "[23~"; break;
            case 0x45: p = "[24~"; break; // F12
            case 0x46: p = "[25~"; break; // print screen (as F13)
            case 0x47: p = "[26~"; break; // scroll lock (as F14)
            case 0x48: p = "[28~"; break; // pause/break (as F15)
            case 0x49: p = "[2~"; break; // insert
            case 0x4a: p = "[H"; break; // home
            case 0x4b: p = "[5~"; break; // page up
            case 0x4c: p = "[3~"; break; // delete
            case 0x4d: p = "[F"; break; // end
            case 0x4e: p = "[6~"; break; // page down
            case 0x4f: p = "[C"; break; // right
            case 0x50: p = "[D"; break; // left
            case 0x51: p = "[B"; break; // down
            case 0x52: p = "[A"; break; // up
            case 0x5d: p = "[G"; break; // keypad 5
            default: p = NULL; break;
        }
        if (p != NULL) {
            rt = keymap_fill_escape(dst, p);
        }
    }
    if (cn != 0) {
        *dst = cn;
        rt = 1;
    }
    return rt;
}
