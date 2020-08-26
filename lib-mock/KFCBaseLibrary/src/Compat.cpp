/**
  Author: sascha_lammers@gmx.de
*/

#if _MSC_VER

#include "Arduino_compat.h"
#include <chrono>
#include <ets_timer_win32.h>
#include <winternl.h>
#include <algorithm>
#include <BufferStream.h>
#include <DumpBinary.h>

#include "EEPROM.h"

EEPROMClass EEPROM;
_SPIFFS SPIFFS;

uint32_t _EEPROM_start;

const String emptyString;

void throwException(PGM_P message) {
    printf("EXCEPTION: %s\n", (const char *)message);
    exit(-1);
}

class GPIOPin {
public:
    GPIOPin() {
        mode = 0;
        value = 0;
    }
    uint8_t mode;
    int value;
};

GPIOPin pins[256];

void pinMode(uint8_t pin, uint8_t mode)
{
    pins[pin].mode = mode;
    printf("pinMode(%u, %u)\n", pin, mode);
}


void digitalWrite(uint8_t pin, uint8_t value)
{
    pins[pin].value = value;
    printf("digitalWrite(%u, %u)\n", pin, value);
}

int digitalRead(uint8_t pin)
{
    return pins[pin].value ? HIGH : LOW;
}

int analogRead(uint8_t pin)
{
    return pins[pin].value;
}

void analogReference(uint8_t mode)
{
}

void analogWrite(uint8_t pin, int value)
{
    pins[pin].value = value;
    printf("analogWrite(%u, %u)\n", pin, value);
}

static auto micros_start_time = std::chrono::high_resolution_clock::now();

uint64_t micros64()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - micros_start_time).count();
}

unsigned long millis(void)
{
    return static_cast<unsigned long>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - micros_start_time).count());
}

unsigned long micros(void)
{
    return static_cast<unsigned long>(micros64());
}

void shiftOut(uint8_t dataPin, uint8_t clockPin, uint8_t bitOrder, uint8_t val)
{
}

uint8_t shiftIn(uint8_t dataPin, uint8_t clockPin, uint8_t bitOrder)
{
    return uint8_t();
}

void attachInterrupt(uint8_t, void(*)(void), int mode)
{
}

void detachInterrupt(uint8_t) {

}

static bool panic_message_box_active = false;

static bool winsock_initialized = false;

void init_winsock() {
    if (!winsock_initialized) {
        int iResult;
        WSADATA wsaData;
        iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (iResult != NO_ERROR) {
            wprintf(L"WSAStartup failed with error: %d\n", iResult);
            exit(-1);
        }
        winsock_initialized = true;
    }
}


void Dir::__test() {
    Dir dir = SPIFFS.openDir("./");
    while (dir.next()) {
        if (dir.isFile()) {
            printf("is_file: Dir::fileName() %s Dir::fileSize() %d\n", dir.fileName().c_str(), dir.fileSize());
            File file = dir.openFile(fs::FileOpenMode::read);
            printf("File::size %d\n", file.size());
            file.close();
        }
        else if (dir.isDirectory()) {
            printf("is_dir: Dir::fileName() %s\n", dir.fileName().c_str());
        }
    }
}

File Dir::openFile(const char *mode) {
    return SPIFFS.open(fileName(), mode);
}


EspClass ESP;

void EspClass::rtcMemDump() {
    if (_rtcMemory) {
        DumpBinary dump(Serial);
        dump.dump(_rtcMemory + rtcMemoryReserved, rtcMemoryUser);
    }
}

void EspClass::rtcClear() {
    if (_rtcMemory) {
        memset(_rtcMemory, 0, rtcMemorySize);
        _writeRtcMemory();
    }
}

void delay(unsigned long time_ms)
{
    ets_timer_delay(time_ms);
}

void delayMicroseconds(unsigned int us)
{
    ets_timer_delay_us(us);
}

unsigned long pulseIn(uint8_t pin, uint8_t state, unsigned long timeout)
{
    return 0;
}

unsigned long pulseInLong(uint8_t pin, uint8_t state, unsigned long timeout)
{
    return 0;
}


uint16_t __builtin_bswap16(uint16_t word) {
    return ((word >> 8) & 0xff) | ((word << 8) && 0xffff);
}

#endif
