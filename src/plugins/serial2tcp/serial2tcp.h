/**
 * Author: sascha_lammers@gmx.de
 */

#if SERIAL2TCP

// Allows to connect over TCP to serial ports

#pragma once

#include <Arduino_compat.h>

#ifndef DEBUG_SERIAL2TCP
#define DEBUG_SERIAL2TCP 0
#endif


/**
 *
 * To connect to a remote serial port, run serial2tcp in server mode and use telnet from the remote machine to connect.
 *
 * If you use another ESP8266 in client mode and a Serial2USB adapter, you can directly use the port from the adapter.
 *
 * A virtual serial port can also be created.
 *
 * windows
 *
 * To use virtual ports with windows, you need to install a driver that can redirect TCP to a virtual port. The virtual port can be used with all software like a regular comport.
 * A free software with this  functionality is available at
 *
 * https://sourceforge.net/projects/com0com/
 *
 * The Null-modem emulator (com0com) is a kernel-mode virtual serial port driver for Windows. You can create an unlimited number
 * of virtual COM port pairs and use any pair to connect one COM port based application to another. The HUB for communications
 * (hub4com) allows to receive data and signals from one COM or TCP port, modify and send it to a number of other COM or TCP ports and vice versa.
 *
 * Alternative software, freeware but limited to a single port
 *
 * https://www.hw-group.com/software/hw-vsp3-virtual-serial-port
 *
 *
 */

/**

com0com

Run setupg.exe and create a port pair, on the left side use "use port class". That should create something like

COM8 and CNCB0

Connect CNCB0 to the tcp socket with the command line tool hub4com

com2tcp --baud 57600 \\.\CNCB0  192.168.0.72 2323

After that, connect to COM8 (COMxx) and it should be redirected through the TCP socket.

With NVT/RFC2217 support

com2tcp-rfc2217.bat \\.\CNCB0  192.168.0.72 2323

 */

#endif
