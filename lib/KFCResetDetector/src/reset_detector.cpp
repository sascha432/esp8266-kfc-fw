
/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "reset_detector.h"

#if DEBUG_RESET_DETECTOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

ResetDetector resetDetector;

extern Stream &MySerial;
extern Stream &DebugSerial;


ResetDetector::ResetDetector() {
#if !DEBUG_RESET_DETECTOR
    // for debugging call _init() in setup() after Serial.begin() and DEBUG_HELPER_INIT()
    _init();
#endif
}

void ResetDetector::_init() {
#if DEBUG_RESET_DETECTOR
    Serial.begin(KFC_SERIAL_RATE);
    Serial.println(F("ResetDetector::_init()"));
#endif
    _debug_println(F("ResetDetector::_init()"));

     _timer = nullptr;
    ResetDetectorData_t data;
    auto isValid = false;

#if HAVE_KFC_PLUGINS

    if (RTCMemoryManager::read(RESET_DETECTOR_RTC_MEM_ID, &data, sizeof(data))) {
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

#if defined(ESP32)
    _resetReason = esp_reset_reason();
#elif defined(ESP8266)
    struct rst_info *reset_info = ESP.getResetInfoPtr();
    _resetReason = reset_info->reason;
#endif

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
    _debug_printf_P(PSTR("\n\n\nRD: valid %d, safe mode=%d, reset counter=%d\n"), isValid, data.safe_mode, data.reset_counter);
    _debug_printf_P(PSTR("RD: reset reason: %s (%d), reset info: %s, crash=%d, reset=%d, reboot=%d, wakeup=%d\n"), getResetReason().c_str(), _resetReason, getResetInfo().c_str(), hasCrashDetected(), hasResetDetected(), hasRebootDetected(), hasWakeUpDetected());
#else
    _debug_printf_P(PSTR("\n\n\nRD: valid %d, magic word %08x, safe mode=%d, reset counter=%d, crc %04x=%04x\n"), isValid, data.magic_word, data.safe_mode, data.reset_counter, crc, data.crc);
    _debug_printf_P(PSTR("RD: reset reason: %s (%d), reset info: %s, crash=%d, reset=%d, reboot=%d, wakeup=%d\n"), getResetReason().c_str(), _resetReason, getResetInfo().c_str(), hasCrashDetected(), hasResetDetected(), hasRebootDetected(), hasWakeUpDetected());
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
    os_timer_create(_timer, reinterpret_cast<os_timer_func_t_ptr>(_timerCallback), reinterpret_cast<void *>(this));
    os_timer_arm(_timer, RESET_DETECTOR_TIMEOUT, 0);
}

void ResetDetector::disarmTimer() {
    if (_timer) {
        os_timer_disarm(_timer);
        os_timer_delete(_timer);
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

bool ResetDetector::hasResetDetected() const {
#if defined(ESP32)
    return (_resetReason == ESP_RST_UNKNOWN || _resetReason == ESP_RST_EXT || _resetReason == ESP_RST_BROWNOUT || _resetReason == ESP_RST_SDIO);
#elif defined(ESP8266)
    return (_resetReason == REASON_DEFAULT_RST || _resetReason == REASON_EXT_SYS_RST);
#endif
}

bool ResetDetector::hasRebootDetected()  const {
#if defined(ESP32)
    return (_resetReason == ESP_RST_SW);
#elif defined(ESP8266)
    return (_resetReason == REASON_SOFT_RESTART);
#endif
}

bool ResetDetector::hasWakeUpDetected() const {
#if defined(ESP32)
    return (_resetReason == ESP_RST_DEEPSLEEP);
#elif defined(ESP8266)
    return (_resetReason == REASON_DEEP_SLEEP_AWAKE);
#endif
}

const String ResetDetector::getResetReason() const {
#if USE_ESP_GET_RESET_REASON
    return ESP.getResetReason();
#elif defined(ESP32)
    switch(_resetReason) {
        case ESP_RST_POWERON:
            return F("Normal startup");
        case ESP_RST_PANIC:
            return F("Panic reset");
        case ESP_RST_INT_WDT:
            return F("Int. WDT reset");
        case ESP_RST_TASK_WDT:
            return F("Task WDT reset");
        case ESP_RST_WDT:
            return F("WDT reset");
        case ESP_RST_SW:
            return F("System restart");
        case ESP_RST_DEEPSLEEP:
            return F("Wake up from deep-sleep");
        case ESP_RST_EXT:
            return F("External system reset");
        case ESP_RST_BROWNOUT:
            return F("Brownout");
        case ESP_RST_SDIO:
            return F("Reset over SDIO");
    }
    return F("Unknown");
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
#if defined(ESP32)
    return getResetReason();
#else
    return ESP.getResetInfo();
#endif
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

    RTCMemoryManager::write(RESET_DETECTOR_RTC_MEM_ID, (void *)&data, sizeof(data));

#else

    data.magic_word = RESET_DETECTOR_MAGIC_WORD;
    data.crc = crc16_calc((uint8_t *)&data, sizeof(data) - sizeof(data.crc));

    if (!ESP.rtcUserMemoryWrite(RESET_DETECTOR_RTC_MEM_ADDRESS, (uint32_t *)&data, sizeof(data))) {
        _debug_printf_P(PSTR("RD: Failed to write to RTC MEM\n"));
    }

#endif

}

#if HAVE_KFC_PLUGINS

class ResetDetectorPlugin : public PluginComponent {
public:
    ResetDetectorPlugin() {
        REGISTER_PLUGIN(this, "ResetDetectorPlugin");
    }
    virtual PGM_P getName() const {
        return PSTR("rd");
    }
    virtual PluginPriorityEnum_t getSetupPriority() const override {
        return PRIO_RESET_DETECTOR;
    }
    virtual uint8_t getRtcMemoryId() const override {
        return RESET_DETECTOR_RTC_MEM_ID;
    }

#if AT_MODE_SUPPORTED
    virtual bool hasAtMode() const override {
        return true;
    }
    void atModeHelpGenerator() override;
    bool atModeHandler(Stream &serial, const String &command, int8_t argc, char **argv) override;
#endif
};

static ResetDetectorPlugin plugin;

#if AT_MODE_SUPPORTED

#include "at_mode.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPP(RD, "RD", "Reset detector clear counter", "Display information");

void ResetDetectorPlugin::atModeHelpGenerator() {
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(RD));
}

bool ResetDetectorPlugin::atModeHandler(Stream &serial, const String &command, int8_t argc, char **argv) {
    if (constexpr_String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(RD))) {
        if (argc == ATModeQueryCommand) {
            serial.printf_P(PSTR("safe mode: %d\nreset counter: %d\ninitial reset counter: %d\ncrash: %d\nreboot: %d\nreset: %d\nreset reason: %s\n"),
                resetDetector.getSafeMode(),
                resetDetector.getResetCounter(),
                resetDetector.getInitialResetCounter(),
                resetDetector.hasCrashDetected(),
                resetDetector.hasRebootDetected(),
                resetDetector.hasResetDetected(),
                resetDetector.getResetReason().c_str()
            );

        } else {
            resetDetector.clearCounter();
        }
    }
    return false;
}

#endif

#endif
