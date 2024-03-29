END-OF-LIFE NOTICE
=========================
Axis Communications is no longer maintaining this project.

Door Controller Test Tool
=========================

- [Overview](#overview)
- [Requirements](#requirements)
 - [Hardware](#hardware)
 - [Software](#software)
- [Getting Started](#getting-started)
 - [Dependencies](#dependencies)
 - [Basic Installation Steps](#basic-installation-steps)

## Overview
The Door Controller Test Tool is an input stimulator and output reader for physical access control systems, which uses the Arduino platform. It’s purpose is to aid in the testing of PACS devices by facilitating automated and manual tests. It does this by enabling you to generate input data to simulate the following devices:

* Wiegand reader data
* REX button
* Door monitor
* Digital input/Switches

And providing feedback for output pins, namely:

* Lock
* Reader green LED
* Reader beeper
* Digital output/Relays

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
* There are dependencies to other Arduino libraries, see "Quick Start" for further details.

## Getting Started

### Dependencies
The following dependencies need to be downloaded and placed in your Arduino environment's "libraries" folder:

1. [Arduino WebSocket Server](https://github.com/AxisCommunications/arduino-websocket-server)
2. [StandardCplusplus](https://github.com/maniacbug/StandardCplusplus)
3. [Webduino](https://github.com/sirleech/Webduino)
4. [aJSON](https://github.com/interactive-matter/aJson)

NOTE: After installing the StandardCplusplus library, edit the [ArduinoLibraryFolder]/StandardCplusplus/system_configuration.h file and change
`#define __UCLIBCXX_STL_BUFFER_SIZE__ 8` to `#define __UCLIBCXX_STL_BUFFER_SIZE__ 0`. We do this to avoid the vector implementation from allocating too much memory.

### Basic Installation Steps
1. Clone the Door Controller Test Tool repository to your hard drive.
2. Format the SD card to FAT16 and insert it into the Arduino Ethernet Shield SD card slot.
3. Upload the Door Controller Test Tool software to the Arduino. See “Uploading the Door Controller Test Tool Software” in the wiki for further details.
4. Configure the Network Settings. See "Configuring the Network" in the wiki for further details.
5. Create pin mappings. See "Configuring the Pins" in the wiki for futher details.
6. Create the logical doors. See "Configuring the Doors" in the wiki for further details.
7. Wire up the Arduino to the PACS device according to the pins- and doors configuration. See “Connecting the Arduino to the PACS Device” in the wiki  for further details.
8. You should now be able to start using Door Controller Test Tool by navigating to its IP address in a browser (or sending commands via HTTP/Websockets). See “Interfacing with the Door Controller Test Tool Software” in the wiki for further details

### Detailed Instructions

See the repository wiki.
