#include "sdcc_keywords.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "CH559.h"
#include "util.h"
#include "USBHost.h"
#include "udev_util.h"


void DEBUG_OUT_DEVICE_STRING(uint8_t index)
{
	uint8_t s = getDeviceString(index);
	if (s != ERR_SUCCESS) return;
	if(convertStringDescriptor(receiveDataBuffer, receiveDataBuffer, RECEIVE_BUFFER_LEN)) {
		DEBUG_OUT("%s", receiveDataBuffer);
	}
}

inline void DEBUG_OUT_USB_DEV_CLASS(uint8_t class)
{
	switch (class) {
		case USB_DEV_CLASS_AUDIO: DEBUG_OUT("audio"); break;
		case USB_DEV_CLASS_COMMUNIC: DEBUG_OUT("communication"); break;
		case USB_DEV_CLASS_HID: DEBUG_OUT("HID"); break;
		case USB_DEV_CLASS_MONITOR: DEBUG_OUT("monitor"); break;
		case USB_DEV_CLASS_POWER: DEBUG_OUT("power"); break;
		case USB_DEV_CLASS_PRINTER: DEBUG_OUT("printer"); break;
		case USB_DEV_CLASS_STORAGE: DEBUG_OUT("storage"); break;
		case USB_DEV_CLASS_HUB: DEBUG_OUT("hub"); break;
		case USB_DEV_CLASS_VEN_SPEC: DEBUG_OUT("vendor-specific"); break;
		default: DEBUG_OUT("unknown"); break;
	}
}

void DEBUG_OUT_USB_REPORT_USAGE(uint8_t data)
{
	switch(data)
	{
		case REPORT_USAGE_UNKNOWN:
			DEBUG_OUT("Unknown");
		break;
		case REPORT_USAGE_POINTER:
			DEBUG_OUT("Pointer");
		break;
		case REPORT_USAGE_MOUSE:
			DEBUG_OUT("Mouse");
		break;
		case REPORT_USAGE_RESERVED:
			DEBUG_OUT("Reserved");
		break;
		case REPORT_USAGE_JOYSTICK:
			DEBUG_OUT("Joystick");
		break;
		case REPORT_USAGE_GAMEPAD:
			DEBUG_OUT("Gamepad");
		break;
		case REPORT_USAGE_KEYBOARD:
			DEBUG_OUT("Keyboard");
		break;
		case REPORT_USAGE_KEYPAD:
			DEBUG_OUT("Keypad");
		break;
		case REPORT_USAGE_MULTI_AXIS:
			DEBUG_OUT("Multi-Axis controller");
		break;
		case REPORT_USAGE_SYSTEM:
			DEBUG_OUT("Tablet system controls");
		break;

		case REPORT_USAGE_X:
			DEBUG_OUT("X");
		break;
		case REPORT_USAGE_Y:
			DEBUG_OUT("Y");
		break;
		case REPORT_USAGE_Z:
			DEBUG_OUT("Z");
		break;
		case REPORT_USAGE_WHEEL:
			DEBUG_OUT("Wheel");
		break;
		default:
			DEBUG_OUT("%x", data);
	}
}



void readInterface(unsigned char rootHubIndex, PXUSB_ITF_DESCR interface)
{
	unsigned char temp = rootHubIndex;
	DEBUG_OUT("Interface %d\n", interface->bInterfaceNumber);
	DEBUG_OUT("  Class %d\n", interface->bInterfaceClass);
	DEBUG_OUT("  Sub Class %d\n", interface->bInterfaceSubClass);
	DEBUG_OUT("  Interface Protocol %d\n", interface->bInterfaceProtocol);
	DEBUG_OUT("  Alternate Setting %d\n", interface->bAlternateSetting);
	DEBUG_OUT("  Endpoint Count %d\n", interface->bNumEndpoints);
}

void readHIDInterface(PXUSB_ITF_DESCR interface, PXUSB_HID_DESCR descriptor)
{
	DEBUG_OUT("HID at Interface %d\n", interface->bInterfaceNumber);
	DEBUG_OUT("  USB %d.%d%d\n", (descriptor->bcdHIDH & 15), (descriptor->bcdHIDL >> 4), (descriptor->bcdHIDL & 15));
	DEBUG_OUT("  Country code 0x%02X\n", descriptor->bCountryCode);
	DEBUG_OUT("  TypeX 0x%02X\n", descriptor->bDescriptorTypeX);
}


void DEBUG_DUMP_USB_DEVICE(USBDevice *dev, uint8_t depth)
{
	for (uint8_t i = 0; i < depth; i++) {
		DEBUG_OUT("| ");
	}
	if (dev->parentDevIndex != PARENT_NONE) {
		DEBUG_OUT("[%d] @%d", dev->parentDevPortIndex, dev->address);
	} else {
		DEBUG_OUT("+ @%d", dev->address);
	}
	DEBUG_OUT(" %02x%02x", dev->vid_h, dev->vid_l);
	DEBUG_OUT("/%02x%02x", dev->pid_h, dev->pid_l);
	DEBUG_OUT(" class %d", dev->class);
	DEBUG_OUT("(");
	DEBUG_OUT_USB_DEV_CLASS(dev->class);
	DEBUG_OUT(")\n");
	for (uint8_t i = 0; i < depth + 1; i++) {
		DEBUG_OUT("| ");
	}
	selectUSBDevice(dev->address - FIRST_USB_DEV_ID);
	DEBUG_OUT("> ");
	DEBUG_OUT_DEVICE_STRING(1);
	DEBUG_OUT(" / ");
	DEBUG_OUT_DEVICE_STRING(2);
	DEBUG_OUT("\n");

	for (uint8_t ii = 0; ii < dev->num_ifaces; ii++) {
		UDevInterface *iface = &dev->iface[ii];
		for (uint8_t i = 0; i < depth + 1; i++) {
			DEBUG_OUT("| ");
		}
		DEBUG_OUT("> interf %d", iface->interface);
		DEBUG_OUT(" class %d", iface->class);
		DEBUG_OUT("(");
		DEBUG_OUT_USB_DEV_CLASS(iface->class);
		DEBUG_OUT(")");
		if (iface->class != USB_DEV_CLASS_HUB) {
			DEBUG_OUT(" usage %d", iface->usage);
			DEBUG_OUT("(");
			DEBUG_OUT_USB_REPORT_USAGE(iface->usage);
			DEBUG_OUT(")");
		} else if (iface->class == USB_DEV_CLASS_HUB) {
			DEBUG_OUT(" %d ports", iface->spec.hub.num_ports);
		}
		if (iface->ep_in) {
			DEBUG_OUT(" endpt IN %02x", iface->ep_in);
		} else if (iface->ep_out) {
			DEBUG_OUT(" endpt OUT %02x", iface->ep_out);
		}
		DEBUG_OUT("\n");
	}
}

void DEBUG_DUMP_USB_TREE()
{
	uint8_t parent_stack[USB_HUB_MAX_DEPTH * 2];
	uint8_t stack_pos = 0;
	uint8_t current_parent = PARENT_NONE;
	uint8_t depth = 0;

	int8_t i = MAX_USB_DEVICES - 1;
rescan: // using loops instead of recursion to save on stack (not sure if it really is using less stack)
	for (; i >= 0; i--) {
		USBDevice *dev = &USBdevs[i];
		if (!dev->connected) continue;
		if (dev->parentDevIndex == current_parent) {
			DEBUG_DUMP_USB_DEVICE(dev, depth);
			if (dev->class == USB_DEV_CLASS_HUB) {
				++depth;
				parent_stack[stack_pos++] = current_parent;
				parent_stack[stack_pos++] = i - 1;
				current_parent = i;
				i = MAX_USB_DEVICES - 1;
				goto rescan;
			}
		}
	}
	if (depth > 0) {
		--depth;
		i = parent_stack[--stack_pos];
		current_parent = parent_stack[--stack_pos];
		goto rescan;
	}
}

