/**
* Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include <OSTimer.h>
#include <EventScheduler.h>

#ifdef Serial
#undef Serial
#endif
#define Serial Serial0 // allows to call begin() etc.. since Serial is a Stream object not HardwareSerial, pointing to Serial0

class TestTimer : public OSTimer /* ETSTimer implementation */ {
public:
    TestTimer() : OSTimer(OSTIMER_NAME("TestTimer")) {
    }

    virtual void run() override {
        Serial.printf_P(PSTR("OSTimer %.3fms\n"), micros() / 1000.0);
    }
};

TestTimer testTimer1;

void setup()
{
    Serial.begin(115200);

    // ETSTimer/OSTimer test
    testTimer1.startTimer(15000, true);

    // Scheduler test
    _Scheduler.add(Event::seconds(30), 10, [](Event::CallbackTimerPtr timer) {
        Serial.printf_P(PSTR("Scheduled event %.3fs, %u/%u\n"), micros() / 1000000.0, timer->_repeat.getRepeatsLeft() + 1, 10);
    });

    //ets_dump_timer(Serial); // #include "ets_timer_win32.h"
    Serial.println(F("Press any key to display millis(). Ctrl+C to end program..."));
}



void loop() 
{
    if (Serial.available()) {
        Serial.printf_P(PSTR("millis()=%u\n"), millis());
        Serial.read();
    }
    delay(50);
}
