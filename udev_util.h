#ifndef __UDEV_UTIL_H__
#define __UDEV_UTIL_H__

void DEBUG_OUT_DEVICE_STRING(uint8_t index);
void DEBUG_OUT_USB_DEV_CLASS(uint8_t class);
void DEBUG_OUT_USB_REPORT_USAGE(uint8_t data);
void readInterface(unsigned char rootHubIndex, PXUSB_ITF_DESCR interface);
void readHIDInterface(PXUSB_ITF_DESCR interface, PXUSB_HID_DESCR descriptor);
void DEBUG_DUMP_USB_DEVICE(USBDevice *dev, uint8_t depth);
void DEBUG_DUMP_USB_TREE();

#endif
