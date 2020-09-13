/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#ifndef DEBUG_RESET_DETECTOR
#define DEBUG_RESET_DETECTOR        0
#endif

#include <Arduino_compat.h>

#if defined(ESP8266)
#include <osapi.h>
#include <user_interface.h>
#elif defined(ESP32)
#elif _WIN32 || _WIN64
#else
#error Platform not supported
#endif

#include <push_pack.h>

#define RESET_DETECTOR_TIMEOUT              5000
#ifndef USE_ESP_GET_RESET_REASON
#if defined(ESP8266)
#define USE_ESP_GET_RESET_REASON            1
#else
#define USE_ESP_GET_RESET_REASON            0
#endif
#endif

#if HAVE_KFC_PLUGINS

#include <debug_helper.h>
#include <plugins.h>
#include <PluginComponent.h>
#include <RTCMemoryManager.h>

#else

#include <crc16.h>

#define RESET_DETECTOR_MAGIC_WORD           0x00234205
#define RESET_DETECTOR_USE_RTC_MEMORY       1
#define RESET_DETECTOR_RTC_MEM_SIZE         512
#define RESET_DETECTOR_RTC_MEM_BLK_SIZE     4
#define RESET_DETECTOR_RTC_DATA_BLK_SIZE    (((sizeof(ResetDetector::ResetDetectorData_t) & (RESET_DETECTOR_RTC_MEM_BLK_SIZE - 1)) ? ((sizeof(ResetDetector::ResetDetectorData_t) + 4) & ~(RESET_DETECTOR_RTC_MEM_BLK_SIZE - 1)) : sizeof(ResetDetector::ResetDetectorData_t)) / RESET_DETECTOR_RTC_MEM_BLK_SIZE)
#define RESET_DETECTOR_RTC_DATA_SIZE        (RESET_DETECTOR_RTC_DATA_BLK_SIZE * RESET_DETECTOR_RTC_MEM_BLK_SIZE)
#define RESET_DETECTOR_RTC_MEM_ADDRESS      ((RESET_DETECTOR_RTC_MEM_SIZE / RESET_DETECTOR_RTC_MEM_BLK_SIZE) - RESET_DETECTOR_RTC_DATA_BLK_SIZE)


#endif

class ResetDetector {
public:
#if HAVE_KFC_PLUGINS
    typedef struct __attribute__packed__ {
        uint8_t reset_counter;
        uint8_t safe_mode;
    } ResetDetectorData_t;
private:
    ResetDetectorData_t data;
public:
#else
    typedef struct __attribute__packed__ {
        uint32_t magic_word;
        uint8_t reset_counter;
        uint8_t safe_mode;
        uint16_t crc;
    } ResetDetectorData_t;
#endif

    ResetDetector();

    // returns the number of resets. starts with 1 after the first reset and increases with
    // each reset that occurs before the device is running longer than RESET_DETECTOR_TIMEOUT
    // once the timeout has occured, it returns 0
    uint8_t getResetCounter() const;
    // returns the same value as getResetCounter() but does not reset to 0 after the timeout
    uint8_t getInitialResetCounter() const;
    // returns the value that was stored in the safe mode field
    uint8_t getSafeMode() const;
    // store value in safe mode field
    void setSafeMode(uint8_t safeMode);
    // set counter to 0
    void clearCounter();
#if DEBUG
    void __setResetCounter(uint8_t counter);
#endif
    void setSafeModeAndClearCounter(uint8_t safeMode);

    bool hasCrashDetected() const;
    bool hasResetDetected() const;
    bool hasRebootDetected() const;
    bool hasWakeUpDetected() const;
    const String getResetReason() const;
    const String getResetInfo() const;

    ETSTimer *getTimer();
    void armTimer();
    void disarmTimer();
    static void _timerCallback(void *arg);

    void _init();
private:
    void _writeData();

private:
    uint8_t _resetCounter;
    uint8_t _initialResetCounter;
    uint8_t _safeMode;
    uint8_t _resetReason;
    ETSTimer _timer;
};

extern ResetDetector resetDetector;

#if HAVE_KFC_PLUGINS
class ResetDetectorPlugin : public PluginComponent {
public:
    ResetDetectorPlugin();

#if DEBUG_RESET_DETECTOR
    virtual void setup(SetupModeType mode) override;
#endif

#if AT_MODE_SUPPORTED
    void atModeHelpGenerator() override;
    bool atModeHandler(AtModeArgs &args) override;
#endif

#if ENABLE_DEEP_SLEEP
public:
    static uint32_t _deepSleepWifiTime;
#endif
};

#endif

#include <pop_pack.h>
