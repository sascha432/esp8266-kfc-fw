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

class StartupTimings {
public:

    void dump(Print &output);
    void log();

    void setWiFiGotIP(uint32_t millis) {
        if (_wifiGotIP == 0) {
            _wifiGotIP = millis;
        }
    }
    uint32_t getWiFiGotIP() const {
        return _wifiGotIP;
    }


#if IOT_REMOTE_CONTROL

public:
    StartupTimings() :
        _preInit(0),
        _preSetup(0),
        _setup(0),
        _loop(0),
        _wifiConnected(0),
        _wifiGotIP(0),
        _ntp(0),
        _mqtt(0),
        _deepSleep(0)
    {}

    void preInit(uint32_t millis) {
        _preInit = millis;
    }

    void preSetup(uint32_t millis) {
        _preSetup = millis;
    }

    void setSetupFunc(uint32_t millis) {
        _setup = millis;
    }

    void setLoopFunc(uint32_t millis) {
        _loop = millis;
    }

    void setWiFiConnected(uint32_t millis) {
        if (_wifiConnected == 0) {
            _wifiConnected = millis;
        }
    }

    void setNtp(uint32_t millis) {
        if (_ntp == 0) {
            _ntp = millis;
        }
    }

    void setMqtt(uint32_t millis) {
        if (_mqtt == 0) {
            _mqtt = millis;
        }
    }

    void setDeepSleep(uint32_t millis) {
        _deepSleep = millis;
    }

private:
    uint32_t _preInit;
    uint32_t _preSetup;
    uint32_t _setup;
    uint32_t _loop;
    uint32_t _wifiConnected;
    uint32_t _wifiGotIP;
    uint32_t _ntp;
    uint32_t _mqtt;
    uint32_t _deepSleep;

#else

public:
    StartupTimings() : _wifiGotIP(0) {}

    void setSetupFunc(uint32_t millis) {}
    void setLoopFunc(uint32_t millis) {}
    void setWiFiConnected(uint32_t millis) {}
    void setNtp(uint32_t millis) {}
    void setMqtt(uint32_t millis) {}
    void setDeepSleep(uint32_t millis) {}

private:
    uint32_t _wifiGotIP;

#endif
};

extern StartupTimings _startupTimings;

class ResetDetectorPlugin : public PluginComponent {
public:
    ResetDetectorPlugin();

#if DEBUG_RESET_DETECTOR
    virtual void setup(SetupModeType mode, const PluginComponents::DependenciesPtr &dependencies) override;
#endif

#if AT_MODE_SUPPORTED
    void atModeHelpGenerator() override;
    bool atModeHandler(AtModeArgs &args) override;
#endif
};

inline uint8_t ResetDetector::getResetCounter() const
{
    return _resetCounter;
}

inline uint8_t ResetDetector::getInitialResetCounter() const
{
    return _initialResetCounter;
}

inline uint8_t ResetDetector::getSafeMode() const
{
    return _safeMode;
}

inline void ResetDetector::setSafeMode(uint8_t safeMode)
{
    _safeMode = safeMode;
    _writeData();
}

inline bool ResetDetector::hasCrashDetected() const
{
#if defined(ESP32)
    return (
        _resetReason == ESP_RST_WDT ||
        _resetReason == ESP_RST_PANIC ||
        _resetReason == ESP_RST_INT_WDT ||
        _resetReason == ESP_RST_TASK_WDT ||
        _resetReason == ESP_RST_WDT
    );
#elif defined(ESP8266)
    return (
        _resetReason != REASON_DEFAULT_RST &&
        _resetReason != REASON_EXT_SYS_RST &&
        _resetReason != REASON_SOFT_RESTART &&
        _resetReason != REASON_DEEP_SLEEP_AWAKE &&
        _resetReason != REASON_EXT_SYS_RST
    );
#endif
}

inline bool ResetDetector::hasResetDetected() const
{
#if defined(ESP32)
    return (_resetReason == ESP_RST_UNKNOWN || _resetReason == ESP_RST_EXT || _resetReason == ESP_RST_BROWNOUT || _resetReason == ESP_RST_SDIO);
#elif defined(ESP8266)
    return (_resetReason == REASON_DEFAULT_RST || _resetReason == REASON_EXT_SYS_RST);
#endif
}

inline bool ResetDetector::hasRebootDetected() const
{
#if defined(ESP32)
    return (_resetReason == ESP_RST_SW);
#elif defined(ESP8266)
    return (_resetReason == REASON_SOFT_RESTART);
#endif
}

inline bool ResetDetector::hasWakeUpDetected() const
{
#if defined(ESP32)
    return (_resetReason == ESP_RST_DEEPSLEEP);
#elif defined(ESP8266)
    return (_resetReason == REASON_DEEP_SLEEP_AWAKE);
#endif
}

#endif

#include <pop_pack.h>
