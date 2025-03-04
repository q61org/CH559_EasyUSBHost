#include "sdcc_keywords.h"
#include <stdint.h>
#include "USBHost.h"
#include "udev_hid.h"
#include "gamepad.h"

static uint8_t gamepad_get_bit(__xdata uint8_t *data, uint8_t pos)
{
    return ((data[pos >> 3] >> (pos & 7)) & 1);
}
static uint8_t gamepad_do_get_nbit(__xdata uint8_t *data, uint8_t pos, uint8_t len)
{
    uint8_t r = 0;
    for (uint8_t i = 0; i < len; i++) {
        r |= gamepad_get_bit(data, pos + i) << i;
    }
    return (r);
}
static uint8_t gamepad_get_4bit(__xdata uint8_t *data, uint8_t pos)
{
    if ((pos & 7) == 0) {
        return (data[pos >> 3] & 0x0f);
    } else if ((pos & 7) == 4) {
        return (data[pos >> 3] >> 4);
    }
    return gamepad_do_get_nbit(data, pos, 4);
}
static uint8_t gamepad_get_8bit(__xdata uint8_t *data, uint8_t pos)
{
    if ((pos & 7) == 0) {
        return data[pos >> 3];
    }
    return gamepad_do_get_nbit(data, pos, 8);
}
static uint8_t gamepad_get_nbit(__xdata uint8_t *data, uint8_t pos, uint8_t len)
{
    uint8_t r;
    switch (len) {
        case 4: r = gamepad_get_4bit(data, pos); break;
        case 8: r = gamepad_get_8bit(data, pos); break;
        default: r = gamepad_do_get_nbit(data, pos, len); break;
    }
    return (r);
}

uint8_t gamepad_parse_hid_data(UDevInterface *iface, __xdata uint8_t *data, uint8_t len, GamepadState *dst)
{
    static const __xdata uint8_t c_hat_tbl[8] = { 0x80, 0xff, 0xff, 0xff, 0x80, 0x00, 0x00, 0x00 };
    uint8_t bitpos = 0;
    uint8_t bitlen = (len >= 32) ? 255 : len * 8;
    uint8_t xyi = 0;
    uint8_t trigi = 0;
    uint8_t btni = 0;

    for (uint8_t i = 0; i < iface->spec.hid.num_reports; i++) {
        if (bitpos >= bitlen) break;
        HIDReportSpec *sp = &iface->spec.hid.reports[i];
        switch (sp->type) {
            case JOYSTICK_INPUT_TYPE_CONST: 
                bitpos += sp->size * sp->count; 
                break;
            case JOYSTICK_INPUT_TYPE_ID:
                bitpos += sp->size;
                break;
            case JOYSTICK_INPUT_TYPE_BUTTON:
                for (uint8_t k = 0; k < sp->count; k++) {
                    if (btni >= GAMEPAD_MAX_NUM_BUTTON) break;
                    if (bitpos >= bitlen) break;
                    uint8_t b = gamepad_get_bit(data, bitpos++);
                    if (b) {
                        if (dst->btns[btni] < 255) dst->btns[btni]++;
                    } else {
                        dst->btns[btni] = 0;
                    }
                    btni++;
                    dst->num_btns = btni;
                }
                break;
            case JOYSTICK_INPUT_TYPE_AXIS:
                for (uint8_t k = 0; k < sp->count; k++) {
                    if (xyi >= GAMEPAD_MAX_NUM_XY) break;
                    if (bitpos >= bitlen) break;
                    uint8_t b = gamepad_get_nbit(data, bitpos, sp->size);
                    bitpos += sp->size;
                    if (k & 1) {
                        dst->xys[xyi].y = b;
                        xyi++;
                    } else {
                        dst->xys[xyi].x = b;
                        dst->num_xys = xyi + 1;
                    }
                }
                break;
            case JOYSTICK_INPUT_TYPE_TRIGGER:
                for (uint8_t k = 0; k < sp->count; k++) {
                    if (trigi >= GAMEPAD_MAX_NUM_TRIGGER) break;
                    if (bitpos >= bitlen) break;
                    uint8_t b = gamepad_get_nbit(data, bitpos, sp->size);
                    bitpos += sp->size;
                    dst->trigs[trigi++] = b;
                    dst->num_trigs = trigi;
                }
                break;
            case JOYSTICK_INPUT_TYPE_HAT_DEG:
                for (uint8_t k = 0; k < sp->count; k++) {
                    if (xyi >= GAMEPAD_MAX_NUM_XY) break;
                    if (bitpos >= bitlen) break;
                    uint8_t b = gamepad_get_nbit(data, bitpos, sp->size);
                    bitpos += sp->size;
                    uint8_t dx, dy;
                    if (b < 8) {
                        dx = c_hat_tbl[b];
                        dy = c_hat_tbl[(b + 6) & 7];
                    } else {
                        dx = dy = 0x80;
                    }
                    dst->xys[xyi].x = dx;
                    dst->xys[xyi].y = dy;
                    dst->num_xys = ++xyi;
                }
                break;
            default:
                break;
        }
    }
    return (bitpos >> 3);
}

void gamepad_state_update(GamepadState *dst, GamepadState *src)
{
    dst->num_xys = src->num_xys;
    for (uint8_t i = 0; i < src->num_xys; i++) {
        dst->xys[i].x = src->xys[i].x;
        dst->xys[i].y = src->xys[i].y;
    }
    dst->num_trigs = src->num_trigs;
    for (uint8_t i = 0; i < src->num_trigs; i++) {
        dst->trigs[i] = src->trigs[i];
    }
    dst->num_btns = src->num_btns;
    for (uint8_t i = 0; i < src->num_btns; i++) {
        if (src->btns[i] == 0) {
            dst->btns[i] = 0;
        } else {
            if (dst->btns[i] < 255) {
                dst->btns[i]++;
            }
        }
    }
}

void gamepad_get_unified_digital_xy(GamepadState *src, GamepadXY *dst)
{
    dst->x = 0x80;
    dst->y = 0x80;

    // just use the first non-neutral axis
    for (uint8_t i = 0; i < src->num_xys; i++) {
        if (src->xys[i].x < 0x40 || src->xys[i].x >= 0xc0 || src->xys[i].y < 0x40 || src->xys[i].y >= 0xc0) {
            if (src->xys[i].x < 0x40)  {
                dst->x = 0;
            } else if (src->xys[i].x >= 0xc0) {
                dst->x = 0xff;
            }
            if (src->xys[i].y < 0x40)  {
                dst->y = 0;
            } else if (src->xys[i].y >= 0xc0) {
                dst->y = 0xff;
            }
            break;
        }
    }
}
