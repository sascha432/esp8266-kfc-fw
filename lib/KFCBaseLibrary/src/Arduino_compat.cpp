/**
  Author: sascha_lammers@gmx.de
*/

#include "Arduino_compat.h"

#if _WIN32 || _WIN64

#include <algorithm>
#include <BufferStream.h>
#include <DumpBinary.h>

#include "EEPROM.h"

EEPROMClass EEPROM;
_SPIFFS SPIFFS;

void throwException(PGM_P message) {
    printf("EXCEPTION: %s\n", (const char *)message);
    exit(-1);
}

static bool init_millis();

static ULONG64 millis_start_time = 0;
static bool millis_initialized = init_millis();

static bool init_millis() {
    millis_start_time = millis();
    return true;
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

unsigned long millis(void) {
    SYSTEMTIME sysTime;
    GetSystemTime(&sysTime);
    time_t now = time(nullptr);
    return (unsigned long)((sysTime.wMilliseconds + now * 1000) - millis_start_time);
}

unsigned long micros(void)
{
    return millis() * 1000UL;
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


void panic() {
    printf("panic() called\n");
    abort();
}

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
            File file = dir.openFile("r");
            printf("File::size %d\n", file.size());
            file.close();
        } else if (dir.isDirectory()) {
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

ESPTimerThread timerThread;
static bool timerThreadExitFuncRegistered = false;


void ESPTimerThread::begin() {
    if (!_handle) {
    }
}

void ESPTimerThread::end() {
    if (_handle) {
        _handle = nullptr;
    }
}

ESPTimerThread::TimerVector &ESPTimerThread::getVector() {
    return _timers;
}

void ESPTimerThread::run() {
    unsigned long time = millis();
    for (auto timer : _timers) {
        if (timer->_nextCall && time >= timer->_nextCall) {
            timer->_nextCall = timer->_repeat ? time + timer->_interval : 0;
            timer->_func(timer->_arg);
        }
    }
    _removeEndedTimers();
}

void ESPTimerThread::_removeEndedTimers() {
    _timers.erase(std::remove_if(_timers.begin(), _timers.end(), [](const os_timer_t *_timer) {
        return _timer->_nextCall == 0;
    }), _timers.end());
}


void delay(unsigned long time_ms) {
    unsigned long _endSleep = millis() + time_ms;
    timerThread.run();
    while (millis() < _endSleep) {
        Sleep(1);
        timerThread.run();
    }
}

void delayMicroseconds(unsigned int us)
{
}

unsigned long pulseIn(uint8_t pin, uint8_t state, unsigned long timeout)
{
    return 0;
}

unsigned long pulseInLong(uint8_t pin, uint8_t state, unsigned long timeout)
{
    return 0;
}

void yield() {
    delay(1);
}

void os_timer_setfn(os_timer_t * timer, os_timer_func_t func, void * arg) {
    timer->_arg = arg;
    timer->_func = func;
}

void os_timer_arm(os_timer_t * timer, int interval, int repeat) {
    timer->_interval = interval;
    timer->_repeat = repeat;
    timer->_nextCall = millis() + timer->_interval;
    timerThread.getVector().push_back(timer);
}

void os_timer_disarm(os_timer_t * timer) {
    timerThread.getVector().erase(std::remove_if(timerThread.getVector().begin(), timerThread.getVector().end(), [timer](const os_timer_t *_timer) {
        return (timer == _timer);
    }), timerThread.getVector().end());
}



uint16_t __builtin_bswap16(uint16_t word) {
    return ((word >> 8) & 0xff) | ((word << 8) && 0xffff);
}

#endif

