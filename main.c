#include "sdcc_keywords.h"

typedef unsigned char *PUINT8;
typedef unsigned char __xdata *PUINT8X;
typedef const unsigned char __code *PUINT8C;
typedef unsigned char __xdata UINT8X;
typedef unsigned char  __data             UINT8D;

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "CH559.h"
#include "util.h"
#include "USBHost.h"
#include "uart0.h"
#include "uart1.h"
#include "ringbuf.h"
#include "udev_hid.h"
#include "udev_hub.h"
#include "gamepad.h"


#define VERSION_BYTE 0x04
#define VERSION_STR  "v20221123"


// ==== TICKS ====
__xdata volatile uint8_t g_subticks;
__xdata volatile uint32_t g_ticks;
__xdata volatile uint8_t g_leddecr;

void ticks_init()
{
    g_ticks = 0;
    TH0 = 0xf0;
    TL0 = 0xbe;
    TMOD = 1;
    TR0 = 1;
}

void g_rb_out_startsend();
INTERRUPT_USING(timer0_isr, INT_NO_TMR0, 1)
{ // 1024 counts per second
    TH0 = 0xf0;
    TL0 = 0xbe;
    g_subticks = ++g_subticks & 0x07;
    if (g_subticks == 0) {
        ++g_ticks;
        if (g_leddecr) --g_leddecr;
    }
    g_rb_out_startsend();
}

volatile inline uint8_t subticks8()
{ // 1024 counts per second
    return (((uint8_t)(g_ticks & 0x1f) << 3) | g_subticks);
}

volatile inline uint32_t ticks()
{ // 128 counts per second
    return g_ticks;
}

volatile inline uint32_t seconds()
{
    return (g_ticks >> 7);
}


// ================

#define p3_assert_interrupt()  (P3 &= 0xfb)
#define p3_clear_interrupt()   (P3 |= 4)
#define p3_assert_inact()      (P3 &= 0xfe)
#define p3_clear_inact()       (P3 |= 1)
#define p3_assert_detect()     (P3 &= 0xfd)
#define p3_clear_detect()      (P3 |= 2)

// ================

RingBuf g_rb_out;
uint8_t g_rb_out_sending = 0;

void g_rb_out_startsend()
{
    if (g_rb_out_sending) return;
    if (!RINGBUF_AVAILABLE(&g_rb_out)) return;
    SER1_IER |= bIER_THR_EMPTY;
    g_rb_out_sending = 1;
}

// ================

INTERRUPT_USING(uart1_isr, INT_NO_UART1, 2)
{
    if (!RINGBUF_AVAILABLE(&g_rb_out)) {
        SER1_IER &= ~bIER_THR_EMPTY;
        g_rb_out_sending = 0;
        return;
    }
    p3_clear_inact();
    g_leddecr = 6;
    uint8_t b;
    RINGBUF_READ(&g_rb_out, b);
    UART1SendAsync(b);
}

// ================

uint8_t __xdata g_poll_req = 0;
uint8_t __xdata g_poll_mode = 0;

INTERRUPT_USING(gpio_isr, INT_NO_GPIO, 3)
{
    g_poll_req = 1;
}

// ================

char bin2hexchar(uint8_t d)
{
    d &= 0x0f;
    return (d < 10) ? '0' + d : 'a' - 10 + d;
}

uint8_t hexchar2bin(const __xdata char *str)
{
    if (str[0] == 0 || str[1] == 0) return 0;
    uint8_t r = 0;
    for (uint8_t i = 0; i < 2; i++) {
        r <<= 4;
        char c = str[i];
        if ('0' <= c && c <= '9') {
            r |= (c - '0');
        } else if ('a' <= c && c <= 'f') {
            r |= (c - 'a' + 10);
        } else if ('A' <= c && c <= 'F') {
            r |= (c - 'A' + 10);
        }
    }
    return r;
}

#define MAX_NUM_KEYBOARDS 8
int8_t __xdata g_kbd_devIndex[MAX_NUM_KEYBOARDS];
uint8_t __xdata g_kbd_devAddr[MAX_NUM_KEYBOARDS];
uint8_t __xdata g_numKbds;
GamepadState g_state[MAX_NUM_KEYBOARDS];
uint8_t __xdata g_kbd_isXinput[MAX_NUM_KEYBOARDS];
uint8_t __xdata g_kbd_interfNo[MAX_NUM_KEYBOARDS];

#define OUTPUT_FORMAT_WITHCOUNT  1
#define OUTPUT_FORMAT_FULLLAYOUT 2

#define kbd_maxcount()           (sizeof(g_kbd_devIndex))
#define kbd_isconnectedat(INDEX) (g_kbd_devIndex[INDEX] >= 0)

int8_t findFreeKbdStateIndex()
{
    for (uint8_t i = 0; i < kbd_maxcount(); i++) {
        if (!kbd_isconnectedat(i)) return i;
    }
    return -1;
}

void usbAttachCallback(uint8_t devIndex, USBDevice *dev, uint8_t is_attach)
{
    DEBUG_OUT("CALLBACK: dev[%d] @%d, is_attach=%d\n", devIndex, dev->address, is_attach);
    p3_clear_interrupt();
    if (is_attach) {
        if (1) { //dev->vid_h == 0x04 || dev->vid_l == 0x5e) {
            int8_t idx = findFreeKbdStateIndex();
            if (idx < 0) {
                DEBUG_OUT("CALLBACK: no more keyboard!\n");
                return;
            }
            g_kbd_devIndex[idx] = devIndex;
            g_kbd_devAddr[idx] = dev->address;
            if (dev->class == 255) {
                DEBUG_OUT("CALLBACK: xinput mode\n");
                g_kbd_isXinput[idx] = 1;
            } else {
                DEBUG_OUT("CALLBACK: normal hid mode\n");
                g_kbd_isXinput[idx] = 0;
            }
            g_kbd_interfNo[idx] = 0;
            for (uint8_t i = 0; i < MAX_INTERFACES_PER_DEVICE; i++) {
                if (dev->iface[i].ep_in) {
                    g_kbd_interfNo[idx] = i;
                    break;
                }
            }
            DEBUG_OUT("CALLBACK: using interf index %d\n", g_kbd_interfNo[idx]);
            gamepad_state_clear(&g_state[idx]);
            g_numKbds++;
            DEBUG_OUT("CALLBACK: %d gamepad(s) now connected\n", g_numKbds);
            uint8_t r = hiddevice_start_input(devIndex, 0);
            DEBUG_OUT("start input: %d\n", r);
        }
    }
#if 0
        for (uint8_t i = 0; i < dev->num_ifaces; i++) {
            DEBUG_OUT("CALLBACK: usage %d\n", dev->iface[i].usage);
            if (dev->vid_h != 0x04 || dev->vid_l != 0x5e) {
                continue;
            }
            /*if (dev->iface[i].usage != Usage_JOYSTICK) {
                continue;
            }*/
            int8_t idx = findFreeKbdStateIndex();
            if (idx < 0) {
                DEBUG_OUT("CALLBACK: no more keyboard!\n");
                return;
            }
            g_kbd_devIndex[idx] = devIndex;
            g_kbd_devAddr[idx] = dev->address;
            //kbdparse_init(&g_kbd[idx]);
            g_numKbds++;
            DEBUG_OUT("CALLBACK: %d keyboard(s) now connected\n", g_numKbds);

            if (g_raw_mode) {
                ringbuf_write(&g_rb_out, bin2hexchar(dev->address >> 4));
                ringbuf_write(&g_rb_out, bin2hexchar(dev->address));
                ringbuf_write(&g_rb_out, 'A');
                ringbuf_write(&g_rb_out, '0');
                ringbuf_write(&g_rb_out, '0');
                ringbuf_write(&g_rb_out, ';');
            }
            uint8_t locks = g_default_locks;
            if (!g_raw_mode || !cfg_separatelock()) for (int8_t k = 0; k < kbd_maxcount(); k++) {
                if (k == idx) continue;
                if (!kbd_isconnectedat(k)) continue;
                locks = g_kbd[k].locks;
                break;
            }
            g_kbd[idx].locks = locks;

            //uint8_t r = hiddevice_start_input(devIndex, Usage_JOYSTICK);
            //DEBUG_OUT("start input: %d\n", r);
        //    setHIDDeviceLED(devIndex, Usage_KEYBOARD, locks);
            if (g_raw_mode) {
                ringbuf_write(&g_rb_out, bin2hexchar(dev->address >> 4));
                ringbuf_write(&g_rb_out, bin2hexchar(dev->address));
                ringbuf_write(&g_rb_out, 'L');
                ringbuf_write(&g_rb_out, '0');
                ringbuf_write(&g_rb_out, bin2hexchar(locks));
                ringbuf_write(&g_rb_out, ';');
            }
            return;
        }
    }
#endif
     else {
        for (uint8_t i = 0; i < kbd_maxcount(); i++) {
            if (g_kbd_devIndex[i] == (int8_t)devIndex) {
                g_kbd_devIndex[i] = -1;
                --g_numKbds;
                DEBUG_OUT("CALLBACK: disconnect, %d keyboard(s) now connected\n", g_numKbds);
            }
        }
    }
}

// ================

uint8_t strlen_xdata_s(const __xdata char *str, uint8_t maxlen)
{
    uint8_t r = 0;
    while (r < maxlen) {
        if (*str++ == 0) break;
        r++;
    }
    return r;
}

#define UART_CMD_MAXLEN 8

void uartcmd_process(const __xdata char *cmd)
{
    uint8_t len = strlen_xdata_s(cmd, UART_CMD_MAXLEN);
    DEBUG_OUT("UART command [%d] %s\n", len, cmd);
    if (cmd[0] == '!') {
        if (cmd[1] == 's' && len == 2) {
            static __xdata uint8_t id[8];
            get_chip_id(id);
            for (uint8_t i = 0; i < 8; i++) {
                ringbuf_write(&g_rb_out, bin2hexchar(id[7 - i] >> 4));
                ringbuf_write(&g_rb_out, bin2hexchar(id[7 - i]));
            }
            ringbuf_write(&g_rb_out, 0x0a);
        } else if (cmd[1] == 'v' && len == 2) {
            const char *v = VERSION_STR;
            while (*v != 0) {
                ringbuf_write(&g_rb_out, *v++);
            }
            ringbuf_write(&g_rb_out, 0x0a);
        }
    }
    if (cmd[0] == 'P') {
        if (cmd[1] == '0') {
            g_poll_req = 0;
            g_poll_mode = 0;
        } else if (cmd[1] == '1') {
            g_poll_req = 1;
        }
    }
#if 0
    if (cmd[0] == 'C') {
        if (len != 5) return;
        uint8_t addr = hexchar2bin(&cmd[1]);
        uint8_t val = hexchar2bin(&cmd[3]);
        cfg_writereg_and_update(addr, val);
    } else if (g_raw_mode && cfg_separatelock()) {
        if (len != 5) return;
        if (cmd[2] != 'L') return;
        uint8_t addr = hexchar2bin(&cmd[0]);
        uint8_t lk = hexchar2bin(&cmd[3]);
        kbd_updatelocks(lk, addr);
    } else {
        if (cmd[0] != 'L') return;
        if (len != 3) return;
        uint8_t lk = hexchar2bin(&cmd[1]);
        kbd_updatelocks(lk, 0);
        spi_update_statusreg(g_numKbds > 0, lk);
    }
#endif
}

void rb_out_hex2char(RingBuf *rb, uint8_t v)
{
    uint8_t d = v >> 4;
    ringbuf_write(rb, (d < 10) ? '0' + d : 'a' - 10 + d);
    d = v & 0x0f;
    ringbuf_write(rb, (d < 10) ? '0' + d : 'a' - 10 + d);
}
void rb_out_hex1char(RingBuf *rb, uint8_t v)
{
    uint8_t d = v & 0x0f;
    ringbuf_write(rb, (d < 10) ? '0' + d : 'a' - 10 + d);
}

void output_gpstate(GamepadState *pad, uint8_t devaddr, uint8_t fmt)
{
    uint8_t d, i;
    rb_out_hex2char(&g_rb_out, devaddr);

    d = 0;
    for (uint8_t k = 0; k < 4; k++) {
        d |= (pad->unified_dpad.btn[k] != 0) ? (1 << k) : 0;
    }
    ringbuf_write(&g_rb_out, 'G');
    rb_out_hex1char(&g_rb_out, d);

    if ((fmt & OUTPUT_FORMAT_WITHCOUNT) == 0) {
        ringbuf_write(&g_rb_out, 'N');
        for (uint8_t i = 0; i < 12; i += 4) {
            d = 0;
            for (uint8_t k = 0; k < 4; k++) {
                if (i + k >= pad->num_btns) break;
                d |= (pad->btns[i + k] != 0) ? (1 << k) : 0;
            }
            rb_out_hex1char(&g_rb_out, d);
        }
    }

    switch (fmt) {
        case OUTPUT_FORMAT_FULLLAYOUT:
            for (i = 0; i < pad->num_dpads; i++) {
                d = 0;
                for (uint8_t k = 0; k < 4; k++) {
                    d |= (pad->dpads[i].btn[k] != 0) ? (1 << k) : 0;
                }
                ringbuf_write(&g_rb_out, 'H');
                rb_out_hex1char(&g_rb_out, d);
            }
            break;

        case OUTPUT_FORMAT_FULLLAYOUT | OUTPUT_FORMAT_WITHCOUNT:
            ringbuf_write(&g_rb_out, 'g');
            for (uint8_t k = 0; k < 4; k++) {
                rb_out_hex2char(&g_rb_out, pad->unified_dpad.btn[k]);
            }
            for (i = 0; i < pad->num_dpads; i++) {
                ringbuf_write(&g_rb_out, 'h');
                for (uint8_t k = 0; k < 4; k++) {
                    rb_out_hex2char(&g_rb_out, pad->dpads[i].btn[k]);
                }
            }
            ringbuf_write(&g_rb_out, 'n');
            for (i = 0; i < pad->num_btns; i++) {
                rb_out_hex2char(&g_rb_out, pad->btns[i]);
            }
            break;
    }

    if (fmt & OUTPUT_FORMAT_FULLLAYOUT) {
        for (i = 0; i < pad->num_xys; i++) {
            ringbuf_write(&g_rb_out, 'X');
            rb_out_hex2char(&g_rb_out, pad->xys[i].x);
            rb_out_hex2char(&g_rb_out, pad->xys[i].y);
        }
        for (i = 0; i < pad->num_trigs; i++) {
            ringbuf_write(&g_rb_out, 'T');
            rb_out_hex2char(&g_rb_out, pad->trigs[i]);
        }

    }
    ringbuf_write(&g_rb_out, ';');
}

void main()
{
    if(!(P4_IN & (1 << 6))) {
        runBootloader();
    }
    ticks_init();
    ET0 = 1;

    static __xdata uint32_t t_last = 255;
    //uint8_t s;
    static __xdata uint8_t uart_buf_pos = 0;
    static __xdata uint8_t kbdconnected = 0;
    static __xdata uint8_t tgl = 0;
    static __xdata uint8_t uartcmd[UART_CMD_MAXLEN];
    static __xdata uint8_t uartcmd_pos = 0;

    P0_PU = 0xf7;
    P1_PU = 0xff;
    P2_PU = 0x3f;
    P2_DIR = 0x80;
    P3_PU = 0xff;
    P3_DIR = 0x00;
    P3 = 0xff;
    P4_PU = 0xff;

    PORT_CFG &= 0xfb; // set P2 to push-pull mode
    
    for (uint8_t i = 0; i < sizeof(g_kbd_devIndex); i++) {
        g_kbd_devIndex[i] = -1;
    }
    for (uint8_t i = 0; i < MAX_NUM_KEYBOARDS; i++) {
        gamepad_state_clear(&g_state[i]);
    }
    g_numKbds = 0;

    // default configs
    uint16_t bd = UART1CalculateDivisor(9600);

    // init others
    initClock();
    initUART0(230400, 1);
    
    DEBUG_OUT("======== Startup ========\n");
    //resetHubDevices(0);
    //resetHubDevices(1);
    initUSB_Host();
    setAttachCallback(usbAttachCallback);
    ringbuf_init(&g_rb_out);

    // read config pins
    delay(50);
    uint8_t cfg_pin = P1 & 0xf0;
    DEBUG_OUT("cfg pin (P1): %02x\n", cfg_pin);

    static __xdata uint8_t out_fmt = OUTPUT_FORMAT_FULLLAYOUT;
    if (cfg_pin & (1 << 5)) out_fmt = 0;
    DEBUG_OUT("out_fmt: %02x\n", out_fmt);

    switch ((cfg_pin >> 6) & 3) {
        case 0: initUART1(115200); break;
        case 1: initUART1(76800); break;
        case 2: initUART1(57600); break;
        default: initUART1(9600); break;
    }

    // reconfig output ports
    if ((cfg_pin & (1 << 4)) == 0) {
        DEBUG_OUT("pullups disabled\n");
        P1_PU = 0x1f;
        P3_PU = 0x03;
    }
    delay(50);

    // config interrupts
    GPIO_IE = bIE_IO_EDGE;
    GPIO_IE |= bIE_P1_4_LO;
    IE_GPIO = 1;
    IE_UART1 = 1;
    EA = 1;
    init_watchdog(1);

    DEBUG_OUT("--- Ready ---\n");
    //DEBUG_OUT("P3 DIR=%02x PU=%02x P3=%02x\n", P3_DIR, P3_PU, P3);
    //DEBUG_OUT("P2 DIR=%02x PU=%02x P2=%02x\n", P2_DIR, P2_PU, P2);
    //DEBUG_OUT("PORT_CFG %02x\n", PORT_CFG);
    //ringbuf_write(&g_rb_out, '!');

    static __xdata uint32_t lasthubchecksecs;
    lasthubchecksecs = seconds();
    uint8_t lastsubtick = 0;
    uint8_t lastoutsubtick8 = 0;
    uint8_t st = 0;
    uint8_t need_out = 0;
    uint8_t targetKbdIndex = 0;
    static GamepadState padforled;

    static UDevInterface iface_xinput;
    iface_xinput.spec.hid.reports[0].type = JOYSTICK_INPUT_TYPE_CONST;
    iface_xinput.spec.hid.reports[0].size = 8;
    iface_xinput.spec.hid.reports[0].count = 2;
    iface_xinput.spec.hid.reports[1].type = JOYSTICK_INPUT_TYPE_DPAD;
    iface_xinput.spec.hid.reports[1].size = 4;
    iface_xinput.spec.hid.reports[1].count = 1;
    iface_xinput.spec.hid.reports[2].type = JOYSTICK_INPUT_TYPE_BUTTON;
    iface_xinput.spec.hid.reports[2].size = 1;
    iface_xinput.spec.hid.reports[2].count = 12;
    iface_xinput.spec.hid.reports[3].type = JOYSTICK_INPUT_TYPE_TRIGGER;
    iface_xinput.spec.hid.reports[3].size = 8;
    iface_xinput.spec.hid.reports[3].count = 2;
    iface_xinput.spec.hid.reports[4].type = JOYSTICK_INPUT_TYPE_AXIS_POSNEG_16BIT;
    iface_xinput.spec.hid.reports[4].size = 8;
    iface_xinput.spec.hid.reports[4].count = 4;
    iface_xinput.spec.hid.num_reports = 5;

    while (1) {
        do {
            clear_watchdog();
            if(!(P4_IN & (1 << 6))) {
                P3 = 0xff;
                init_watchdog(0);
                runBootloader();
            }
            if (UART1Available()) {
                uint8_t b = UART1Receive();
                if (b >= 0x20) {
                    uartcmd[uartcmd_pos] = b;
                    //DEBUG_OUT("UART in %02x\n", uartcmd[uartcmd_pos]);
                    if (uartcmd[uartcmd_pos] == ';') {
                        uartcmd[uartcmd_pos] = 0;
                        uartcmd_process(uartcmd);
                        uartcmd_pos = 0;
                    } else if (++uartcmd_pos >= sizeof(uartcmd)) {
                        uartcmd_pos = sizeof(uartcmd) - 1;
                    }
                }
            }
        } while (lastsubtick == g_subticks);
        lastsubtick = g_subticks;

        if (g_leddecr == 0) {
            p3_assert_inact();
        }

        if (seconds() != lasthubchecksecs) {
            clear_watchdog();
            checkRootHubConnections();
            checkHubConnections();
            lasthubchecksecs = seconds();
            if (g_poll_mode > 0) --g_poll_mode;
        }
        if (kbdconnected != (g_numKbds > 0)) {
            kbdconnected = (g_numKbds > 0);
            if (!kbdconnected) {
                p3_clear_detect();
            } else {
                p3_assert_detect();
            }
        }

        if (g_poll_req) {
            g_poll_mode = 11;
            need_out = 1;
            g_poll_req = 0;
        } else if (g_poll_mode == 0) {
            st = subticks8();
            if ((uint8_t)(st - lastoutsubtick8) > 15) {
                need_out = 1;
                lastoutsubtick8 = st;
            }
        }
        if (!need_out) continue;

        uint8_t ledout_done = 0;
        for (targetKbdIndex = 0; targetKbdIndex < MAX_NUM_KEYBOARDS; targetKbdIndex++) {
            if (!kbd_isconnectedat(targetKbdIndex)) continue;
            UDevInterface *iface;
            uint8_t devaddr = g_kbd_devAddr[targetKbdIndex];
            static __xdata uint8_t buf[32];
            static uint16_t pollsn = 0;
            uint8_t len = pollHIDDevice(g_kbd_devIndex[targetKbdIndex], g_kbd_interfNo[targetKbdIndex], buf, sizeof(buf), &iface);
            ++pollsn;
            //uint8_t len = pollHIDDevice(g_kbd_devIndex[targetKbdIndex], Usage_JOYSTICK, buf, sizeof(buf), &iface);
            if (len > 0) {
                /*DEBUG_OUT("pollHIDDev %04x @%d [%d] ", pollsn, targetKbdIndex, len);
            	for (uint8_t iii = 0; iii < len; iii++)
	            {
		            DEBUG_OUT("%02X ", buf[iii]);
	            }
                DEBUG_OUT("\n");*/

                gamepad_state_clear(&padforled);
                if (g_kbd_isXinput[targetKbdIndex]) {
                    uint8_t k, btnswap;
                    gamepad_parse_hid_data(&iface_xinput, buf, len, &padforled);
                    for (k = 0; k < 4; k++) {
                        btnswap = padforled.btns[8 + k];
                        padforled.btns[8 + k] = padforled.btns[k];
                        padforled.btns[k] = btnswap;
                    }
                } else {
                    gamepad_parse_hid_data(iface, buf, len, &padforled);
                }
                //static GamepadDPad dpad;
                //gamepad_get_unified_dpad(&padforled, &dpad);
                if (ledout_done == 0) {
                    uint8_t p3out = 0xfc;
                    uint8_t p1out = 0xe0;
                    uint8_t unidir = 0;
                    if (padforled.unified_dpad.dir.up) {
                        unidir ^= 0x01;
                    } else if (padforled.unified_dpad.dir.down) {
                        unidir ^= 0x02;
                    }
                    if (padforled.unified_dpad.dir.left) {
                        unidir ^= 0x04;
                    } else if (padforled.unified_dpad.dir.right) {
                        unidir ^= 0x08;
                    }
                    p3out ^= (unidir << 2);
                    if (padforled.btns[0] != 0) {
                        p3out ^= 0x40;
                    }
                    if (padforled.btns[1] != 0) {
                        p3out ^= 0x80;
                    }
                    P3 &= p3out; // | 3;
                    P3 |= p3out;
                    if (padforled.btns[2] != 0) {
                        p1out ^= 0x20;
                    }
                    if (padforled.btns[3] != 0) {
                        p1out ^= 0x40;
                    }
                    if (padforled.btns[4] != 0) {
                        p1out ^= 0x80;
                    }
                    P1 &= p1out | 0x1f;
                    P1 |= p1out;
                    ledout_done = 1;
                }
                //DEBUG_OUT("%02x %02x %02x; ", st, lastoutsubtick8, st - lastoutsubtick8);

                if (g_poll_mode || !gamepad_state_isequal(&g_state[targetKbdIndex], &padforled, out_fmt == 0)) {
                    output_gpstate(&padforled, devaddr, out_fmt);
                    gamepad_state_update(&g_state[targetKbdIndex], &padforled);
                }
                //DEBUG_OUT("unidir: %02x, xys: %02x,%02x  %02x,%02x\n", unidir, padforled.xys[0].x, padforled.xys[0].y, padforled.xys[1].x, padforled.xys[1].y);
            }
        }
        need_out = 0;
    }
}
