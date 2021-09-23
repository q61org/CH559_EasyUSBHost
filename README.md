# CH559 EasyUSBHost for HID Keyboards

A little HID keyboard -> UART adapter that translates keyboard inputs to ASCII texts.
Lets you add keyboard inputs to your gadgets very easily.

## What It Does

* Translates HID keyboard inputs to ASCII texts and outputs via UART.
  * Non-letter keys such as cursor keys are ignored (by default) or translated to VT100 (or xterm) control sequences.
  * Caps lock and num lock are treated accordingly, including indicator LEDs on keyboard.
  * Scroll lock key turns indicator LED on/off, but does not affect text output (by default).
  * Shift and ctrl keys are treated accordingly. Alt and meta keys are ignored (by default).
* Recognizes USB hubs and multiple keyboards. Maximum 16 USB devices (including hubs) and 8 keyboards.
  * Inputs from multiple keyboards are merged into single output stream.
  * Has a mode that prefixes address of the keyboard before ASCII value.
* Barcode readers and other input devices are also recognized, as long as they are HID-keyboard compatible.

## What It Doesn't Do

* It doesn't translate extra utility keys (such as volume control and web launcher keys), as they are not part of generic HID keyboard protocol.

## Features That May Be Useful

* Supports JIS and US keyboard layouts.
* Configurable key repeats.
* Lock keys (caps, num, scroll) can be disabled individually.
  * You can change lock states via UART or SPI, even if keys are disabled.
* An open-collector pin that goes low when special key combination is pressed (default: ctrl + alt + del).
  * Can be used to reset microcontroller or other external circuits.
* Ctrl <-> capslock swapping.

## Hardware Setup

Simply hook up CH559L as:

* USB: HP and HM pins (pins 29 and 30) to a USB A receptacle.
* UART1: RXD1 and TXD1 pins (pins 27 and 28) to a receiver (such as Arduino and other microcontrollers, anything you would like).
* DP and DM pins (pins 31 and 32) are only used for downloading firmware.
* Power pins to appropriate power, of course.

TBA: example schematic

This project is not tested on CH559T.

## Compiling

You need SDCC and make to compile this project.

Additionally, this project uses `--xstack` option on SDCC.
Since SDCC doesn't include libc libraries with `--xstack` option in its default install, you need to compile (and install) such libraries in order to compile this project. You can do so by doing `make model-mcs51-xstack-auto` in `device/lib` directory of the SDCC source.

Modify Makefile so that `SDCC_PREFIX` points to where your SDCC is installed in.

This project is intended to be compiled in WSL (Ubuntu).

## License

GPL v3.

## Credits

This project is derived (and extensively modified) from [CH559sdccUSBHost](https://github.com/atc1441/CH559sdccUSBHost)

For my part:
>    CH559 EasyUSBHost for HID Keyboards
>    Copyright (C) 2021 Kouichi Kuroi (q61.org)
>
>    This program is free software: you can redistribute it and/or modify
>    it under the terms of the GNU General Public License as published by
>    the Free Software Foundation, either version 3 of the License, or
>    (at your option) any later version.
>
>    This program is distributed in the hope that it will be useful,
>    but WITHOUT ANY WARRANTY; without even the implied warranty of
>    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
>    GNU General Public License for more details.
>
>    You should have received a copy of the GNU General Public License
>    along with this program.  If not, see <https://www.gnu.org/licenses/>.
