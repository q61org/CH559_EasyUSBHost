#ifndef __GAMEPAD_H__
#define __GAMEPAD_H__

#include <stdint.h>

#define GAMEPAD_MAX_NUM_XY 4
#define GAMEPAD_MAX_NUM_TRIGGER 4
#define GAMEPAD_MAX_NUM_BUTTON 16

struct gamepad_xy_t {
    uint8_t x;
    uint8_t y;
};
typedef struct gamepad_xy_t __xdata GamepadXY;

struct gamepad_state_t {
    uint8_t num_xys;
    uint8_t num_trigs;
    uint8_t num_btns;
    struct gamepad_xy_t xys[GAMEPAD_MAX_NUM_XY];
    uint8_t trigs[GAMEPAD_MAX_NUM_TRIGGER];
    uint8_t btns[GAMEPAD_MAX_NUM_BUTTON];
};
typedef struct gamepad_state_t __xdata GamepadState;

uint8_t gamepad_parse_hid_data(UDevInterface *iface, __xdata uint8_t *data, uint8_t len, GamepadState *dst);
void gamepad_state_update(GamepadState *dst, GamepadState *src);
void gamepad_get_unified_digital_xy(GamepadState *src, GamepadXY *dst);

#endif
