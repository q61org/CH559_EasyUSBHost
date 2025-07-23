#ifndef __GAMEPAD_H__
#define __GAMEPAD_H__

#include <stdint.h>

#define GAMEPAD_MAX_NUM_DPAD 4
#define GAMEPAD_MAX_NUM_XY 4
#define GAMEPAD_MAX_NUM_TRIGGER 4
#define GAMEPAD_MAX_NUM_BUTTON 16

#define GAMEPAD_DPAD_UP 1
#define GAMEPAD_DPAD_DOWN 2
#define GAMEPAD_DPAD_LEFT 4
#define GAMEPAD_DPAD_RIGHT 8

struct gamepad_xy_t {
    int8_t x;
    int8_t y;
};
typedef struct gamepad_xy_t __xdata GamepadXY;

struct gamepad_state_t {
    uint8_t num_dpads;
    uint8_t num_xys;
    uint8_t num_trigs;
    uint8_t num_btns;
    uint8_t reserved0;
    uint8_t unified_dpad;
    uint8_t dpads[GAMEPAD_MAX_NUM_DPAD];
    struct gamepad_xy_t xys[GAMEPAD_MAX_NUM_XY];
    uint8_t trigs[GAMEPAD_MAX_NUM_TRIGGER];
    uint8_t btns[GAMEPAD_MAX_NUM_BUTTON / 8];
};
typedef struct gamepad_state_t __xdata GamepadState;

uint8_t gamepad_parse_hid_data(UDevInterface *iface, __xdata uint8_t *data, uint8_t len, GamepadState *dst);
void gamepad_state_clear(GamepadState *dst);
void gamepad_state_update(GamepadState *dst, GamepadState *src);
//void gamepad_get_unified_digital_xy(GamepadState *src, GamepadXY *dst);
void gamepad_get_unified_dpad(GamepadState *src, uint8_t *dst);
uint8_t gamepad_state_isequal(GamepadState *a, GamepadState *b, uint8_t unified_only);

#endif
