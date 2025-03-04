#ifndef __UDEV_HID_H__
#define __UDEV_HID_H__

enum {
    JOYSTICK_INPUT_TYPE_CONST = 0,
    JOYSTICK_INPUT_TYPE_ID = 1,
    JOYSTICK_INPUT_TYPE_BUTTON = 2,
    JOYSTICK_INPUT_TYPE_AXIS = 3,
    JOYSTICK_INPUT_TYPE_TRIGGER = 4,
    JOYSTICK_INPUT_TYPE_HAT_DEG = 5,
    JOYSTICK_INPUT_TYPE_DPAD = 6,
    JOYSTICK_INPUT_TYPE_AXIS_POSNEG_16BIT = 7,
    JOYSTICK_INPUT_TYPE_END = 15
};

uint8_t IsKeyboardConnected();
uint8_t pollHIDDevice(uint8_t devIndex, uint8_t usage, __xdata uint8_t *dst, uint8_t maxlen, UDevInterface **iface_dst);
uint8_t setHIDDeviceLED(uint8_t devIndex, uint8_t usage, uint8_t led);
uint8_t hiddevice_init_endpoint(USBDevice *dev, UDevInterface *iface, uint8_t endpoint);
uint8_t hiddevice_start_input(uint8_t devIndex, uint8_t usage);

#endif
