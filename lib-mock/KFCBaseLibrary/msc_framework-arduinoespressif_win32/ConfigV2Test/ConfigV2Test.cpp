/**
* Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include "EEPROM.h"

void setup()
{
	if (!ESP.flashEraseSector(0)) {
		Serial.println(F("Failed to erase sector 0"));
	}

	//uint8_t buf[256];
	int len = 0;

	EEPROM.begin(2);
	EEPROM.write(0, 100);
	EEPROM.write(1, 200);
	EEPROM.commit();

	EEPROM.begin(3);
	char buf3[3];
	EEPROM.get(0, buf3);
	EEPROM.end();




	//ESP.flashWriepromeprote(0, buf, 10);

	//__dump_binary_to(Serial, buf, len);

	exit(0);
}

void loop()
{
}

