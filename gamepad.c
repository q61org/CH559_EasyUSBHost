#include "sdcc_keywords.h"
#include <stdint.h>
#include "USBHost.h"
#include "udev_hid.h"
#include "gamepad.h"
#include "util.h"

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
    static const __xdata uint8_t c_hat_tbl[8] = { 0x01, 0x09, 0x08, 0x0a, 0x02, 0x06, 0x04, 0x05 };
    uint8_t bitpos = 0;
    uint8_t bitlen = (len >= 32) ? 255 : len * 8;
    uint8_t dpadi = 0;
    uint8_t xyi = 0;
    uint8_t trigi = 0;
    uint8_t btni = 0;
//    uint8_t udbtn[4] = {0, 0, 0, 0};
//    uint8_t udbtn_done = 0;
    uint8_t b = 0;

    for (uint8_t i = 0; i < iface->spec.hid.num_reports; i++) {
        if (bitpos >= bitlen) break;
        HIDReportSpec *sp = &iface->spec.hid.reports[i];
        switch (sp->type) {
            case JOYSTICK_INPUT_TYPE_CONST: 
                //DEBUG_OUT(" C%d", bitpos);
                bitpos += sp->size * sp->count; 
                break;
            case JOYSTICK_INPUT_TYPE_ID:
                bitpos += sp->size;
                break;
            case JOYSTICK_INPUT_TYPE_BUTTON:
                //DEBUG_OUT(" B%d", bitpos);
                for (uint8_t k = 0; k < sp->count; k++) {
                    if (bitpos >= bitlen) break;
                    b = gamepad_get_bit(data, bitpos++);
                    if (btni >= GAMEPAD_MAX_NUM_BUTTON) continue;
                    if (b) {
                        dst->btns[btni >> 3] |= (1 << (btni & 7));
                    } else {
                        dst->btns[btni >> 3] &= (0xff ^ (1 << (btni & 7)));
                    }
                    btni++;
                    dst->num_btns = btni;
                }
                break;
            case JOYSTICK_INPUT_TYPE_AXIS:
            case JOYSTICK_INPUT_TYPE_AXIS_POSNEG_16BIT:
                for (uint8_t k = 0; k < sp->count; k++) {
                    //DEBUG_OUT(" X%d", bitpos);
                    if (bitpos >= bitlen) break;
                    if (sp->type == JOYSTICK_INPUT_TYPE_AXIS_POSNEG_16BIT) {
                        b = gamepad_get_nbit(data, bitpos + 8, 8);
                        bitpos += 16;
                        if (k & 1) b ^= 0xff;
                    //    b ^= (k & 1) ? 0xff : 0x00;
                    //    b ^= (k & 1) ? 0x7f : 0x80;
                    } else {
                        b = gamepad_get_nbit(data, bitpos, sp->size);
                        bitpos += sp->size;
                        b ^= 0x80;
                    }
                    if (xyi >= GAMEPAD_MAX_NUM_XY) continue;
                    //DEBUG_OUT(" @%d:%02x", bitpos, data[bitpos>>3]);
                    if (k & 1) {
                        dst->xys[xyi].y = b;
                        //DEBUG_OUT(" b=%02x XY[%d]:%02x,%02x;", b, xyi, dst->xys[xyi].x, dst->xys[xyi].y);
                        /*if (!udbtn_done && (dst->xys[xyi].x < 0x40 || dst->xys[xyi].x >= 0xc0 || dst->xys[xyi].y < 0x40 || dst->xys[xyi].y >= 0xc0)) {
                            if (dst->xys[xyi].y < 0x40) {
                                udbtn[0] = 1;
                            } else if (dst->xys[xyi].y >= 0xc0) {
                                udbtn[1] = 1;
                            }
                            if (dst->xys[xyi].x < 0x40) {
                                udbtn[2] = 1;
                            } else if (dst->xys[xyi].x >= 0xc0) {
                                udbtn[3] = 1;
                            }
                            udbtn_done = 1;
                        } */                       
                        xyi++;
                    } else {
                        dst->xys[xyi].x = b;
                        //DEBUG_OUT(" b=%02x xyi=%d;", b, xyi);
                        dst->num_xys = xyi + 1;
                    }
                }
                break;
            case JOYSTICK_INPUT_TYPE_TRIGGER:
                for (uint8_t k = 0; k < sp->count; k++) {
                    //DEBUG_OUT(" T%d", bitpos);
                    if (bitpos >= bitlen) break;
                    b = gamepad_get_nbit(data, bitpos, sp->size);
                    bitpos += sp->size;
                    if (trigi >= GAMEPAD_MAX_NUM_TRIGGER) continue;
                    dst->trigs[trigi++] = b;
                    dst->num_trigs = trigi;
                }
                break;
            case JOYSTICK_INPUT_TYPE_HAT_DEG:
            case JOYSTICK_INPUT_TYPE_DPAD:
                for (uint8_t k = 0; k < sp->count; k++) {
                    //DEBUG_OUT(" H%d", bitpos);
                    if (bitpos >= bitlen) break;
                    b = gamepad_get_nbit(data, bitpos, sp->size);
                    bitpos += sp->size;
                    if (dpadi >= GAMEPAD_MAX_NUM_DPAD) continue;
                    if (sp->type == JOYSTICK_INPUT_TYPE_HAT_DEG) {
                        if (b < 8) {
                            b = c_hat_tbl[b];
                        } else {
                            b = 0;
                        }
                    }
                    //uint8_t nonzero = 0;
                    for (uint8_t n = 0; n < 4; n++) {
                        uint8_t bpress = b & (1 << n);
                        if (bpress) {
                         //   nonzero = 1;
                            if (dst->dpads[dpadi].btn[n] < 255) {
                                dst->dpads[dpadi].btn[n]++;
                            }
                        } else {
                            dst->dpads[dpadi].btn[n] = 0;
                        }
                    }
                   /* if (!udbtn_done && nonzero) {
                        for (uint8_t n = 0; n < 4; n++) {
                            udbtn[n] = dst->dpads[dpadi].btn[n];
                        }
                        udbtn_done = 1;
                    }*/
                    dst->num_dpads = ++dpadi;
                }
                break;
            default:
                break;
        }
    }
    gamepad_get_unified_dpad(dst, &dst->unified_dpad);
    return (bitpos >> 3);
}

void gamepad_state_clear(GamepadState *dst)
{
    dst->num_dpads = 0;
    dst->num_xys = 0;
    dst->num_trigs = 0;
    dst->num_btns = 0;
    dst->unified_dpad.btn[0] = 0;
    dst->unified_dpad.btn[1] = 0;
    dst->unified_dpad.btn[2] = 0;
    dst->unified_dpad.btn[3] = 0;
    for (uint8_t i = 0; i < GAMEPAD_MAX_NUM_DPAD; i++) {
        dst->dpads[i].btn[0] = 0;
        dst->dpads[i].btn[1] = 0;
        dst->dpads[i].btn[2] = 0;
        dst->dpads[i].btn[3] = 0;
    }
    for (uint8_t i = 0; i < GAMEPAD_MAX_NUM_BUTTON / 8; i++) {
        dst->btns[i] = 0;
    }
}

void gamepad_state_update(GamepadState *dst, GamepadState *src)
{
    for (uint8_t k = 0; k < 4; k++) {
        if (src->unified_dpad.btn[k] == 0) {
            dst->unified_dpad.btn[k] = 0;
        } else {
            if (dst->unified_dpad.btn[k] < 255) {
                dst->unified_dpad.btn[k]++;
            }
        }
    }
    dst->num_dpads = src->num_dpads;
    for (uint8_t i = 0; i < src->num_dpads; i++) {
        for (uint8_t k = 0; k < 4; k++) {
            if (src->dpads[i].btn[k] == 0) {
                dst->dpads[i].btn[k] = 0;
            } else {
                if (dst->dpads[i].btn[k] < 255) {
                    dst->dpads[i].btn[k]++;
                }
            }
        }
    }
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
    for (uint8_t i = 0; i < GAMEPAD_MAX_NUM_BUTTON / 8; i++) {
        dst->btns[i] = src->btns[i];
    }
}

void gamepad_get_unified_dpad(GamepadState *src, GamepadDPad *dst)
{
    dst->btn[0] = 0;
    dst->btn[1] = 0;
    dst->btn[2] = 0;
    dst->btn[3] = 0;

    // just use the first non-neutral axis
    for (uint8_t i = 0; i < src->num_dpads; i++) {
        if (src->dpads[i].dir.up || src->dpads[i].dir.down || src->dpads[i].dir.left || src->dpads[i].dir.right) {
            if (src->dpads[i].dir.left) {
                dst->dir.left = 1;
            } else if (src->dpads[i].dir.right) {
                dst->dir.right = 1;
            }
            if (src->dpads[i].dir.up) {
                dst->dir.up = 1;
            } else if (src->dpads[i].dir.down) {
                dst->dir.down = 1;
            }
            break;
        }
    }
    if (dst->dir.up || dst->dir.down || dst->dir.left || dst->dir.right) return;

    for (uint8_t i = 0; i < src->num_xys; i++) {
        if (src->xys[i].x < -64 || src->xys[i].x >= 64 || src->xys[i].y < -64 || src->xys[i].y >= 64) {
            if (src->xys[i].x < -64)  {
                dst->dir.left = 1;
            } else if (src->xys[i].x >= 64) {
                dst->dir.right = 1;
            }
            if (src->xys[i].y < -64)  {
                dst->dir.up = 1;
            } else if (src->xys[i].y >= 64) {
                dst->dir.down = 1;
            }
            break;
        }
    }

}

uint8_t gamepad_state_isequal(GamepadState *a, GamepadState *b, uint8_t unified_only) 
{
    uint8_t i, k;
    if (a->num_dpads != b->num_dpads) return 0;
    if (a->num_xys != b->num_xys) return 0;
    if (a->num_trigs != b->num_trigs) return 0;
    if (a->num_btns != b->num_btns) return 0;
    if (unified_only) {
        if (a->btns[0] != b->btns[0]) return 0;
        if ((a->btns[1] & 0x0f) != (b->btns[1] & 0x0f)) return 0;
        for (k = 0; k < 4; k++) {
            if (!a->unified_dpad.btn[k] != !b->unified_dpad.btn[k]) return 0;
        }
        return 1;
    }
    
    for (i = 0; i < GAMEPAD_MAX_NUM_BUTTON / 8; i++) {
        if (!a->btns[i] != !b->btns[i]) return 0;
    }
    for (i = 0; i < a->num_dpads; i++) {
        for (k = 0; k < 4; k++) {
            if (!a->dpads[i].btn[k] != !b->dpads[i].btn[k]) return 0;
        }
    }
    for (i = 0; i < a->num_xys; i++) {
        if (a->xys[i].x != b->xys[i].x) return 0;
        if (a->xys[i].y != b->xys[i].y) return 0;
    }
    for (i = 0; i < a->num_trigs; i++) {
        if (a->trigs[i] != b->trigs[i]) return 0;
    }
    return 1;
}
