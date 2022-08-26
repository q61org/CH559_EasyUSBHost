#include "sdcc_keywords.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "CH559.h"
#include "util.h"
#include "USBHost.h"
#include "udev_util.h"
#include "udev_ftdi.h"


#define FTDI_SIO_SET_BAUD_RATE          3
#define FTDI_SIO_SET_LATENCY_TIMER      9
__code unsigned char ftdidev_setBaudRateRequest[] = { USB_REQ_TYP_OUT | USB_REQ_TYP_VENDOR, FTDI_SIO_SET_BAUD_RATE, 0x00, 0x00, 0x00 };
__code unsigned char ftdidev_setLatencyTimerRequest[] = { USB_REQ_TYP_OUT | USB_REQ_TYP_VENDOR, FTDI_SIO_SET_LATENCY_TIMER, 0x00 };


static uint8_t s_udev_vid_is_ftdi(USBDevice *d)
{
    return (d->vid_h == 4 && d->vid_l == 3);
}

uint8_t ftdidevice_init_endpoint(USBDevice *dev, UDevInterface *iface, uint8_t endpoint)
{
	if (!s_udev_vid_is_ftdi(dev)) {
		return 255;
	}
	return 0;
}

int8_t ftdidevice_send(uint8_t devIndex, const __xdata uint8_t *src, uint8_t len)
{
	USBDevice *d = &USBdevs[devIndex];
	if (!d->connected) return -1;
    if (!s_udev_vid_is_ftdi(d)) return -1;

	for (uint8_t ii = 0; ii < d->num_ifaces; ii++) {
		UDevInterface *iface = &d->iface[ii];
		if (iface->class != USB_DEV_CLASS_VEN_SPEC) continue;
        if (iface->ep_out == 0) continue;

		selectUSBDevice(devIndex);
        fillTxBuffer_xdata(src, len);
        UH_TX_LEN = len;

		uint8_t s = hostTransfer(USB_PID_OUT << 4 | (iface->ep_out & 0x7F), (iface->ep_out & 0x80) ? bUH_R_TOG | bUH_T_TOG : 0, 0);
		if (s == ERR_SUCCESS) {
			iface->ep_out ^= 0x80;
            return UH_TX_LEN;
		} else if (s == 42) {
            return 0;
        }
        return -1;
	}
	return -1;
}

int8_t ftdidevice_receive(uint8_t devIndex, __xdata uint8_t *dst, uint8_t maxlen)
{
	USBDevice *d = &USBdevs[devIndex];
	if (!d->connected) return -1;
    if (!s_udev_vid_is_ftdi(d)) return -1;

	for (uint8_t ii = 0; ii < d->num_ifaces; ii++) {
		UDevInterface *iface = &d->iface[ii];
		if (iface->class != USB_DEV_CLASS_VEN_SPEC) continue;
        if (iface->ep_in == 0) continue;

		selectUSBDevice(devIndex);
        uint8_t s = hostTransfer(USB_PID_IN << 4 | (iface->ep_in & 0x7f), (iface->ep_in & 0x80) ? bUH_R_TOG | bUH_T_TOG : 0, 0);
        if (s == ERR_SUCCESS) {
            iface->ep_in ^= 0x80;
    		uint8_t len = USB_RX_LEN;
		    if (len < 2) return 0;
      		len -= 2;
       		if (len > maxlen) len = maxlen;
       		for (uint8_t i = 0; i < len; i++) {
		        dst[i] = RxBuffer[2 + i];
	        }
            return len;
        } else if (s == 42) {
            return 0;
        }
        return -1;
	}
	return -1;
}

int8_t ftdidevice_setBaudRate(uint8_t devIndex, uint32_t baud)
{
	USBDevice *d = &USBdevs[devIndex];
	if (!d->connected) return -1;
    if (!s_udev_vid_is_ftdi(d)) return -1;

	uint16_t baud_value, baud_index = 0;
	uint32_t divisor3 = 48000000 / 2 / baud;
	static const uint8_t divfrac [8] = {0, 3, 2, 0, 1, 1, 2, 3};
    static const uint8_t divindex[8] = {0, 0, 0, 1, 0, 1, 1, 1};

    baud_value = divisor3 >> 3;
    baud_value |= divfrac [divisor3 & 0x7] << 14;
    baud_index = divindex[divisor3 & 0x7];

    /* Deal with special cases for highest baud rates. */
    if(baud_value == 1) baud_value = 0;
    else // 1.0
        if(baud_value == 0x4001) baud_value = 1; // 1.5
	
	//DEBUG_OUT("FTDISetBaudRate: baud=%ld value=%d index=%d\n", baud, baud_value, baud_index);

	unsigned short len;
	fillTxBuffer(ftdidev_setBaudRateRequest, sizeof(ftdidev_setBaudRateRequest));
	TxBuffer[2] = baud_value & 0x0ff;
	TxBuffer[3] = baud_value >> 8;
	TxBuffer[4] = baud_index & 0x0ff;
	
    selectUSBDevice(devIndex);
	unsigned char s = hostCtrlTransfer(receiveDataBuffer, &len, 0);
	return s;
}

int8_t ftdidevice_setLatencyTimer(uint8_t devIndex, uint8_t timer)
{
	USBDevice *d = &USBdevs[devIndex];
	if (!d->connected) return -1;
    if (!s_udev_vid_is_ftdi(d)) return -1;

	unsigned short len;
	fillTxBuffer(ftdidev_setLatencyTimerRequest, sizeof(ftdidev_setLatencyTimerRequest));
	TxBuffer[2] = timer;
	
	selectUSBDevice(devIndex);
	unsigned char s = hostCtrlTransfer(receiveDataBuffer, &len, 0);
	return s;
}
