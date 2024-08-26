#include "sdcc_keywords.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "CH559.h"
#include "util.h"
#include "USBHost.h"
#include "udev_hid.h"
#include "udev_util.h"

uint8_t __xdata keyboardIsConnected = 0;

__code unsigned char  SetHIDIdleRequest[] = {USB_REQ_TYP_CLASS | USB_REQ_RECIP_INTERF, HID_SET_IDLE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
__code unsigned char  GetHIDReport[] = {USB_REQ_TYP_IN | USB_REQ_RECIP_INTERF, USB_GET_DESCRIPTOR, 0x00, USB_DESCR_TYP_REPORT, 0 /*interface*/, 0x00, 0xff, 0x00};
__code unsigned char HIDSetReportRequest[] = { USB_REQ_TYP_CLASS | USB_REQ_RECIP_INTERF, HID_SET_REPORT, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

__code unsigned char GIPStartInput[] = {0x05, 0x20, 0x01, 0x01, 0x00 };

void parseHIDDeviceReport(unsigned char __xdata *report, unsigned short length, UDevInterface *dst_iface)
{
	unsigned short i = 0;
	unsigned char level = 0;
	unsigned char isUsageSet = 0;
	while(i < length)
	{
		unsigned char j;
		unsigned char id = report[i] & 0b11111100;
		unsigned char size = report[i] & 0b00000011;
		unsigned long data = 0;
		if(size == 3) size++;
		for(j = 0; j < size; j++)
			data |= ((unsigned long)report[i + 1 + j]) << (j * 8);
		for(j = 0; j < level - (id == REPORT_COLLECTION_END ? 1 : 0); j++)
			DEBUG_OUT("    ");
		switch(id)
		{
			case REPORT_USAGE_PAGE:	//todo clean up defines (case)
			{
				unsigned long vd = data < REPORT_USAGE_PAGE_VENDOR ? data : REPORT_USAGE_PAGE_VENDOR;
				DEBUG_OUT("Usage page ");
				switch(vd)
				{
					case REPORT_USAGE_PAGE_LEDS:
						DEBUG_OUT("LEDs");
					break;
					case REPORT_USAGE_PAGE_KEYBOARD:
						DEBUG_OUT("Keyboard/Keypad");
					break;
					case REPORT_USAGE_PAGE_BUTTON:
						DEBUG_OUT("Button");
					break;
					case REPORT_USAGE_PAGE_GENERIC:
						DEBUG_OUT("generic desktop controls");
					break;
					case REPORT_USAGE_PAGE_VENDOR:
						DEBUG_OUT("vendor defined 0x%04lx", data);
					break;
					default:
						DEBUG_OUT("unknown 0x%02lx", data);
				}
				DEBUG_OUT("\n");
				dst_iface->usagePage = vd;
			}
			break;
			case REPORT_USAGE:
				if (!isUsageSet){
					dst_iface->usage = data;
					isUsageSet = 1;
				} else {
					//DEBUG_OUT("second usage??");
					break;
				}
				DEBUG_OUT("Usage ");
				DEBUG_OUT_USB_REPORT_USAGE(data);
				DEBUG_OUT("\n");
			break;
			case REPORT_LOCAL_MINIMUM:
				DEBUG_OUT("Logical min %lu\n", data);
			break;
			case REPORT_LOCAL_MAXIMUM:
				DEBUG_OUT("Logical max %lu\n", data);
			break;
			case REPORT_PHYSICAL_MINIMUM:
				DEBUG_OUT("Physical min %lu\n", data);
			break;
			case REPORT_PHYSICAL_MAXIMUM:
				DEBUG_OUT("Physical max %lu\n", data);
			break;
			case REPORT_USAGE_MINIMUM:
				DEBUG_OUT("Physical min %lu\n", data);
			break;
			case REPORT_USAGE_MAXIMUM:
				DEBUG_OUT("Physical max %lu\n", data);
			break;
			case REPORT_COLLECTION:
				DEBUG_OUT("Collection start %lu\n", data);
				level++;
			break;
			case REPORT_COLLECTION_END:
				DEBUG_OUT("Collection end %lu\n", data);
				level--;
			break;
			case REPORT_UNIT:
				DEBUG_OUT("Unit 0x%02lx\n", data);
			break;
			case REPORT_INPUT:
				DEBUG_OUT("Input 0x%02lx\n", data);
			break;
			case REPORT_OUTPUT:
				DEBUG_OUT("Output 0x%02lx\n", data);
			break;
			case REPORT_FEATURE:
				DEBUG_OUT("Feature 0x%02lx\n", data);
			break;
			case REPORT_REPORT_SIZE:
				DEBUG_OUT("Report size %lu\n", data);
			break;
			case REPORT_REPORT_ID:
				DEBUG_OUT("Report ID %lu\n", data);
			break;
			case REPORT_REPORT_COUNT:
				DEBUG_OUT("Report count %lu\n", data);
			break;
			default:
				DEBUG_OUT("Unknown HID report identifier: 0x%02x (%i bytes) data: 0x%02lx\n", id, size, data);
		};
		i += size + 1;
	}
}

unsigned char getHIDDeviceReport(UDevInterface *iface)
{
 	unsigned char s;
	unsigned short len, reportLen = RECEIVE_BUFFER_LEN;
	DEBUG_OUT("Requesting report from interface %i\n", iface->interface);

	fillTxBuffer(SetHIDIdleRequest, sizeof(SetHIDIdleRequest));
	((PXUSB_SETUP_REQ)TxBuffer)->wIndexL = iface->interface;	
	s = hostCtrlTransfer(receiveDataBuffer, &len, 0);
	
	//todo really dont care if successful? 8bitdo faild here
	//if(s != ERR_SUCCESS)
	//	return s;

	fillTxBuffer(GetHIDReport, sizeof(GetHIDReport));
	((PXUSB_SETUP_REQ)TxBuffer)->wIndexL = iface->interface;
	((PXUSB_SETUP_REQ)TxBuffer)->wLengthL = (unsigned char)(reportLen & 255); 
	((PXUSB_SETUP_REQ)TxBuffer)->wLengthH = (unsigned char)(reportLen >> 8);
	s = hostCtrlTransfer(receiveDataBuffer, &len, RECEIVE_BUFFER_LEN);
	if(s != ERR_SUCCESS)
		return s;
	/*
	for (i = 0; i < len; i++)
	{
		DEBUG_OUT("0x%02X ", receiveDataBuffer[i]);
	}
	DEBUG_OUT("\n");*/
	//sendProtocolMSG(MSG_TYPE_HID_INFO, len, CurrentDevive, HIDdevice[CurrentDevive].interface, HIDdevice[CurrentDevive].rootHub, receiveDataBuffer);
	parseHIDDeviceReport(receiveDataBuffer, len, iface);
	return (ERR_SUCCESS);
}


uint8_t getHIDDeviceIndexList(uint8_t usage, __xdata uint8_t *dst, uint8_t maxlen)
{
	uint8_t rt = 0;
	for (uint8_t di = 0; di < MAX_USB_DEVICES; di++) {
		USBDevice *d = &USBdevs[di];
		if (!d->connected) continue;
		for (uint8_t ii = 0; ii < d->num_ifaces; ii++) {
			UDevInterface *iface = &d->iface[ii];
			if (iface->class != USB_DEV_CLASS_HID) continue;
			if (iface->usage != usage) continue;
			if (rt >= maxlen) break;
			dst[rt++] = di;
			break;
		}
		if (rt >= maxlen) break;
	}
	return rt;
}

uint8_t pollHIDDevice(uint8_t devIndex, uint8_t usage, __xdata uint8_t *dst, uint8_t maxlen)
{
	USBDevice *d = &USBdevs[devIndex];
	if (!d->connected) return 0;
	for (uint8_t ii = 0; ii < d->num_ifaces; ii++) {
		UDevInterface *iface = &d->iface[ii];
		//if (iface->class != USB_DEV_CLASS_HID) continue;
		if (iface->usage != usage) continue;
		if (iface->ep_in == 0) continue;

		selectUSBDevice(devIndex);
		uint8_t s = hostTransfer(USB_PID_IN << 4 | (iface->ep_in & 0x7F), (iface->ep_in & 0x80) ? bUH_R_TOG | bUH_T_TOG : 0, 0);
		if (s == ERR_SUCCESS) {
			iface->ep_in ^= 0x80;
			uint8_t len = USB_RX_LEN;
			if (len > 0) {
				if (len > maxlen) len = maxlen;
				for (uint8_t i = 0; i < len; i++) {
					dst[i] = RxBuffer[i];
				}
				return len;
			}
		}
	}
	return 0;
}


uint8_t setHIDDeviceLED(uint8_t devIndex, uint8_t usage, uint8_t led)
{
	uint8_t rt = 0;
	static __xdata uint8_t txdata;
	txdata = led;
	unsigned short len = 0;
	// setup method
	USBDevice *d = &USBdevs[devIndex];
	if (!d->connected) return 0;

	for (uint8_t ii = 0; ii < d->num_ifaces; ii++) {
		UDevInterface *iface = &d->iface[ii];
		if (iface->class != USB_DEV_CLASS_HID) continue;
		if (iface->usage != usage) continue;

		selectUSBDevice(devIndex);
		fillTxBuffer(HIDSetReportRequest, sizeof(HIDSetReportRequest));
		((PXUSB_SETUP_REQ)TxBuffer)->wIndexL = iface->interface;
		((PXUSB_SETUP_REQ)TxBuffer)->wLengthL = 1;
		((PXUSB_SETUP_REQ)TxBuffer)->wValueH = 0x02;
		//DEBUG_OUT("setting led 0x%02x", dt);
		rt = hostCtrlTransfer(&txdata, &len, 1);
		break;
	}
	return rt;
}

uint8_t hiddevice_start_input(uint8_t devIndex, uint8_t usage)
{
	USBDevice *d = &USBdevs[devIndex];
	if (!d->connected) return 0;
	for (uint8_t ii = 0; ii < d->num_ifaces; ii++) {
		UDevInterface *iface = &d->iface[ii];
		//if (iface->class != USB_DEV_CLASS_HID) continue;
		//if (iface->usage != usage) continue;
		if (iface->ep_out & 0x7f != 4) continue;

		selectUSBDevice(devIndex);
		fillTxBuffer(GIPStartInput, sizeof(GIPStartInput));
		UH_TX_LEN = sizeof(GIPStartInput);
		uint8_t s = hostTransfer(USB_PID_OUT << 4 | (iface->ep_out & 0x7F), (iface->ep_out & 0x80) ? bUH_R_TOG | bUH_T_TOG : 0, 0);
		if (s == ERR_SUCCESS) {
			iface->ep_out ^= 0x80;
			uint8_t len = UH_TX_LEN;
			return (len != sizeof(GIPStartInput));
		} else {
			return s;
		}
	}
	return 255;
#if 0
	uint8_t rt = 0;
	unsigned short len = 0;
	// setup method
	USBDevice *d = &USBdevs[devIndex];
	if (!d->connected) return 0;

	for (uint8_t ii = 0; ii < d->num_ifaces; ii++) {
		UDevInterface *iface = &d->iface[ii];
		if (iface->class != USB_DEV_CLASS_VEN_SPEC) continue;
		if (iface->usage != usage) continue;

		selectUSBDevice(devIndex);
		fillTxBuffer(GIPStartInput, sizeof(GIPStartInput));
		((PXUSB_SETUP_REQ)TxBuffer)->wIndexL = iface->interface;
		((PXUSB_SETUP_REQ)TxBuffer)->wLengthL = 1;
		((PXUSB_SETUP_REQ)TxBuffer)->wValueH = 0x02;
		//DEBUG_OUT("setting led 0x%02x", dt);
		rt = hostCtrlTransfer(receiveDataBuffer, &len, 0);
		break;
	}
	return rt;
#endif
}

uint8_t hiddevice_init_endpoint(USBDevice *dev, UDevInterface *iface, uint8_t endpoint)
{
    if(endpoint & 0x80){
        getHIDDeviceReport(iface);
    }
    return 0;
}
