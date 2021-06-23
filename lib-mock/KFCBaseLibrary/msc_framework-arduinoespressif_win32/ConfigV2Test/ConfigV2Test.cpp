/**
* Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include "ets_sys_win32.h"
#include <thread>

void setup()
{
	if (!ESP.flashEraseSector(0)) {
		Serial.println(F("Failed to erase sector 0"));
	}
}

void loop()
{
	exit(0);
}
