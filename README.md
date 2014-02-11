Door Controller Test Tool
=========================

- [Overview](#overview)
- [Requirements](#requirements)
 - [Hardware](#hardware)
 - [Software](#software)
- [Quick Start](#quick-start)
 - [Basic Installation Steps](#basic-installation-steps)

## Overview
The Door Controller Test Tool is an input stimulator and output reader for physical access control systems, which uses the Arduino platform. It’s purpose is to aid in the testing of PACS devices by facilitating automated and manual tests. It does this by enabling you to generate input data to simulate the following devices:

* Wiegand reader data
* REX button
* Door monitor

And providing feedback for output pins, namely:

* Lock
* Reader green LED
* Reader beeper

Two interfaces are provided for controlling input and reading output; HTTP and WebSockets. Additionally, a Web GUI (which uses the WebSockets interface and resides on the Arduino itself) is provided.

## Requirements

### Hardware

* Arduino Mega 2560
* Arduino Ethernet Shield, for network connectivity. For powering the Arduino, get one with Power over Ethernet support.
* SD Card (needs to store less than 100kB, so smallest one you can find will suffice)
* Ethernet TP cable, for connecting the Arduino to a switch/router.
* Flat cable (or other cabling preference), for connecting the Arduino to the PACS device. How the actual physical cabling is performed is left for the user to decide. 
* USB cable, 4pin USB Type A-M - 4pin USB Type B. For uploading software and viewing debug output (it can also be used to power the Arduino).

### Software

* [The Arduino IDE](http://arduino.cc/en/main/software) - For uploading the Door Controller Test Tool software to the Arduino.
* To use the web GUI, a browser which supports both WebSockets RFC6455 and AngularJS is needed (Chrome version 30+ should work fine).

## Quick Start

### Basic Installation Steps
1. Clone the Door Controller Test Tool repository to your hard drive.
2. Edit the sd_card/config/pins.cfg file to create pin mappings. See “Configuring the Pins” for further details.
3. Edit the sd_card/config/doors.cfg file to create logical doors. See “Configuring the Doors” for further details.
4. Edit the sd_card/config/network.cfg to set up the Arduino network interface. See “Configuring the Network” for further details.
5. Wire up the Arduino to the PACS device according to the pins- and doors configuration. See “Connecting the Arduino to the PACS Device” for further details.
6. Format the SD card to FAT16, copy the contents of sd_card to the root of the card and insert it into the Arduino Ethernet Shield SD card slot.
7. Upload the Door Controller Test Tool software to the Arduino. See “Uploading the Door Controller Test Tool Software” for further details.
8. You should now be able to start using Door Controller Test Tool by navigating to its IP address in a browser (or sending commands via HTTP/Websockets).
