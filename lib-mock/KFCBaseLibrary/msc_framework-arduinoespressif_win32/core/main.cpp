/**
* Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
//#include "ets_sys_win32.h"

//int main() {
//
//    SetConsoleOutputCP(CP_UTF8);
//    ESP._enableMSVCMemdebug();
//    DEBUG_HELPER_INIT();
//
//    for (int i = 0; i < 10; i++) {
//        Serial.printf_P(PSTR("millis %u\n"), millis());
//        delay(1000);
//    }
//
//    return 0;
//}

#include <Arduino_compat.h>

void setup()
{
    static_cast<HardwareSerial &>(Serial).begin(115200);
}

void loop()
{
    Serial.printf_P(PSTR("millis %u\n"), millis());
    delay(1000);
}
