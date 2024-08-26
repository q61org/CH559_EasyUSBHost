#ifndef __USBHOST_H__
#define __USBHOST_H__

struct _RootHubDevice
{
	unsigned char status;
	unsigned char address;
	unsigned char speed;
};

struct udev_spec_hub_t
{
	uint8_t num_ports;
	uint8_t port_flags;
	uint8_t port_connected;
};
#define USB_HUB_MAX_DEPTH 4

struct hid_report_t
{
	uint8_t size;
	uint8_t count;
	uint8_t usage;
};

#define UDEV_HID_MAX_NUM_REPORTS 8

struct udev_spec_hid_t
{
	uint8_t num_reports;
	struct hid_report_t reports[UDEV_HID_MAX_NUM_REPORTS];
};

struct udev_interface_t
{
	uint8_t interface;
	uint8_t class;
	uint8_t subclass;
	uint8_t usage;
	uint8_t usagePage;
	uint8_t ep_in;
	uint8_t ep_out;
	union {
		struct udev_spec_hub_t hub;
		struct udev_spec_hid_t hid;
	} spec;
};
typedef struct udev_interface_t __xdata UDevInterface;
#define MAX_INTERFACES_PER_DEVICE 4

#define USB_DEVICE_STRING_MAXLEN 16
struct usbdevice_t
{
	uint8_t connected;
	uint8_t rootHubIndex;
	uint8_t address;
	uint8_t speed;
	uint8_t class;
	uint8_t subclass;
	uint8_t vid_l;
	uint8_t vid_h;
	uint8_t pid_l;
	uint8_t pid_h;
	uint8_t endpoint0size;
	uint8_t parentDevIndex;
	uint8_t parentDevPortIndex;
	uint8_t num_ifaces;
	struct udev_interface_t iface[MAX_INTERFACES_PER_DEVICE];
};
typedef struct usbdevice_t __xdata USBDevice;
#define FIRST_USB_DEV_ID 16
#define MAX_USB_DEVICES 16
#define PARENT_NONE 255


typedef void __code (*UDevAttachCallback)(uint8_t dev_index, USBDevice *udev, uint8_t is_attach);


extern USBDevice USBdevs[];
extern __at(0x0000) __xdata uint8_t RxBuffer[];
extern __at(0x0040) __xdata uint8_t TxBuffer[];
#define RECEIVE_BUFFER_LEN    512
extern __xdata uint8_t receiveDataBuffer[];


#define ROOT_HUB_COUNT  2

#define MAX_EXHUB_PORT_COUNT    4
#define EXHUB_PORT_NONE         0xff
#define MAX_INTERFACE_COUNT     4
#define MAX_ENDPOINT_COUNT      4
#define MAX_EXHUB_LEVEL         1
#define ENDPOINT_OUT            0
#define ENDPOINT_IN             1


#define ERR_SUCCESS         0x00
#define ERR_USB_CONNECT     0x15
#define ERR_USB_DISCON      0x16
#define ERR_USB_BUF_OVER    0x17
#define ERR_USB_DISK_ERR    0x1F
#define ERR_USB_TRANSFER    0x20 
#define ERR_USB_UNSUPPORT   0xFB
#define ERR_USB_UNKNOWN     0xFE

#define ROOT_DEVICE_DISCONNECT  0
#define ROOT_DEVICE_CONNECTED   1
#define ROOT_DEVICE_FAILED      2
#define ROOT_DEVICE_SUCCESS     3

/*#define DEV_TYPE_KEYBOARD   ( USB_DEV_CLASS_HID | 0x20 )
#define DEV_TYPE_MOUSE      ( USB_DEV_CLASS_HID | 0x30 )
#define DEV_TYPE_JOYSTICK      ( USB_DEV_CLASS_HID | 0x40 )
#define DEV_TYPE_GAMEPAD      ( USB_DEV_CLASS_HID | 0x50 )*/

#define HID_SEG_KEYBOARD_MODIFIER_INDEX 0
#define HID_SEG_KEYBOARD_VAL_INDEX      1
#define HID_SEG_BUTTON_INDEX            2
#define HID_SEG_X_INDEX                 3
#define HID_SEG_Y_INDEX                 4
#define HID_SEG_WHEEL_INDEX             5
#define HID_SEG_COUNT                   6


#define REPORT_USAGE_PAGE 		0x04
#define REPORT_USAGE 			0x08
#define REPORT_LOCAL_MINIMUM 	0x14
#define REPORT_LOCAL_MAXIMUM 	0x24
#define REPORT_PHYSICAL_MINIMUM 0x34
#define REPORT_PHYSICAL_MAXIMUM 0x44
#define REPORT_USAGE_MINIMUM	0x18
#define REPORT_USAGE_MAXIMUM	0x28

#define REPORT_UNIT				0x64
#define REPORT_INPUT			0x80
#define REPORT_OUTPUT 			0x90
#define REPORT_FEATURE			0xB0

#define REPORT_REPORT_SIZE		0x74
#define REPORT_REPORT_ID		0x84
#define REPORT_REPORT_COUNT		0x94

#define REPORT_COLLECTION		0xA0
#define REPORT_COLLECTION_END	0xC0

#define REPORT_USAGE_UNKNOWN	0x00
#define REPORT_USAGE_POINTER	0x01
#define REPORT_USAGE_MOUSE		0x02
#define REPORT_USAGE_RESERVED	0x03
#define REPORT_USAGE_JOYSTICK	0x04
#define REPORT_USAGE_GAMEPAD	0x05
#define REPORT_USAGE_KEYBOARD	0x06
#define REPORT_USAGE_KEYPAD		0x07
#define REPORT_USAGE_MULTI_AXIS	0x08
#define REPORT_USAGE_SYSTEM		0x09

#define REPORT_USAGE_X			0x30
#define REPORT_USAGE_Y			0x31
#define REPORT_USAGE_Z			0x32
#define REPORT_USAGE_Rx			0x33
#define REPORT_USAGE_Ry			0x34
#define REPORT_USAGE_Rz			0x35
#define REPORT_USAGE_WHEEL		0x38

#define REPORT_USAGE_PAGE_GENERIC	0x01
#define REPORT_USAGE_PAGE_KEYBOARD 	0x07
#define REPORT_USAGE_PAGE_LEDS		0x08
#define REPORT_USAGE_PAGE_BUTTON	0x09
#define REPORT_USAGE_PAGE_VENDOR	0xff00

void fillTxBuffer(const __code uint8_t *data, unsigned char len);
unsigned char hostCtrlTransfer(unsigned char __xdata *DataBuf, unsigned short *RetLen, unsigned short maxLenght);
unsigned char hostTransfer(unsigned char endp_pid, unsigned char tog, unsigned short timeout );
void selectUSBDevice(uint8_t devIndex);
unsigned char initializeRootHubConnection(unsigned char rootHubIndex, uint8_t parentIndex, uint8_t parentPortIndex, uint8_t speed);
unsigned char getDeviceString(uint8_t index);
char convertStringDescriptor(unsigned char __xdata *usbBuffer, unsigned char __xdata *strBuffer, unsigned short bufferLength);

void resetRootHub(unsigned char i);
void initUSB_Host();
unsigned char checkRootHubConnections();

void setAttachCallback(UDevAttachCallback func);
void callAttachCallback(uint8_t devIndex, USBDevice *dev, uint8_t is_attach);

#endif