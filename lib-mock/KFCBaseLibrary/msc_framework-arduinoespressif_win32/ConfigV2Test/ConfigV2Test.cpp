/**
* Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include "EEPROM.h"

void setup()
{
	ESP.flashDump(Serial);
	exit(0);

#if 0
	if (!ESP.flashEraseSector(0)) {
		Serial.println(F("Failed to erase sector 0"));
	}

	const char *buf2 = "test";
	ESP.flashWrite(32, (uint32_t*)&buf2[0], 5);

	uint8_t buf3[1024];
	ESP.flashRead(0, buf3, sizeof(buf3));

	__dump_binary_to(Serial, buf3, sizeof(buf3));

	exit(1);
#endif
#if 0
	int len = 0;

	EEPROM.begin(2);
	EEPROM.write(0, 0x21);
	EEPROM.write(1, 0x43);
	EEPROM.commit();

	EEPROM.begin(4);
	uint32_t buf;
	EEPROM.get(0, buf);

	Serial.printf("%08x\n", buf);

	EEPROM.end();

	exit(0);
#endif
}

void loop()
{
}

