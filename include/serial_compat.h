/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

// The Arduino core only declares the global Serial if NO_GLOBAL_SERIAL is not defined. This
// firmware owns UART0 (Serial0) and redirects Serial/DebugSerial to a Stream, see
// serial_handler.cpp, so NO_GLOBAL_SERIAL is set and the core declares nothing at all.
//
// The fork of the core had these declarations in HardwareSerial.h
// (#if defined(HAVE_KFC_FIRMWARE_VERSION) extern Stream &Serial;). With a stock core they have to
// be forced into every translation unit (build_flags "-include serial_compat.h"), otherwise third
// party libraries that use Serial (Adafruit_Sensor, asyncHTTPrequest, ...) do not compile.

#if defined(__cplusplus)

#include <Arduino.h>
#include <HardwareSerial.h>

// the firmware owns the UARTs (serial_handler.cpp defines Serial0 and Serial1) and redirects
// Serial/DebugSerial to Stream objects, so the core's global instances are disabled with
// NO_GLOBAL_SERIAL

extern Stream &Serial;
extern Stream &DebugSerial;

#if ESP32
// HardwareSerial.h does not declare them at all with NO_GLOBAL_SERIAL, the instances live in
// serial_handler.cpp
// NOTE: on ESP8266 the core still declares Serial1 (only NO_GLOBAL_SERIAL1 disables that one)
extern HardwareSerial Serial0;
extern HardwareSerial Serial1;
#endif

#endif
