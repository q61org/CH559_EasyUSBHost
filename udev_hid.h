#ifndef __UDEV_HID_H__
#define __UDEV_HID_H__

uint8_t IsKeyboardConnected();
uint8_t pollHIDDevice(uint8_t devIndex, uint8_t usage, __xdata uint8_t *dst, uint8_t maxlen);
uint8_t setHIDDeviceLED(uint8_t devIndex, uint8_t usage, uint8_t led);
uint8_t hiddevice_init_endpoint(USBDevice *dev, UDevInterface *iface, uint8_t endpoint);
uint8_t hiddevice_start_input(uint8_t devIndex, uint8_t usage);

#endif
