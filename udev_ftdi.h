#ifndef __UDEV_FTDI_H__
#define __UDEV_FTDI_H__

#include <stdint.h>

uint8_t ftdidevice_init_endpoint(USBDevice *dev, UDevInterface *iface, uint8_t endpoint);

int8_t ftdidevice_send(uint8_t devIndex, const __xdata uint8_t *src, uint8_t len);
int8_t ftdidevice_receive(uint8_t devIndex, __xdata uint8_t *dst, uint8_t maxlen);
int8_t ftdidevice_setBaudRate(uint8_t devIndex, uint32_t baud);
int8_t ftdidevice_setLatencyTimer(uint8_t devIndex, uint8_t timer);


#endif
