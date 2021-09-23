#ifndef __KEYMAP_H__
#define __KEYMAP_H__

enum {
    MODIFIER_CTRL_L = 0x01,
    MODIFIER_SHIFT_L = 0x02,
    MODIFIER_ALT_L = 0x04,
    MODIFIER_GUI_L = 0x08,
    MODIFIER_CTRL_R = 0x10,
    MODIFIER_SHIFT_R = 0x20,
    MODIFIER_ALT_R = 0x40,
    MODIFIER_GUI_R = 0x80
};

#define KEYMAP_JP 0
#define KEYMAP_US 1

void keymap_setmap(uint8_t mapid);
uint8_t keymap_keynum_to_char(uint8_t keynum, uint8_t modifier, uint8_t locks, char __xdata *dst, uint8_t dstlen);

#endif
