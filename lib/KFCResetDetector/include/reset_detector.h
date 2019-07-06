
/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>

#if defined(ESP8266)
#include <osapi.h>
#include <user_interface.h>
#endif

#define RESET_DETECTOR_TIMEOUT              5000

#define RESET_DETECTOR_MAGIC_WORD           0x00234205

#define RESET_DETECTOR_USE_RTC_MEMORY       1
#define RESET_DETECTOR_RTC_MEM_SIZE         512
#define RESET_DETECTOR_RTC_MEM_BLK_SIZE     4
#define RESET_DETECTOR_RTC_MEM_ADDRESS      ((RESET_DETECTOR_RTC_MEM_SIZE - sizeof(ResetDetector::ResetDetectorData_t)) / RESET_DETECTOR_RTC_MEM_BLK_SIZE)

class ResetDetector {
public:
    struct ResetDetectorData_t {
        uint32_t magic_word;
        uint16_t reset_counter: 8;
        uint16_t safe_mode: 4;
        uint16_t crc;
    };

    ResetDetector();

    uint8_t getResetCounter() const;
    uint8_t getSafeMode() const;
    void setSafeMode(uint8_t safeMode);

    bool hasCrashDetected() const;
    bool hasResetDetected() const;
    bool hasRebootDetected() const;
    const String getResetReason() const;
    const String getResetInfo() const;

    static void _timerCallback(void *arg);
    void clearCounter();

private:
    void _init();
    void _writeData();

private:
    uint8_t _resetCounter;
    uint8_t _safeMode;
    uint8_t _resetReason;
    os_timer_t *_timer;
};

extern ResetDetector resetDetector;
