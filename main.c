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
//#include "kbdparse.h"
//#include "keymap.h"
#include "ringbuf.h"
//#include "udev_hid.h"
#include "udev_hub.h"
#include "udev_ftdi.h"


#define VERSION_BYTE 0x01
#define VERSION_STR  "experimental"


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

INTERRUPT_USING(timer0_isr, INT_NO_TMR0, 1)
{ // 1024 counts per second
    TH0 = 0xf0;
    TL0 = 0xbe;
    g_subticks = ++g_subticks & 0x07;
    if (g_subticks == 0) {
        ++g_ticks;
        if (g_leddecr) --g_leddecr;
    }
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

static void s_ringbuf_write_hexchar(RingBuf *rb, uint8_t v)
{
    ringbuf_write(rb, bin2hexchar(v >> 4));
    ringbuf_write(rb, bin2hexchar(v));
}

#define MAX_NUM_KEYBOARDS 8
int8_t __xdata g_kbd_devIndex[MAX_NUM_KEYBOARDS];
uint8_t __xdata g_kbd_devAddr[MAX_NUM_KEYBOARDS];
//KbdState g_kbd[MAX_NUM_KEYBOARDS];
uint8_t __xdata g_numKbds;
RingBuf g_rb_out;
uint8_t __xdata g_raw_mode;

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
    if (is_attach) {
        for (uint8_t i = 0; i < dev->num_ifaces; i++) {
            // TODO: check if the device is ftdi?
            /*if (dev->iface[i].usage != Usage_KEYBOARD) {
                continue;
            }*/
            int8_t idx = findFreeKbdStateIndex();
            if (idx < 0) {
                DEBUG_OUT("CALLBACK: no more keyboard!\n");
                return;
            }

            if (ftdidevice_setBaudRate(devIndex, 19200) != 0) {
                DEBUG_OUT("CALLBACK: failed to set baud rate\n");
                return;
            }
            if (ftdidevice_setLatencyTimer(devIndex, 1) != 0) {
                DEBUG_OUT("CALLBACK: failed to set latency timer\n");
                return;
            }
            g_kbd_devIndex[idx] = devIndex;
            g_kbd_devAddr[idx] = dev->address;
            g_numKbds++;
            DEBUG_OUT("CALLBACK: %d keyboard(s) now connected\n", g_numKbds);
            ringbuf_write(&g_rb_out, 'A');
            s_ringbuf_write_hexchar(&g_rb_out, devIndex);
            s_ringbuf_write_hexchar(&g_rb_out, 1);
            ringbuf_write(&g_rb_out, ';');
            return;
        }
    } else {
        for (uint8_t i = 0; i < kbd_maxcount(); i++) {
            if (g_kbd_devIndex[i] == (int8_t)devIndex) {
                g_kbd_devIndex[i] = -1;
                --g_numKbds;
                DEBUG_OUT("CALLBACK: disconnect, %d keyboard(s) now connected\n", g_numKbds);
                ringbuf_write(&g_rb_out, 'A');
                s_ringbuf_write_hexchar(&g_rb_out, devIndex);
                s_ringbuf_write_hexchar(&g_rb_out, 0);
                ringbuf_write(&g_rb_out, ';');
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

#define UART_CMD_MAXLEN 32

void uartcmd_process(const __xdata char *cmd)
{
    static uint8_t __xdata s_ftdi_rcvbuf[64];
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
    uint8_t devIndex = 0;
    uint8_t i;
    int8_t r, n;
    if (cmd[0] == 'L') {
        if (len != 1) return;
        ringbuf_write(&g_rb_out, 'l');
        s_ringbuf_write_hexchar(&g_rb_out, g_numKbds);
        for (i = 0; i < g_numKbds; i++) {
            s_ringbuf_write_hexchar(&g_rb_out, g_kbd_devIndex[i]);
        }
        ringbuf_write(&g_rb_out, ';');
    } else if (cmd[0] == 'S') {
        devIndex = hexchar2bin(&cmd[2]);
        if (cmd[1] == 'B') {
            if (len != 9) return;
            uint32_t baud = 0;
            baud += 10000 * (cmd[4] - '0');
            baud +=  1000 * (cmd[5] - '0');
            baud +=   100 * (cmd[6] - '0');
            baud +=    10 * (cmd[7] - '0');
            baud +=     1 * (cmd[8] - '0');
            r = ftdidevice_setBaudRate(devIndex, baud);

            ringbuf_write(&g_rb_out, 's');
            ringbuf_write(&g_rb_out, 'b');
            s_ringbuf_write_hexchar(&g_rb_out, devIndex);
            s_ringbuf_write_hexchar(&g_rb_out, r);
            ringbuf_write(&g_rb_out, ';');
        }
    } else if (cmd[0] == 'R') {
        if (len != 3) return;
        devIndex = hexchar2bin(&cmd[1]);
        r = ftdidevice_receive(devIndex, s_ftdi_rcvbuf, sizeof(s_ftdi_rcvbuf));
        ringbuf_write(&g_rb_out, 'r');
        s_ringbuf_write_hexchar(&g_rb_out, devIndex);
        s_ringbuf_write_hexchar(&g_rb_out, r);
        if ((r > 0) && (r <= sizeof(s_ftdi_rcvbuf))) {
            for (n = 0; n < r; n++) {
                s_ringbuf_write_hexchar(&g_rb_out, s_ftdi_rcvbuf[n]);
            }
        }
        ringbuf_write(&g_rb_out, ';');
    } else if (cmd[0] == 'T') {
        if (len < 3) return;
        devIndex = hexchar2bin(&cmd[1]);
        uint8_t dstpos = 0;
        for (i = 3; i < len; i += 2) {
            s_ftdi_rcvbuf[dstpos++] = hexchar2bin(&cmd[i]);
        }
        r = ftdidevice_send(devIndex, s_ftdi_rcvbuf, dstpos);
        ringbuf_write(&g_rb_out, 't');
        s_ringbuf_write_hexchar(&g_rb_out, devIndex);
        s_ringbuf_write_hexchar(&g_rb_out, r);
        ringbuf_write(&g_rb_out, ';');
    }
}

static void passthrough_mode()
{
    P0_PU = 0xff;
    P0_DIR = 0x80;
    P1_PU = 0x7f;
    P2_PU = 0x7f;
    P2_DIR = 0x80;
    P3_PU = 0xfd;
    P3_DIR = 0x02;
    P4_PU = 0xff;
    PORT_CFG &= 0xf3; // set P2 and P3 to push-pull mode

    P0 &= 0x7f;
    delay(100);
    P0 |= 0x80;

    uint8_t rts_cur = 1;
    uint8_t rts_last = 1;
    uint8_t rts_decr = 0;
    uint8_t ticks_last = ticks() & 0x0ff;

    while (1) {
        uint8_t p2p = P2;
        uint8_t p3p = P3;
        P2 = (p3p & 1) ? 0x80 : 0x00;
        P3 = (P3 & 0xfd) | ((p2p & 0x40) ? 0x02 : 0x00);

        uint8_t ticks_cur = ticks() & 0x0ff;
        if (ticks_last != ticks_cur) {
            ticks_last = ticks_cur;
            rts_cur = (p3p & 0x04) >> 2;
            if ((rts_cur == 0) && (rts_last == 1)) {
                P0 = 0x00;
                rts_decr = 32;
            }
            rts_last = rts_cur;
            if (rts_decr > 0) {
                if (--rts_decr == 0) {
                    P0 = 0x80;
                }
            }
        }

        if(!(P4_IN & (1 << 6))) {
            runBootloader();
        }
    }
}

void main()
{
    if(!(P4_IN & (1 << 6))) {
        runBootloader();
    }
    initClock();
    ticks_init();
    ET0 = 1;
    P1_PU &= 0x7f;
    EA = 1;

    while (seconds() < 2) {
        if(!(P4_IN & (1 << 6))) {
            runBootloader();
        }
    }
    if (P1 & 0x80) {
        passthrough_mode();
    }

    EA = 0;
    
    static __xdata uint32_t t_last = 255;
    //uint8_t s;
    static __xdata uint8_t uart_buf_pos = 0;
    static __xdata uint8_t kbdconnected = 0;
    static __xdata uint8_t tgl = 0;
    static __xdata uint8_t cfg_pin;
    static __xdata uint8_t uartcmd[UART_CMD_MAXLEN];
    static __xdata uint8_t uartcmd_pos = 0;

    P0_PU = 0xff;
    P0_DIR = 0x80;
    P1_PU = 0x7f;
    P2_PU = 0x7f;
    P2_DIR = 0x80;
    P3_PU = 0xfd;
    P3_DIR = 0x02;
    P4_PU = 0xff;

    PORT_CFG &= 0xf3; // set P2 and P3 to push-pull mode
    
    for (uint8_t i = 0; i < sizeof(g_kbd_devIndex); i++) {
        g_kbd_devIndex[i] = -1;
    }
    g_numKbds = 0;

    // default configs
    // init others
    initClock();
    initUART0(230400, 0);
    initUART1(57600);
    DEBUG_OUT("======== Startup ========\n");
    //resetHubDevices(0);
    //resetHubDevices(1);
    initUSB_Host();
    setAttachCallback(usbAttachCallback);
    ringbuf_init(&g_rb_out);

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

    while (1) {
        do {
            clear_watchdog();
            if(!(P4_IN & (1 << 6))) {
                P3 = 0xff;
                init_watchdog(0);
                runBootloader();
            }
            if (ringbuf_available(&g_rb_out) && UART1TxIsEmpty()) {
                g_leddecr = 6;
                uint8_t d = ringbuf_pop(&g_rb_out);
                //DEBUG_OUT("sending %02x via UART1\n", d);
                UART1SendAsync(d);
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

        if (seconds() != lasthubchecksecs) {
            clear_watchdog();
            checkRootHubConnections();
            checkHubConnections();
            lasthubchecksecs = seconds();
        }

    }
}
