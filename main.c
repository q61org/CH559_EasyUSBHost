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
#include "kbdparse.h"
#include "keymap.h"
#include "ringbuf.h"
#include "udev_hid.h"
#include "udev_hub.h"


#define VERSION_BYTE 0x03
#define VERSION_STR  "v20210626 q61.org k.k."


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

// ==== SPI ====

__xdata volatile uint8_t g_spiphase;

#define SPI_REG_SIZE 8
#define SPI_REG_SIZE_MASK 7
uint8_t __xdata g_spireg[SPI_REG_SIZE];
uint8_t __xdata g_spiaddr;
uint8_t __xdata g_spidata;
uint8_t __xdata g_spisent;

INTERRUPT_USING(spi0_isr, INT_NO_SPI0, 2)
{
    if (SPI0_SETUP & bS0_SLV_SELT) {
        if (g_spiphase == 1) {
            g_spiaddr = SPI0_DATA;
            SPI0_DATA = g_spisent = g_spireg[g_spiaddr & SPI_REG_SIZE_MASK];
            g_spiphase++;
        } else if (g_spiphase == 2) {
            g_spidata = SPI0_DATA;
            g_spiphase++;
        }

    }
    S0_IF_BYTE = 0;
}

void spi_writereg(uint8_t addr, uint8_t value)
{
    addr &= SPI_REG_SIZE_MASK;
    if (addr == 0) return;
    g_spireg[addr] = value;
}

uint8_t spi_updatecfg(uint8_t addr)
{ // returns (lock_bitmap | 0x80) if update is required
    uint8_t r = 0;
    switch (addr) {
        case 1: case 2: // uart divisor
            initUART1withDivisor(((uint16_t)g_spireg[2] << 8) | g_spireg[1]);
            break;
        case 4: // config
            keymap_setmap((g_spireg[addr] & 0x80) ? KEYMAP_US : KEYMAP_JP);
            break;
        case 5: // status
            r = (0x80 | (g_spireg[addr] & 0x07));
            break;
    }
    return r;
}

void spi_update_statusreg(uint8_t connected, uint8_t locks)
{
    g_spireg[5] = (connected ? 0x80 : 0x00) | (locks & 0x07);
}

#define cfg_repeat_delay()  (g_spireg[3] & 0xf0)
#define cfg_repeat_intvl()  ((g_spireg[3] & 0x0f) << 2)
#define cfg_dis_numlk()     (g_spireg[4] & 0x01)
#define cfg_dis_capslk()    (g_spireg[4] & 0x02)
#define cfg_dis_scrlk()     (g_spireg[4] & 0x04)
#define cfg_separatelock()  (g_spireg[4] & 0x10)
#define cfg_ctrlsequence()  (g_spireg[4] & 0x10)
#define cfg_swapcaps()      (g_spireg[4] & 0x20)
#define cfg_crlf()          (g_spireg[4] & 0x40)
#define cfg_keymap_us()     (g_spireg[4] & 0x80)
#define cfg_locks()         (g_spireg[5] & 0x07)
#define cfg_intr_modifier() (g_spireg[6])
#define cfg_intr_keynum()   (g_spireg[7])

// ================

#define p3_assert_interrupt()  (P3 &= 0xfb)
#define p3_clear_interrupt()   (P3 |= 4)
#define p3_assert_inact()      (P3 &= 0xfe)
#define p3_clear_inact()       (P3 |= 1)
#define p3_assert_detect()     (P3 &= 0xfd)
#define p3_clear_detect()      (P3 |= 2)

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
KbdState g_kbd[MAX_NUM_KEYBOARDS];
uint8_t __xdata g_numKbds;
RingBuf g_rb_out;
uint8_t __xdata g_raw_mode;
uint8_t __xdata g_default_locks;

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
        for (uint8_t i = 0; i < dev->num_ifaces; i++) {
            if (dev->iface[i].usage != Usage_KEYBOARD) {
                continue;
            }
            int8_t idx = findFreeKbdStateIndex();
            if (idx < 0) {
                DEBUG_OUT("CALLBACK: no more keyboard!\n");
                return;
            }
            g_kbd_devIndex[idx] = devIndex;
            g_kbd_devAddr[idx] = dev->address;
            kbdparse_init(&g_kbd[idx]);
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
            setHIDDeviceLED(devIndex, Usage_KEYBOARD, locks);
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
    } else {
        for (uint8_t i = 0; i < kbd_maxcount(); i++) {
            if (g_kbd_devIndex[i] == (int8_t)devIndex) {
                g_kbd_devIndex[i] = -1;
                --g_numKbds;
                DEBUG_OUT("CALLBACK: disconnect, %d keyboard(s) now connected\n", g_numKbds);
                if (g_raw_mode) {
                    ringbuf_write(&g_rb_out, bin2hexchar(dev->address >> 4));
                    ringbuf_write(&g_rb_out, bin2hexchar(dev->address));
                    ringbuf_write(&g_rb_out, 'a');
                    ringbuf_write(&g_rb_out, '0');
                    ringbuf_write(&g_rb_out, '0');
                    ringbuf_write(&g_rb_out, ';');
                }
            }
        }
    }
}

void kbd_updatelocks(uint8_t newlocks, uint8_t specific_addr)
{
    for (uint8_t i = 0; i < kbd_maxcount(); i++) {
        if (!kbd_isconnectedat(i)) continue;
        if (specific_addr && g_kbd_devAddr[i] != specific_addr) continue;
        KbdState *kbd = &g_kbd[i];
        kbd->locks = newlocks;
        setHIDDeviceLED(g_kbd_devIndex[i], Usage_KEYBOARD, newlocks);
    }
}

// ================

void cfg_writereg_and_update(uint8_t addr, uint8_t value)
{
    addr &= 0x7f;
    if (addr >= SPI_REG_SIZE) return;
    spi_writereg(addr, value);
    uint8_t r = spi_updatecfg(addr);
    if (r & 0x80) {
        if (g_numKbds > 0 && !(g_raw_mode && cfg_separatelock())) {
            kbd_updatelocks(cfg_locks(), 0);
        } else {
            g_default_locks = cfg_locks();
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
    static __xdata uint8_t cfg_pin;
    static __xdata uint8_t uartcmd[UART_CMD_MAXLEN];
    static __xdata uint8_t uartcmd_pos = 0;

    P0_PU = 0xf7;
    P1_PU = 0xff;
    P2_PU = 0x3f;
    P2_DIR = 0x80;
    P3_PU = 0xfb;
    P3_DIR = 0x00;
    P3 = 0xff;
    P4_PU = 0xff;

    PORT_CFG &= 0xfb; // set P2 to push-pull mode
    
    for (uint8_t i = 0; i < sizeof(g_kbd_devIndex); i++) {
        g_kbd_devIndex[i] = -1;
    }
    g_numKbds = 0;

    // default configs
    g_default_locks = LOCK_NUM;
    uint16_t bd = UART1CalculateDivisor(9600);
    g_spireg[0] = VERSION_BYTE;
    g_spireg[1] = bd & 0x0ff;
    g_spireg[2] = bd >> 8;
    g_spireg[3] = 0x40 | 0x04;
    g_spireg[4] = 0;
    spi_update_statusreg(0, g_default_locks); // reg 5
    g_spireg[6] = 5;
    g_spireg[7] = 0x4c;

    // init others
    initClock();
    initUART0(230400, 1);
    initUART1(9600);
    DEBUG_OUT("======== Startup ========\n");
    //resetHubDevices(0);
    //resetHubDevices(1);
    initUSB_Host();
    setAttachCallback(usbAttachCallback);
    ringbuf_init(&g_rb_out);

    g_spiphase = 0;
    cfg_pin = P3;
    g_raw_mode = (P3 & (1 << 3)) == 0;
    if ((cfg_pin & (1 << 4)) == 0) {
        // software config (spi) mode
        DEBUG_OUT("Setting up SPI... ");
        PORT_CFG &= 0xfd;
        P1_PU &= 0x0f;
        P1_DIR &= 0x0f;
        SPI0_SETUP = 0x90;
        SPI0_CTRL = 0x81;
        SPI0_STAT = 0xff;
        g_spidata = 0;
        g_spiphase = 1;
        IE_SPI0 = 1;
        DEBUG_OUT("done.\n");
    } else {
        // hardware config mode
        uint8_t p1_in = P1;
        if ((cfg_pin & (1 << 5)) == 0) g_spireg[4] |= 0x80; // us keymap
        if ((cfg_pin & (1 << 6)) == 0) g_spireg[4] |= 0x10; // multibyte control sequence / separate locks
        if ((cfg_pin & (1 << 7)) == 0) g_spireg[3] = 0; // no repeat
        if ((p1_in & (1 << 4)) == 0) g_spireg[4] |= 0x07; // disable lock keys
        switch ((p1_in >> 5) ^ 0x07) {
            case 0: break;
            case 1: initUART1(14400); break;
            case 2: initUART1(19200); break;
            case 3: initUART1(28800); break;
            case 4: initUART1(38400); break;
            case 5: initUART1(57600); break;
            case 6: initUART1(76800); break;
            case 7: initUART1(115200); break;
        }
    }
    keymap_setmap((g_spireg[4] & 0x80) ? KEYMAP_US : KEYMAP_JP);
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
    uint8_t targetKbdIndex = 0;

    while (1) {
        do {
            clear_watchdog();
            if(!(P4_IN & (1 << 6))) {
                P3 = 0xff;
                init_watchdog(0);
                runBootloader();
            }
            if (ringbuf_available(&g_rb_out) && UART1TxIsEmpty()) {
                p3_clear_inact();
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
            switch (g_spiphase) {
                case 0: // spi is disabled, nop
                    break;
                case 1: // spi idle
                    break;
                case 2: // spi address received
                    break;
                case 3: // spi data received
                    DEBUG_OUT("SPI rcvd, @%02x/%02x, sent %02x\n", g_spiaddr, g_spidata, g_spisent);
                    g_spiphase++;
                    if (g_spiaddr & 0x80) {
                        cfg_writereg_and_update(g_spiaddr, g_spidata);
                    }
                    break;
                default:
                    if ((SPI0_SETUP & bS0_SLV_SELT) == 0) {
                        DEBUG_OUT("SPI reset.\n");
                        g_spiphase = 1;
                    }
                    break;
            }
        } while (lastsubtick == g_subticks);
        lastsubtick = g_subticks;

        if (seconds() != lasthubchecksecs) {
            clear_watchdog();
            checkRootHubConnections();
            checkHubConnections();
            lasthubchecksecs = seconds();
        }
        if (kbdconnected != (g_numKbds > 0)) {
            kbdconnected = (g_numKbds > 0);
            if (!kbdconnected) {
                p3_clear_detect();
            } else {
                p3_assert_detect();
            }
            uint8_t lk = g_default_locks;
            if (kbdconnected) for (uint8_t i = 0; i < kbd_maxcount(); i++) {
                if (!kbd_isconnectedat(i)) continue;
                lk = g_kbd[i].locks;
            }
            spi_update_statusreg(kbdconnected, lk);
        }

        if (kbd_isconnectedat(targetKbdIndex)) {
            KbdState *kbd = &g_kbd[targetKbdIndex];
            uint8_t devaddr = g_kbd_devAddr[targetKbdIndex];
            static __xdata uint8_t buf[8];
            static KeyEvent evts[16];
            uint8_t nevts = 0;
            uint8_t len = pollHIDDevice(g_kbd_devIndex[targetKbdIndex], Usage_KEYBOARD, buf, sizeof(buf));
            if (len > 0) {
                if (cfg_swapcaps()) kbdparse_hid_swapcaps(buf, len);
                nevts = kbdparse_hidinput(kbd, ticks(), buf, len, evts, 16);
            }
            if (nevts == 0) {
                nevts = kbdparse_getrepeat(kbd, ticks(), cfg_repeat_delay(), cfg_repeat_intvl(), &evts[0]);
            }
            if (nevts > 0) {
                DEBUG_OUT("@%d KeyEvt%3d:", devaddr, nevts);
                for (uint8_t i = 0; i < nevts; i++) {
                    KeyEvent *e = &evts[i];
                    DEBUG_OUT(" %c%02x", (e->evttype == KEYEVENT_REPEAT) ? 'R' : ((e->evttype == KEYEVENT_DOWN) ? 'D' : 'U'), e->keynum);
                    //DEBUG_OUT("** KEY EVT: %s %d %02x\n", (e->evttype == KEYEVENT_REPEAT) ? "REPEAT" : ((e->evttype == KEYEVENT_DOWN) ? "DOWN" : "UP"), e->is_modifier, e->keynum);
                    if (e->evttype == KEYEVENT_DOWN) {
                        if (cfg_intr_keynum() != 0 && e->keynum == cfg_intr_keynum() && e->modifier == cfg_intr_modifier()) {
                            DEBUG_OUT("<INTERRUPT>");
                            p3_assert_interrupt();
                        }
                        uint8_t doUpdateLed = 0;
                        switch (e->keynum) {
                            case 0x53: // numlock
                                if (cfg_dis_numlk()) break;
                                kbd->locks ^= LOCK_NUM;
                                doUpdateLed = 1;
                                break;
                            case 0x39: // caps
                                if (cfg_dis_capslk()) break;
                                kbd->locks ^= LOCK_CAPS;
                                doUpdateLed = 1;
                                break;
                            case 0x47: // scroll
                                if (cfg_dis_scrlk()) break;
                                kbd->locks ^= LOCK_SCROLL;
                                doUpdateLed = 1;
                                break;
                        }
                        if (doUpdateLed) {
                            if (g_raw_mode && cfg_separatelock()) {
                                setHIDDeviceLED(g_kbd_devIndex[targetKbdIndex], Usage_KEYBOARD, kbd->locks);
                            } else {
                                kbd_updatelocks(kbd->locks, 0);
                                spi_update_statusreg(kbdconnected, kbd->locks);
                            }
                            if (g_raw_mode) {
                                ringbuf_write(&g_rb_out, bin2hexchar(devaddr >> 4));
                                ringbuf_write(&g_rb_out, bin2hexchar(devaddr));
                                ringbuf_write(&g_rb_out, 'L');
                                ringbuf_write(&g_rb_out, '0');
                                ringbuf_write(&g_rb_out, bin2hexchar(kbd->locks));
                                ringbuf_write(&g_rb_out, ';');
                            }
                        }
                    } else if (e->evttype == KEYEVENT_UP && cfg_intr_keynum() != 0) {
                        if (e->keynum == cfg_intr_keynum() || e->modifier != cfg_intr_modifier()) {
                            p3_clear_interrupt();
                        }
                    }

                    static __xdata char charbuf[8];
                    uint8_t cnt = 0;
                    if ((e->evttype == KEYEVENT_DOWN || e->evttype == KEYEVENT_REPEAT) && (e->is_modifier == 0)) {
                        cnt = keymap_keynum_to_char(e->keynum, e->modifier, kbd->locks, charbuf, 8);
                    }

                    if (g_raw_mode) {
                        ringbuf_write(&g_rb_out, bin2hexchar(devaddr >> 4));
                        ringbuf_write(&g_rb_out, bin2hexchar(devaddr));
                        char d = '?';
                        switch (e->evttype) {
                            case KEYEVENT_DOWN: d = (e->is_modifier) ? 'M' : 'K'; break;
                            case KEYEVENT_UP:   d = (e->is_modifier) ? 'm' : 'k'; break;
                            case KEYEVENT_REPEAT: d = 'R'; break;
                        }
                        ringbuf_write(&g_rb_out, d);
                        ringbuf_write(&g_rb_out, bin2hexchar(e->keynum >> 4));
                        ringbuf_write(&g_rb_out, bin2hexchar(e->keynum));
                        if (cnt == 1) {
                            ringbuf_write(&g_rb_out, bin2hexchar(charbuf[0] >> 4));
                            ringbuf_write(&g_rb_out, bin2hexchar(charbuf[0]));
                        }
                        ringbuf_write(&g_rb_out, ';');

                    } else if (cnt == 1 || (cnt > 1 && cfg_ctrlsequence())) {
                        if (cfg_crlf() && cnt == 1 && charbuf[0] == 0x0a) {
                            ringbuf_write(&g_rb_out, 0x0d);
                        }
                        for (uint8_t i = 0; i < cnt; i++) {
                            ringbuf_write(&g_rb_out, charbuf[i]);
                        }
                    }
                }
                DEBUG_OUT("\n");
            }
        }
        for (uint8_t i = 0; i < MAX_NUM_KEYBOARDS; i++) {
            if (++targetKbdIndex >= kbd_maxcount()) {
                targetKbdIndex = 0;
            }
            if (kbd_isconnectedat(targetKbdIndex)) break;
        }
        if (g_leddecr == 0) {
            p3_assert_inact();
        }
    }
}
