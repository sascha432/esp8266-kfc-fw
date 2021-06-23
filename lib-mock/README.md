# Mock libraries for compiling for Win32

## Current Status

It is "work in progress". Most Arduino based code is working and the ESP8266 framework is partially implemented.

Serial as mapped to the Window Console for I/O and debugging.

## GPIO Support

255 GPIO pins are supported, digital and analog.

## Fash Memory Emulation

By default, the flash memory is organized in a split configuration `conf\ld\eagle.flash.4m2m.ld`

- 4MByte Flash Memory in 4KB blocks
- 1MByte for code
- 1Mbyte free
- 2Mbyte for SPIFFS/LittleFS, EEPROM and other types

See `lib\KFCBaseLibrary\include\section_defines.h` for details. The data is stored in `FLASH_MEMORY_STORAGE_FILE` (default `EspFlashMemory.4m2m.dat`)

## IRAM/DRAM Support

There is no support for IRAM/DRAM and the default Windows memory allocator is used. it is possible to limit the read/write access to a certain area. Details can be found in the EEPROM library

## EEPROM Emulation

The EEPROM is a single 4096 Byte Flash Sector based on the Flash Emulation

## Win32 GUI support

Support for TFT displays is implemented via Adafruit's GFX library, which requires to compile it was Win32 Application instead of Console and add the GFX mock library. A static library will be available soon...

## Static Library for Win32 Console

`lib-mock\KFCBaseLibrary\msc_framework-arduinoespressif_win32\Debug` contains a static library `framework-arduinoespressif.lib` for Visual Studio to compile ESP8266 application on Win32/x86/DEBUG.

## Example

ConsoleApplication1 is an example. Press Ctrl+C to end the program...

```c++
#include <Arduino_compat.h>
#include <OSTimer.h>
#include <EventScheduler.h>

#define Serial Serial0 // allows to call begin() etc.. since Serial is a Stream object not HardwareSerial, pointing to Serial0

class TestTimer : public OSTimer /* ETSTimer implementation */ {
public:
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

    Serial.println(F("Press any key to display millis(). Ctrl+C to end program..."));
}

void loop()
{
    if (Serial.available()) {
        Serial.printf_P(PSTR("millis %u\n"), millis());
        Serial.read();
    }
    delay(50);
}
```
