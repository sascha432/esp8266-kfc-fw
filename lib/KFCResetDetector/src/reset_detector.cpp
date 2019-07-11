
/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "reset_detector.h"

ResetDetector resetDetector;

#ifndef DEBUG_RESET_DETECTOR
#define DEBUG_RESET_DETECTOR        1
#endif
#if DEBUG_RESET_DETECTOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

ResetDetector::ResetDetector() {
#if DEBUG_RESET_DETECTOR
    Serial.begin(115200);
#endif
    // _init();
}

void ResetDetector::_init() {
     _timer = nullptr;
#if HAVE_KFC_PLUGINS
    register_all_plugins();
#endif

    ResetDetectorData_t data;
    auto isValid = false;

#if HAVE_KFC_PLUGINS

    if (plugin_read_rtc_memory(RESET_DETECTOR_RTC_MEM_ID, &data, sizeof(data))) {
        isValid = true;
    }

#else
    uint16_t crc = ~0;
    memset(&data, 0, sizeof(data));

    if (ESP.rtcUserMemoryRead(RESET_DETECTOR_RTC_MEM_ADDRESS, (uint32_t *)&data, sizeof(data))) {
        if (data.magic_word == RESET_DETECTOR_MAGIC_WORD) {
            crc = crc16_calc((uint8_t *)&data, sizeof(data) - sizeof(data.crc));
            if (crc == data.crc) {
                isValid = true;
            }
        }
    }
#endif

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
    _initialResetCounter = _resetCounter;


#if HAVE_KFC_PLUGINS
    _debug_printf_P(PSTR("\n\n\nRD: valid %d, safe mode: %d, reset counter %d\n"), isValid, data.safe_mode, data.reset_counter);
    _debug_printf_P(PSTR("RD: reset reason: %s (%d), reset info: %s, is crash: %d, is reset: %d, is reboot: %d\n"), getResetReason().c_str(), _resetReason, getResetInfo().c_str(), hasCrashDetected(), hasResetDetected(), hasRebootDetected());
#else
    _debug_printf_P(PSTR("\n\n\nRD: valid %d, magic word %08x, safe mode: %d, reset counter %d, crc: %04x = %04x\n"), isValid, data.magic_word, data.safe_mode, data.reset_counter, crc, data.crc);
    _debug_printf_P(PSTR("RD: reset reason: %s (%d), reset info: %s, is crash: %d, is reset: %d, is reboot: %d\n"), getResetReason().c_str(), _resetReason, getResetInfo().c_str(), hasCrashDetected(), hasResetDetected(), hasRebootDetected());
#endif

    _writeData();
    armTimer();
}

os_timer_t *ResetDetector::getTimer() {
    return _timer;
}

void ResetDetector::armTimer() {
    if (_timer) {
        disarmTimer();
    }
    _timer = new os_timer_t();
    os_timer_setfn(_timer, reinterpret_cast<os_timer_func_t *>(_timerCallback), reinterpret_cast<void *>(this));
    os_timer_arm(_timer, RESET_DETECTOR_TIMEOUT, 0);
}

void ResetDetector::disarmTimer() {
    if (_timer) {
        os_timer_disarm(_timer);
        delete _timer;
        _timer = nullptr;
    }
}


void ResetDetector::_timerCallback(void *arg) {
    auto &rd = *reinterpret_cast<ResetDetector *>(arg);
    rd.clearCounter();
    rd.disarmTimer();
}

void ResetDetector::clearCounter() {
    _debug_printf_P(PSTR("RD: set reset counter = 0 (%d)\n"), _resetCounter);
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
    return (_resetReason == REASON_DEFAULT_RST || _resetReason == REASON_EXT_SYS_RST);
}

bool ResetDetector::hasRebootDetected()  const {
    return (_resetReason == REASON_SOFT_RESTART);
}

bool ResetDetector::hasWakeUpDetected() const {
    return (_resetReason == REASON_DEEP_SLEEP_AWAKE);
}

const String ResetDetector::getResetReason() const {
#if USE_ESP_GET_RESET_REASON
    return ESP.getResetReason();
#else
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
#endif
}

const String ResetDetector::getResetInfo() const {
    return ESP.getResetInfo();
}

uint8_t ResetDetector::getResetCounter() const {
    return _resetCounter;
}

uint8_t ResetDetector::getInitialResetCounter() const {
    return _initialResetCounter;
}

uint8_t ResetDetector::getSafeMode() const {
    return _safeMode;
}

void ResetDetector::setSafeMode(uint8_t safeMode) {
    _safeMode = safeMode;
    _writeData();
}

#if DEBUG
void ResetDetector::__setResetCounter(uint8_t counter) {
    _initialResetCounter = _resetCounter = counter;
    _writeData();
}
#endif

void ResetDetector::_writeData() {

    ResetDetectorData_t data;
    memset((uint8_t *)&data, 0, sizeof(data));

    data.reset_counter = _resetCounter;
    data.safe_mode = _safeMode;

#if HAVE_KFC_PLUGINS

    plugin_write_rtc_memory(RESET_DETECTOR_RTC_MEM_ID, (void *)&data, sizeof(data));

#else

    data.magic_word = RESET_DETECTOR_MAGIC_WORD;
    data.crc = crc16_calc((uint8_t *)&data, sizeof(data) - sizeof(data.crc));

    if (!ESP.rtcUserMemoryWrite(RESET_DETECTOR_RTC_MEM_ADDRESS, (uint32_t *)&data, sizeof(data))) {
        _debug_printf_P(PSTR("RD: Failed to write to RTC MEM\n"));
    }

#endif

}

#if HAVE_KFC_PLUGINS

bool reset_detector_command_handler(Stream &serial, const String &command, int8_t argc, char **argv) {

    if (command.length() == 0) {
        serial.print(F(
            " AT+RD?\n"
            "    Display information\n"
            " AT+RDCLEAR\n"
            "    Reset detector clear counter\n"
        ));
    } else if (command.equalsIgnoreCase(F("RDCLEAR"))) {
        if (argc == -1) {
            serial.printf_P(PSTR("safe mode: %d\nreset counter: %d\ninitial reset counter: %d\ncrash: %d\nreboot: %d\nreset: %d\nreset reason: %s / %s\n"),
                resetDetector.getSafeMode(),
                resetDetector.getResetCounter(),
                resetDetector.getInitialResetCounter(),
                resetDetector.hasCrashDetected(),
                resetDetector.hasRebootDetected(),
                resetDetector.hasResetDetected(),
                resetDetector.getResetReason().c_str(),
                ESP.getResetReason().c_str()
            );

        } else {
            resetDetector.clearCounter();
        }
    }
    return false;
}

PROGMEM_PLUGIN_CONFIG_DEF(
    /* pluginName               */ rd,
    /* setupPriority            */ 0,
    /* allowSafeMode            */ false,
    /* autoSetupWakeUp          */ false,
    /* rtcMemoryId              */ RESET_DETECTOR_RTC_MEM_ID,
    /* setupPlugin              */ nullptr,
    /* statusTemplate           */ nullptr,
    /* configureForm            */ nullptr,
    /* reconfigurePlugin        */ nullptr,
    /* prepareDeepSleep         */ nullptr,
    /* atModeCommandHandler     */ reset_detector_command_handler
);

#endif
