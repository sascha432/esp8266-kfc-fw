/**
* Author: sascha_lammers@gmx.de
*/

#if _WIN32

#include "Serial.h"

HardwareSerial fakeSerial;
HardwareSerial &Serial0 = fakeSerial;
Stream &Serial = fakeSerial;

#endif
