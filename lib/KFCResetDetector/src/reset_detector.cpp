/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "reset_detector.h"
#include <LoopFunctions.h>
#include <PluginComponent.h>
#include <plugins_menu.h>
#include <PrintHtmlEntitiesString.h>
#include "save_crash.h"
#if IOT_SWITCH
#include "../src/plugins/switch/switch_def.h"
#endif

#undef __LDBG_printf
#if DEBUG_RESET_DETECTOR
#    if ESP32
#        define __LDBG_printf(fmt, ...) log_printf("RD%04u (line %u): " fmt "\r\n", micros() / 1000, __LINE__, ##__VA_ARGS__)
#    else
#        define __LDBG_printf(fmt, ...) ::printf_P(PSTR("RD%04u (line %u): " fmt "\r\n"), micros() / 1000, __LINE__, ##__VA_ARGS__)
#    endif
#else
#    define __LDBG_printf(...)
#endif

using ResetDetectorUninitialized = stdex::UninitializedClass<ResetDetector>;
static ResetDetectorUninitialized resetDetectorNoInit __attribute__((section(".noinit")));
ResetDetector &resetDetector = resetDetectorNoInit._object;

void resetDetectorNoInit_init()
{
    resetDetectorNoInit.init();
    __DBG_printf("resetDetectorNoInit_init()");
}

void __preinit(void)
{
    resetDetectorNoInit.init();
    componentRegisterNoInit.init();
    resetDetector.begin();
}

extern "C" {

#if ESP8266

    #include <user_interface.h>

    #if !ENABLE_DEEP_SLEEP

        void preinit(void)
        {
            __preinit();
        }

    #endif

#endif

}

void ResetDetector::end()
{
    #if ESP8266 && DEBUG_RESET_DETECTOR
        __LDBG_printf("rd::end(), _uart=%p", _uart);
        if (_uart) {
            __LDBG_printf("\r\n");
            uart_flush(_uart);
            uart_uninit(_uart);
            _uart = nullptr;
        }
    #endif
}

void ResetDetector::begin(HardwareSerial *serial, int baud)
{
    #if ESP8266
        #if DEBUG_RESET_DETECTOR
            __LDBG_printf("rd::begin(), _uart=%p, serial=%p", _uart, serial, baud);

            if (_uart) {
                __LDBG_printf("\r\n");
                uart_flush(_uart);
                uart_uninit(_uart);
                _uart = nullptr;
                __LDBG_printf("_uart=%p", _uart);
            }
            __LDBG_printf("serial=%p begin=%u", serial, baud);
        #endif
        #if IOT_LED_MATRIX_STANDBY_PIN == 3
            serial->begin(baud, SERIAL_8N1, SERIAL_TX_ONLY, 1);
        #else
            serial->begin(baud, SERIAL_8N1);
        #endif
    #elif ESP32
        serial->begin(baud);
    #endif
}

void ResetDetector::begin()
{
    #if ESP8266
        #if DEBUG_RESET_DETECTOR
            auto oldUart = _uart;
            __LDBG_printf("rd::begin(), _uart=%p", _uart);
            if (_uart) {
                __LDBG_printf("begin() called multiple times without end()");
                end();
            }
            _uart = uart_init(UART0, 115200, (int) SERIAL_8N1, (int) SERIAL_FULL, 1, 64, false);
            __LDBG_printf("rd::begin() has been called, old_uart=%p _uart=%p", oldUart, _uart);
        #endif

        __LDBG_printf("init reset detector");

        _readData();
        ++_data;

        struct rst_info &resetInfo = *system_get_rst_info();
        _data.pushReason(resetInfo.reason);
        __LDBG_printf("depc=%08x epc1=%08x epc2=%08x epc3=%08x exccause=%08x excvaddr=%08x reason=%u", resetInfo.depc, resetInfo.epc1, resetInfo.epc2, resetInfo.epc3, resetInfo.exccause, resetInfo.excvaddr, resetInfo.reason);

        #if ENABLE_DEEP_SLEEP == 0
            __DBG_printf("WIFI mode=%u default=%u", wifi_get_opmode(), wifi_get_opmode_default());
            if (wifi_get_opmode() != WIFI_OFF) {
                wifi_set_opmode_current(WIFI_OFF);
            }
            if (wifi_get_opmode_default() != WIFI_OFF) {
                wifi_set_opmode(WIFI_OFF);
            }
        #endif

        __LDBG_printf("valid=%d safe_mode=%u counter=%u new=%u", !!_storedData, _storedData.isSafeMode(), _storedData.getResetCounter(), _data.getResetCounter());
        __LDBG_printf("info=%s crash=%u reset=%u reboot=%u wakeup=%u", getResetInfo().c_str(), hasCrashDetected(), hasResetDetected(), hasRebootDetected(), hasWakeUpDetected());

    #elif ESP32

        __LDBG_printf("init reset detector");

        struct rst_info resetInfo = {};
        resetInfo.reason = esp_reset_reason();

        _readData();
        ++_data;
        _data.pushReason(resetInfo.reason);

        __LDBG_printf("depc=%08x epc1=%08x epc2=%08x epc3=%08x exccause=%08x excvaddr=%08x reason=%u", resetInfo.depc, resetInfo.epc1, resetInfo.epc2, resetInfo.epc3, resetInfo.exccause, resetInfo.excvaddr, resetInfo.reason);

    #endif

    #if DEBUG_RESET_DETECTOR
        __LDBG_printf("reason history");
        for(auto reason: _storedData) {
            __LDBG_printf("%u - %s", reason, getResetReason(reason));
        }
        __LDBG_printf("%u - %s (recent)", _data.getReason(), getResetReason());
    #endif

    _writeData();
    #if ESP8266
        armTimer();
    #elif ESP32
        // the timer must be started in setup()
    #endif

    #if IOT_SWITCH && IOT_SWITCH_STORE_STATES_RTC_MEM
        SwitchPlugin_rtcMemLoadState();
    #endif
}

const __FlashStringHelper *ResetDetector::getResetReason(uint8_t reason)
{
#if USE_ESP_GET_RESET_REASON
    if (static_cast<int>(reason) == 254) {
        return F("User exception (panic/abort/assert)");
    }
    switch(reason) {
        // normal startup by power on
        case REASON_DEFAULT_RST:
            return F("Power On");
        // hardware watch dog reset
        case REASON_WDT_RST:
            return F("Hardware Watchdog");
        // exception reset, GPIO status won’t change
        case REASON_EXCEPTION_RST:
            return F("Exception");
        // software watch dog reset, GPIO status won’t change
        case REASON_SOFT_WDT_RST:
            return F("Software Watchdog");
        // software restart ,system_restart , GPIO status won’t change
        case REASON_SOFT_RESTART:
            return F("Software/System restart");
        // wake up from deep-sleep
        case REASON_DEEP_SLEEP_AWAKE:
            return F("Deep-Sleep Wake");
        // // external system reset
        case REASON_EXT_SYS_RST:
            return F("External System");
        default:
            break;
    }
    return F("Unknown");
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
        default:
            break;
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


#if DEBUG

void ResetDetector::__setResetCounter(Counter_t counter)
{
    _storedData = counter;
    _data = counter;
    _writeData();
}
#endif

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

#include "logger.h"

static ResetDetectorPlugin plugin;

PROGMEM_DEFINE_PLUGIN_OPTIONS(
    ResetDetectorPlugin,
    "rd",               // name
    "Reset Detector",   // friendly name
    "safecrash",        // web_templates
    "safecrash",        // config_forms
    "",                 // reconfigure_dependencies
    PluginComponent::PriorityType::RESET_DETECTOR,
    PluginComponent::RTCMemoryId::RESET_DETECTOR,
    static_cast<uint8_t>(PluginComponent::MenuType::CUSTOM),
    true,               // allow_safe_mode
    true,               // setup_after_deep_sleep
    true,               // has_get_status
    false,              // has_config_forms
    false,              // has_web_ui
    true,               // has_web_templates
    true,               // has_at_mode
    0                   // __reserved
);

ResetDetectorPlugin::ResetDetectorPlugin() : PluginComponent(PROGMEM_GET_PLUGIN_OPTIONS(ResetDetectorPlugin))
{
    #if ESP32
        __preinit();
    #endif
    REGISTER_PLUGIN(this, "ResetDetectorPlugin");
}

void ResetDetectorPlugin::getStatus(Print &output)
{
    auto info = SaveCrash::createFlashStorage().getInfo();
    output.printf_P(PSTR("%u crash report(s), total size "), info.numTraces());
    output.print(formatBytes(info.size()));
    output.printf_P(PSTR(HTML_S(br) "%s of %s available"), formatBytes(info.available()).c_str(), formatBytes(info.capacity()).c_str());
}


#if !RESET_DETECTOR_INCLUDE_HPP_INLINE
#include "reset_detector.hpp"
#endif
