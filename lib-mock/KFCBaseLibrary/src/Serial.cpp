/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#if _MSC_VER

#include "Serial.h"

HardwareSerial fakeSerial;
HardwareSerial &Serial0 = fakeSerial;
Stream &Serial = fakeSerial;

#endif
