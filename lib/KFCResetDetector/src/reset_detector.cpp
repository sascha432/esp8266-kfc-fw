/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "reset_detector.h"
#include <LoopFunctions.h>
#include <PluginComponent.h>

#undef __LDBG_printf
#if DEBUG && DEBUG_RESET_DETECTOR
#define __LDBG_printf(fmt, ...) ::printf_P(PSTR("RD%04u: " fmt "\n"), micros() / 1000, ##__VA_ARGS__)
#else
#define __LDBG_printf(...)
#endif

#if defined(ESP8266)
extern "C" {
    #include "user_interface.h"
}
#endif

ResetDetectorUnitialized resetDetector __attribute__((section(".noinit")));;

#if !ENABLE_DEEP_SLEEP
extern "C" void preinit(void)
{
    resetDetector.init();
    resetDetector.begin();
}
#endif


void ResetDetector::end()
{
    if (_uart) {
#if DEBUG && DEBUG_RESET_DETECTOR
        ::printf_P(PSTR("\r\n"));
#endif
        uart_flush(_uart);
        uart_uninit(_uart);
        _uart = nullptr;
    }
}

void ResetDetector::begin()
{
    if (_uart) {
        __LDBG_printf("begin() called multiple times without end()");
        end();
    }
    _uart = uart_init(UART0, 115200, (int) SERIAL_8N1, (int) SERIAL_FULL, 1, 64, false);

#if DEBUG && DEBUG_RESET_DETECTOR
    ::printf_P(PSTR("\r\n"));
#endif
    __LDBG_printf("init reset detector");

    _readData();
    ++_data;

#if defined(ESP32)
    _data.pushReason(esp_reset_reason());
#elif defined(ESP8266)
    struct rst_info &resetInfo = *system_get_rst_info();
    _data.pushReason(resetInfo.reason);
    __LDBG_printf("depc=%08x epc1=%08x epc2=%08x epc3=%08x exccause=%08x excvaddr=%08x reason=%u", resetInfo.depc, resetInfo.epc1, resetInfo.epc2, resetInfo.epc3, resetInfo.exccause, resetInfo.excvaddr, resetInfo.reason);
#endif
    __LDBG_printf("valid=%d safe_mode=%u counter=%u new=%u", !!_storedData, _storedData.isSafeMode(), _storedData.getResetCounter(), _data.getResetCounter());
    __LDBG_printf("info=%s crash=%u reset=%u reboot=%u wakeup=%u",
        getResetInfo().c_str(), hasCrashDetected(), hasResetDetected(), hasRebootDetected(), hasWakeUpDetected()
    );
    __LDBG_printf("reason history");
    for(auto reason: _storedData) {
        __LDBG_printf("%u - %s", reason, getResetReason(reason));
    }
    __LDBG_printf("%u - %s (recent)", _data.getReason(), getResetReason());

    _writeData();
    armTimer();
}

ETSTimer *ResetDetector::getTimer()
{
    return &_timer;
}

void ResetDetector::armTimer()
{
    ets_timer_disarm(&_timer);
    ets_timer_setfn(&_timer, reinterpret_cast<ETSTimerFunc *>(_timerCallback), reinterpret_cast<void *>(this));
    ets_timer_arm_new(&_timer, RESET_DETECTOR_TIMEOUT, false, true);
}

const __FlashStringHelper *ResetDetector::getResetReason(uint8_t reason)
{
#if USE_ESP_GET_RESET_REASON
    // copy of ESP.getResetInfo(), returning a PROGMEM string
    const __FlashStringHelper* buff;
    switch(reason) {
        // normal startup by power on
        case REASON_DEFAULT_RST:      buff = F("Power On"); break;
        // hardware watch dog reset
        case REASON_WDT_RST:          buff = F("Hardware Watchdog"); break;
        // exception reset, GPIO status won’t change
        case REASON_EXCEPTION_RST:    buff = F("Exception"); break;
        // software watch dog reset, GPIO status won’t change
        case REASON_SOFT_WDT_RST:     buff = F("Software Watchdog"); break;
        // software restart ,system_restart , GPIO status won’t change
        case REASON_SOFT_RESTART:     buff = F("Software/System restart"); break;
        // wake up from deep-sleep
        case REASON_DEEP_SLEEP_AWAKE: buff = F("Deep-Sleep Wake"); break;
        // // external system reset
        case REASON_EXT_SYS_RST:      buff = F("External System"); break;
        default:                      buff = F("Unknown"); break;
    }
    return buff;
#elif defined(ESP32)
    switch(reason) {
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
    switch(reason) {
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

const String ResetDetector::getResetInfo() const
{
#if defined(ESP32)
    return getResetReason();
#else
    return ESP.getResetInfo();
#endif
}

void ResetDetector::clearCounter()
{
    __LDBG_printf("set counter=0 (%u) safe_mode=%u", _data.getResetCounter(), _data.isSafeMode());
    _data = 0;
    _writeData();
}

#if DEBUG
void ResetDetector::__setResetCounter(Counter_t counter)
{
    _storedData = counter;
    _data = counter;
    _writeData();
}
#endif

void ResetDetector::setSafeModeAndClearCounter(bool safeMode)
{
    __LDBG_printf("set counter=0 (%u) set safe_mode=%u", _data.getResetCounter(), data.isSafeMode());
    _data = Data(0, safeMode);
    _writeData();
}

void ResetDetector::_readData()
{
    if (RTCMemoryManager::read(PluginComponent::RTCMemoryId::RESET_DETECTOR, &_storedData, sizeof(_storedData))) {
        _storedData.setValid(true);
        _data = _storedData;
    }
    else {
        _storedData = Data();
        _data = Data(0, false);
    }
}

void ResetDetector::_writeData()
{
    if (!_data) {
        RTCMemoryManager::remove(PluginComponent::RTCMemoryId::RESET_DETECTOR);
    }
    else {
        RTCMemoryManager::write(PluginComponent::RTCMemoryId::RESET_DETECTOR, &_data, sizeof(_data));
    }
}

#include "logger.h"

static ResetDetectorPlugin plugin;
extern void PluginComponentInitRegisterEx();

PROGMEM_DEFINE_PLUGIN_OPTIONS(
    ResetDetectorPlugin,
    "rd",               // name
    "rd",               // friendly name
    "",                 // web_templates
    "",                 // config_forms
    "",                 // reconfigure_dependencies
    PluginComponent::PriorityType::RESET_DETECTOR,
    PluginComponent::RTCMemoryId::RESET_DETECTOR,
    static_cast<uint8_t>(PluginComponent::MenuType::NONE),
    true,               // allow_safe_mode
    true,               // setup_after_deep_sleep
    false,              // has_get_status
    false,              // has_config_forms
    false,              // has_web_ui
    false,              // has_web_templates
    true,               // has_at_mode
    0                   // __reserved
);

ResetDetectorPlugin::ResetDetectorPlugin() : PluginComponent(PROGMEM_GET_PLUGIN_OPTIONS(ResetDetectorPlugin))
{
    PluginComponentInitRegisterEx();
    REGISTER_PLUGIN(this, "ResetDetectorPlugin");
}

#if AT_MODE_SUPPORTED

#include "at_mode.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPP(RD, "RD", "Reset detector clear counter", "Display information");

void ResetDetectorPlugin::atModeHelpGenerator()
{
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(RD), getName_P());
}

bool ResetDetectorPlugin::atModeHandler(AtModeArgs &args)
{
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(RD))) {
        if (args.isQueryMode()) {
            args.getStream().printf_P(PSTR("safe mode: %u\nreset counter: %u\ninitial reset counter: %u\ncrash: %u\nreboot: %u\nreset: %u\nwake up: %u\nreset reason: %s\n"),
                resetDetector.getSafeMode(),
                resetDetector.getResetCounter(),
                resetDetector.getInitialResetCounter(),
                resetDetector.hasCrashDetected(),
                resetDetector.hasRebootDetected(),
                resetDetector.hasResetDetected(),
                resetDetector.hasWakeUpDetected(),
                resetDetector.getResetReason()
            );
        }
        else {
            resetDetector.clearCounter();
        }
        return true;
    }
    return false;
}

#endif

#if !RESET_DETECTOR_INCLUDE_HPP_INLINE
#include "reset_detector.hpp"
#endif
