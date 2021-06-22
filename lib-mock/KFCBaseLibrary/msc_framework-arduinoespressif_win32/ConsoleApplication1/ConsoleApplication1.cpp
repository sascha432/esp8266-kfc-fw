/**
* Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>

#define Serial Serial0 // allows to call begin() etc.. since Serial is a Stream object not HardwareSerial, pointing to Serial0

void setup()
{
    Serial.begin(115200);
}

void loop() 
{
    Serial.printf_P(PSTR("millis %u\n"), millis());
    delay(1000);
}
