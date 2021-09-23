#ifndef __UDEV_HUB_H__
#define __UDEV_HUB_H__

uint8_t checkHubConnections();
uint8_t hubdevice_init_endpoint(USBDevice *dev, UDevInterface *iface, uint8_t endpoint);


#endif
