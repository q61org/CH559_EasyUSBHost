#include "sdcc_keywords.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "CH559.h"
#include "util.h"
#include "USBHost.h"
#include "udev_util.h"

__code unsigned char SetupGetHubDescr[] = { HUB_GET_HUB_DESCRIPTOR, HUB_GET_DESCRIPTOR, 0x00, USB_DESCR_TYP_HUB, 0x00, 0x00, sizeof( USB_HUB_DESCR ), 0x00 };

inline uint8_t hubdevice_initUSBus(uint8_t devIndex)
{
	selectUSBDevice(devIndex);
	return 0;
}

inline UDevInterface *hubdevice_findInterface(uint8_t devIndex)
{
	for (uint8_t ii = 0; ii < USBdevs[devIndex].num_ifaces; ii++) {
		if (USBdevs[devIndex].iface[ii].class == USB_DEV_CLASS_HUB) {
			return &USBdevs[devIndex].iface[ii];
		}
	}
	return NULL;
}

uint8_t hubdevice_getDescriptor(uint8_t devIndex)
{
	DEBUG_OUT("hub get descriptor %d\n", devIndex);
	UDevInterface *iface = hubdevice_findInterface(devIndex);
	if (iface == NULL) {
		DEBUG_OUT("no interface\n");
		return ERR_USB_UNSUPPORT;
	}
	unsigned short len;
	fillTxBuffer(SetupGetHubDescr, sizeof(SetupGetHubDescr));
	((PXUSB_SETUP_REQ)TxBuffer)->wIndexL = iface->interface;	
	uint8_t s = hostCtrlTransfer(receiveDataBuffer, &len, 0);
	if (s != ERR_SUCCESS) {
		DEBUG_OUT("get descriptor failed: %d\n", s);
		return s;
	}
	if (len < ((PUSB_SETUP_REQ)SetupGetHubDescr)->wLengthL) {
		return ERR_USB_BUF_OVER;
	}
	//__xdata USB_HUB_DESCR *d = (__xdata USB_HUB_DESCR*)receiveDataBuffer;
	DEBUG_OUT("Hub descriptor data:")
	for (unsigned short i = 0; i < len; i++) {
		DEBUG_OUT(" %02x", RxBuffer[i]);
	}
	DEBUG_OUT("\n");
	iface->spec.hub.num_ports = ((PXUSB_HUB_DESCR)receiveDataBuffer)->bNbrPorts;
	return 0;
}

uint8_t hubdevice_getPortStatus(uint8_t devIndex, uint8_t portIndex)
{
	hubdevice_initUSBus(devIndex);
	PXUSB_SETUP_REQ pSetup = (PXUSB_SETUP_REQ)TxBuffer;
	pSetup->bRequestType = HUB_GET_PORT_STATUS;
	pSetup->bRequest = HUB_GET_STATUS;
	pSetup->wValueL = 0;
	pSetup->wValueH = 0;
	pSetup->wIndexL = portIndex;
	pSetup->wIndexH = 0;
	pSetup->wLengthL = 4;
	pSetup->wLengthH = 0;
	unsigned short len;
	uint8_t s = hostCtrlTransfer(receiveDataBuffer, &len, 0);
	if (s != ERR_SUCCESS) {
		//DEBUG_OUT("getPortStatus hostCtrlTransfer failed: %d\n", s);
		return s;
	}
	/*DEBUG_OUT("Hub %d port %d status:", devIndex, portIndex);
	for (unsigned short i = 0; i < len; i++) {
		DEBUG_OUT(" %02x", receiveDataBuffer[i]);
	}
	DEBUG_OUT("\n");*/
	return 0;
}

uint8_t hubdevice_setPortFeature(uint8_t devIndex, uint8_t portIndex, uint8_t feature)
{
	hubdevice_initUSBus(devIndex);
	PXUSB_SETUP_REQ pSetup = (PXUSB_SETUP_REQ)TxBuffer;
	pSetup->bRequestType = HUB_SET_PORT_FEATURE;
	pSetup->bRequest = HUB_SET_FEATURE;
	pSetup->wValueL = feature;
	pSetup->wValueH = 0;
	pSetup->wIndexL = portIndex;
	pSetup->wIndexH = 0;
	pSetup->wLengthL = 0;
	pSetup->wLengthH = 0;
	unsigned short len;
	uint8_t s = hostCtrlTransfer(receiveDataBuffer, &len, 0);
	if (s != ERR_SUCCESS) {
		//DEBUG_OUT("hub_setPortFeature hostCtrlTransfer failed: %d\n", s);
	}
	return s;
}

uint8_t hubdevice_clearPortFeature(uint8_t devIndex, uint8_t portIndex, uint8_t feature)
{
	hubdevice_initUSBus(devIndex);
	PXUSB_SETUP_REQ pSetup = (PXUSB_SETUP_REQ)TxBuffer;
	pSetup->bRequestType = HUB_CLEAR_PORT_FEATURE;
	pSetup->bRequest = HUB_CLEAR_FEATURE;
	pSetup->wValueL = feature;
	pSetup->wValueH = 0;
	pSetup->wIndexL = portIndex;
	pSetup->wIndexH = 0;
	pSetup->wLengthL = 0;
	pSetup->wLengthH = 0;
	unsigned short len;
	uint8_t s = hostCtrlTransfer(receiveDataBuffer, &len, 0);
	if (s != ERR_SUCCESS) {
		//DEBUG_OUT("hub_clearPortFeature hostCtrlTransfer failed: %d\n", s);
	}
	return s;
}

inline uint8_t findDeviceIndexAtHubPort(uint8_t hubIndex, uint8_t portIndex)
{
	for (uint8_t i = 0; i < MAX_USB_DEVICES; i++) {
		USBDevice *dev = &USBdevs[i];
		if ((dev->parentDevIndex == hubIndex) && (dev->parentDevPortIndex == portIndex)) {
			return i;
		}
	}
	return 0xff;
}

void disconnectDeviceAtIndex(uint8_t devIndex)
{
    if (devIndex == 0xff) return;
	uint8_t dev_stack[USB_HUB_MAX_DEPTH];
	uint8_t ii_stack[USB_HUB_MAX_DEPTH];
	uint8_t pi_stack[USB_HUB_MAX_DEPTH];
	uint8_t depth = 0;

	uint8_t ii = 0, pi = 1;
	USBDevice * dev = NULL;
rescan: // using loops instead of recursion to save on stack
	dev = &USBdevs[devIndex];
	for (; ii < dev->num_ifaces; ii++) {
		UDevInterface *iface = &dev->iface[ii];
		if (iface->class != USB_DEV_CLASS_HUB) continue;

		for (; pi <= iface->spec.hub.num_ports; pi++) {
			uint8_t childIndex = findDeviceIndexAtHubPort(devIndex, pi);
			if (childIndex == 0xff) continue;

			dev_stack[depth] = devIndex;
			ii_stack[depth] = ii;
			pi_stack[depth] = pi + 1;
			devIndex = childIndex;
			ii = 0;
			pi = 1;
			depth++;
			goto rescan;
		}
	}
	DEBUG_OUT("Device @%d (hub @%d port %d) disconnected.\n", dev->address, dev->parentDevIndex + FIRST_USB_DEV_ID, dev->parentDevPortIndex);
	callAttachCallback(devIndex, dev, 0);
	dev->connected = 0;
	if (depth > 0) {
		--depth;
		devIndex = dev_stack[depth];
		ii = ii_stack[depth];
		pi = pi_stack[depth];
		goto rescan;
	}
}

uint8_t checkHubConnections()
{
	for (int8_t di = MAX_USB_DEVICES - 1; di >= 0; di--) {
		if (!USBdevs[di].connected) continue;
		UDevInterface *iface = hubdevice_findInterface(di);
		if (iface == NULL) continue;
		if (iface->spec.hub.num_ports == 0) continue;
        uint8_t hub_addr = di + FIRST_USB_DEV_ID;

		selectUSBDevice(di);
		uint8_t s = hostTransfer(USB_PID_IN << 4 | iface->ep_in, (iface->ep_in & 0x80) ? bUH_R_TOG | bUH_T_TOG : 0, 0);
		if (s == ERR_SUCCESS) {
			iface->ep_in ^= 0x80;
			if (USB_RX_LEN > 0) {
				uint8_t len = USB_RX_LEN;
				DEBUG_OUT("Hub @%d, data in [%d]:", hub_addr, len);
				for (uint8_t k = 0; k < len; k++) {
					DEBUG_OUT(" %02x", RxBuffer[k]);
				}
				DEBUG_OUT("\n");
				iface->spec.hub.port_flags |= RxBuffer[0];
			}
		} else if (s != 42) {
			DEBUG_OUT("Hub @%d error %d\n", hub_addr, s);
		}

		for (uint8_t pi = 1; pi <= iface->spec.hub.num_ports; pi++) {
			clear_watchdog();
			if ((iface->spec.hub.port_flags & (1 << pi)) == 0) {
				continue;
			}
			DEBUG_OUT("Hub @%d: port %d status change flagged, checking.\n", hub_addr, pi);
			s = hubdevice_clearPortFeature(di, pi, HUB_C_PORT_CONNECTION);
			s = hubdevice_getPortStatus(di, pi);
			if (s != ERR_SUCCESS) {
				break;
			}
            uint8_t need_reset = 0;
			if ((RxBuffer[0] & 1) == ((iface->spec.hub.port_connected >> pi) & 1)) {
				DEBUG_OUT("Hub @%d: port %d no connection change.\n", hub_addr, pi);
                if (!(RxBuffer[0] & 2)) {
                    DEBUG_OUT("Hub @%d: port %d is disabled, disconnecting. (will reset later.)\n", hub_addr, pi);
                    s = hubdevice_clearPortFeature(di, pi, HUB_C_PORT_ENABLE);
                    disconnectDeviceAtIndex(findDeviceIndexAtHubPort(di, pi));
                    iface->spec.hub.port_connected &= ((1 << pi) ^ 0xff);
                    need_reset = 1;
                } else {
				//DEBUG_OUT("  port status: %02x %02x\n", RxBuffer[0], RxBuffer[1]);
                    continue;
                }
			}

			if (need_reset || (RxBuffer[0] & 1)) {
				DEBUG_OUT("Hub @%d: port %d connected. Resetting.\n", hub_addr, pi);
				s = hubdevice_setPortFeature(di, pi, HUB_PORT_RESET);
				if (s != ERR_SUCCESS) {
					//DEBUG_OUT("Hub %d: port %d reset failed: %d.\n", i, pi, s);
					continue;
				}
				do {
					delay(1);
					s = hubdevice_getPortStatus(di, pi);
					if (s != ERR_SUCCESS) {
						//DEBUG_OUT("Hub %d: port %d: get port status failed: %d.\n", i, pi, s);
						break;
					}
				} while (RxBuffer[0] & (1 << (HUB_PORT_RESET & 7)));
				//DEBUG_OUT("  port status: %02x %02x\n", RxBuffer[0], RxBuffer[1]);
				if (s != ERR_SUCCESS) {
					continue;
				}
				if ((RxBuffer[0] & 1) == 0) {
					continue; // TODO: detach
				}
				uint8_t speed = (RxBuffer[1] & (1 << (HUB_PORT_LOW_SPEED & 0x07))) ? 0 : 1;
				s = hubdevice_clearPortFeature(di, pi, HUB_C_PORT_RESET);
				if (s != ERR_SUCCESS) {
					continue;
				}
				initializeRootHubConnection(USBdevs[di].rootHubIndex, di, pi, speed);
				iface->spec.hub.port_connected |= (1 << pi);
			} else {
				//DEBUG_OUT("Hub %d: port %d disconnected.\n", i, pi);
				uint8_t childIndex = findDeviceIndexAtHubPort(di, pi);
				if (childIndex != 0xff) {
					disconnectDeviceAtIndex(childIndex);
				}
				iface->spec.hub.port_connected &= ((1 << pi) ^ 0xff);
			}
			s = hubdevice_clearPortFeature(di, pi, HUB_C_PORT_CONNECTION);
			DEBUG_DUMP_USB_TREE();
		}
		iface->spec.hub.port_flags = 0;
	}
	return (0);
}

uint8_t hubdevice_calculate_depth(USBDevice *dev)
{
    uint8_t rt = 0;
    while (dev->parentDevIndex != PARENT_NONE) {
        dev = &USBdevs[dev->parentDevIndex];
        rt++;
    }
    return rt;
}

uint8_t hubdevice_init_endpoint(USBDevice *dev, UDevInterface *iface, uint8_t endpoint)
{
    if ((endpoint & 0x80) == 0) {
        return 0;
    }
    if (hubdevice_calculate_depth(dev) >= USB_HUB_MAX_DEPTH) {
        DEBUG_OUT("Hub @%d: too many levels, children will not be enumated.\n", dev->address);
        dev->iface->spec.hub.num_ports = 0;
        return 0;
    }
    uint8_t di = dev->address - FIRST_USB_DEV_ID; // FIXME!: indexing
    hubdevice_getDescriptor(di);
    //DEBUG_OUT("Hub has %d ports.\n", iface->spec.hub.num_ports);
    for (uint8_t pi = 0; pi < iface->spec.hub.num_ports; pi++) {
        hubdevice_setPortFeature(di, pi + 1, HUB_PORT_POWER);
    }
    for (uint8_t pi = 0; pi < iface->spec.hub.num_ports; pi++) {
        hubdevice_clearPortFeature(di, pi + 1, HUB_C_PORT_CONNECTION);
    }/*
    for (uint8_t pi = 0; pi < iface->spec.hub.num_ports; pi++) {
        hubdevice_getPortStatus(di, pi + 1);
    }*/
    iface->spec.hub.port_flags = 0xff;
    iface->spec.hub.port_connected = 0;
    return 0;
}
