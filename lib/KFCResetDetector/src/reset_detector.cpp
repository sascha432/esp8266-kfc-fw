
/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "reset_detector.h"

ResetDetector resetDetector;

#ifndef DEBUG_RESET_DETECTOR
#define DEBUG_RESET_DETECTOR 0
#endif

#if DEBUG_RESET_DETECTOR
#define DEBUG_PRINT(...) Serial.printf_P(__VA_ARGS__)
#else
#define DEBUG_PRINT(...) ;
#endif

ResetDetector::ResetDetector() {
    _init();
}

void ResetDetector::_init() {
    ResetDetectorData_t data;
    bool isValid = false;
    uint16_t crc = ~0;

#if DEBUG_RESET_DETECTOR
    Serial.begin(115200);
#endif

    memset(&data, 0, sizeof(data));

    if (ESP.rtcUserMemoryRead(RESET_DETECTOR_RTC_MEM_ADDRESS, (uint32_t *)&data, sizeof(data))) {
        if (data.magic_word == RESET_DETECTOR_MAGIC_WORD) {
            crc = crc16_calc((uint8_t *)&data, sizeof(data) - sizeof(data.crc));
            if (crc == data.crc) {
                isValid = true;
            }
        }
    }

    struct rst_info *reset_info = ESP.getResetInfoPtr();
    _resetReason = reset_info->reason;

    if (isValid) {
        data.reset_counter++;
        _safeMode = data.safe_mode;
        _resetCounter = data.reset_counter;
    } else {
        _safeMode = 0;
        _resetCounter = 0;
    }
    DEBUG_PRINT(PSTR("\n\n\nRD: valid %d, magic word %08x, safe mode: %d, reset counter %d, crc: %04x = %04x\n"),
        isValid, data.magic_word, data.safe_mode, data.reset_counter, crc, data.crc);
    DEBUG_PRINT(PSTR("RD: reset reason: %s (%d) / %s, reset info: %s, is crash: %d, is reset: %d, is reboot: %d\n"),
        getResetReason().c_str(), _resetReason, ESP.getResetReason().c_str(), getResetInfo().c_str(), hasCrashDetected(), hasResetDetected(), hasRebootDetected());

    _writeData();


#if defined(ESP8266)
    _timer = new os_timer_t();
    os_timer_setfn(_timer, reinterpret_cast<os_timer_func_t *>(_timerCallback), reinterpret_cast<void *>(this));
    os_timer_arm(_timer, RESET_DETECTOR_TIMEOUT, 0);
#else
    #error Platform not supported
#endif

}

void ResetDetector::_timerCallback(void *arg) {
    ResetDetector *rd = reinterpret_cast<ResetDetector *>(arg);
    rd->clearCounter();
}

void ResetDetector::clearCounter() {
    DEBUG_PRINT(PSTR("RD: set reset counter = 0 (%d)\n"), _resetCounter);
    _resetCounter = 0;
    _writeData();
}

bool ResetDetector::hasCrashDetected() const {
    return (
        _resetReason != REASON_DEFAULT_RST &&
        _resetReason != REASON_EXT_SYS_RST &&
        _resetReason != REASON_SOFT_RESTART &&
        _resetReason != REASON_DEEP_SLEEP_AWAKE &&
        _resetReason != REASON_EXT_SYS_RST);
}

bool ResetDetector::hasResetDetected() const {
    return
        _resetReason == REASON_DEFAULT_RST ||
        _resetReason == REASON_EXT_SYS_RST;
}

bool ResetDetector::hasRebootDetected()  const {
    return (_resetReason == REASON_SOFT_RESTART);
}

const String ResetDetector::getResetReason() const {
    switch(_resetReason) {
        case REASON_DEFAULT_RST:
            return F("Normal startup");
        case REASON_WDT_RST:
            return F("WDT reset");
        case REASON_EXCEPTION_RST:
            return F("Exception reset");
        case REASON_SOFT_WDT_RST:
            return F("Soft WDT reset");
        case REASON_SOFT_RESTART:
            return F("System restart");
        case REASON_DEEP_SLEEP_AWAKE:
            return F("Wake up from deep-sleep");
        case REASON_EXT_SYS_RST:
            return F("External system reset");
    }
    return F("Unknown");
}

const String ResetDetector::getResetInfo() const {
    return ESP.getResetInfo();
}

uint8_t ResetDetector::getResetCounter() const {
    return _resetCounter;
}

uint8_t ResetDetector::getSafeMode() const {
    return _safeMode;
}

void ResetDetector::setSafeMode(uint8_t safeMode) {
    _safeMode = safeMode;
    _writeData();
}

void ResetDetector::_writeData() {

    ResetDetectorData_t data;
    memset((uint8_t *)&data, 0, sizeof(data));

    data.magic_word = RESET_DETECTOR_MAGIC_WORD;
    data.reset_counter = _resetCounter;
    data.safe_mode = _safeMode;
    data.crc = crc16_calc((uint8_t *)&data, sizeof(data) - sizeof(data.crc));

    if (!ESP.rtcUserMemoryWrite(RESET_DETECTOR_RTC_MEM_ADDRESS, (uint32_t *)&data, sizeof(data))) {
        DEBUG_PRINT(PSTR("RD: Failed to write to RTC MEM\n"));
    }

}
