
/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "reset_detector.h"
#if HAVE_KFC_PLUGINS
#include <LoopFunctions.h>
#include <PluginComponent.h>
#endif

#if DEBUG_RESET_DETECTOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

ResetDetector resetDetector;

ResetDetector::ResetDetector() : _timer()
{
#if !DEBUG_RESET_DETECTOR
    _init();
#else
    #warning Call _init() manually after the debug output is turned on
#endif
}

void ResetDetector::_init()
{
    _debug_println();

    ResetDetectorData_t data = {};
    auto isValid = false;

#if HAVE_KFC_PLUGINS

    if (RTCMemoryManager::read(PluginComponent::RTCMemoryId::RESET_DETECTOR, &data, sizeof(data))) {
        isValid = true;
    }

#else
    if (ESP.rtcUserMemoryRead(RESET_DETECTOR_RTC_MEM_ADDRESS, (uint32_t *)&data, sizeof(data))) {
        if (data.magic_word == RESET_DETECTOR_MAGIC_WORD) {
            if (crc16_update(&data, sizeof(data) - sizeof(data.crc)) == data.crc) {
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
        if (data.reset_counter < std::numeric_limits<decltype(data.reset_counter)>::max()) { // stop before overflow
            data.reset_counter++;
        }
        _safeMode = data.safe_mode;
        _resetCounter = data.reset_counter;
    }
    else {
        _safeMode = 0;
        _resetCounter = 0;
    }
    _initialResetCounter = _resetCounter;


#if HAVE_KFC_PLUGINS
    __LDBG_printf("\n\n\nRD: valid %d, safe mode=%d, reset counter=%d", isValid, data.safe_mode, data.reset_counter);
    __LDBG_printf("RD: reset reason: %s (%d), reset info: %s, crash=%d, reset=%d, reboot=%d, wakeup=%d", getResetReason().c_str(), _resetReason, getResetInfo().c_str(), hasCrashDetected(), hasResetDetected(), hasRebootDetected(), hasWakeUpDetected());
#else
    __LDBG_printf("\n\n\nRD: valid %d, magic word %08x, safe mode=%d, reset counter=%d", isValid, data.magic_word, data.safe_mode, data.reset_counter);
    __LDBG_printf("RD: reset reason: %s (%d), reset info: %s, crash=%d, reset=%d, reboot=%d, wakeup=%d", getResetReason().c_str(), _resetReason, getResetInfo().c_str(), hasCrashDetected(), hasResetDetected(), hasRebootDetected(), hasWakeUpDetected());
#endif

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

void ResetDetector::disarmTimer()
{
    ets_timer_disarm(&_timer);
    ets_timer_done(&_timer);
}


void ResetDetector::_timerCallback(void *arg)
{
    auto rd = reinterpret_cast<ResetDetector *>(arg);
    rd->clearCounter();
    rd->disarmTimer();
}

const String ResetDetector::getResetReason() const
{
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
    __LDBG_printf("RD: reset counter = 0 (%d)", _resetCounter);
    _resetCounter = 0;
    _writeData();
}

#if DEBUG
void ResetDetector::__setResetCounter(uint8_t counter)
{
    _initialResetCounter = _resetCounter = counter;
    _writeData();
}
#endif

void ResetDetector::setSafeModeAndClearCounter(uint8_t safeMode)
{
    __LDBG_printf("RD: reset counter = 0 (%d) and safe_mode=%u", _resetCounter, safeMode);
    _safeMode = safeMode;
    _resetCounter = 0;
    _writeData();
}

void ResetDetector::_writeData()
{
    ResetDetectorData_t data = { _resetCounter, _safeMode };

#if HAVE_KFC_PLUGINS

    RTCMemoryManager::write(PluginComponent::RTCMemoryId::RESET_DETECTOR, &data, sizeof(data));

#else

    data.magic_word = RESET_DETECTOR_MAGIC_WORD;
    data.crc = crc16_update(&data, sizeof(data) - sizeof(data.crc));

    if (!ESP.rtcUserMemoryWrite(RESET_DETECTOR_RTC_MEM_ADDRESS, (uint32_t *)&data, sizeof(data))) {
        __LDBG_printf("RD: Failed to write to RTC MEM");
    }

#endif

}

#if HAVE_KFC_PLUGINS

#include "logger.h"

StartupTimings _startupTimings;

#if IOT_REMOTE_CONTROL

void StartupTimings::dump(Print &output)
{
    output.printf_P(PSTR("setup() call: %ums\n"), _setup);
    output.printf_P(PSTR("first loop() call: %ums\n"), _loop);
    output.printf_P(PSTR("Wifi connected: %ums\n"), _wifiConnected);
    output.printf_P(PSTR("WiFi (DHCP/IP stack) ready: %ums\n"), _wifiGotIP);
    output.printf_P(PSTR("NTP time received: %ums\n"), _ntp);
    output.printf_P(PSTR("MQTT connection established: %ums\n"), _mqtt);
    output.printf_P(PSTR("Deep Sleep: %ums\n"), _deepSleep);
}

void StartupTimings::log()
{
    Logger_notice(F("WiFi connected/ready %ums/%ums; NTP %ums; MQTT %ums, Deep Sleep %ums"), _wifiConnected, _wifiGotIP, _ntp, _mqtt, _deepSleep);
}

#else

void StartupTimings::dump(Print &output)
{
    output.printf_P(PSTR("WiFi ready: %ums\n"), _wifiGotIP);
}

void StartupTimings::log() {}

#endif

static ResetDetectorPlugin plugin;

#define ALWAYS_RUN_SETUP   (DEBUG_RESET_DETECTOR ? true : false)

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
    ALWAYS_RUN_SETUP,   // allow_safe_mode
    ALWAYS_RUN_SETUP,   // setup_after_deep_sleep
    false,              // has_get_status
    false,              // has_config_forms
    false,              // has_web_ui
    false,              // has_web_templates
    true,               // has_at_mode
    0                   // __reserved
);

ResetDetectorPlugin::ResetDetectorPlugin() : PluginComponent(PROGMEM_GET_PLUGIN_OPTIONS(ResetDetectorPlugin))
{
    REGISTER_PLUGIN(this, "ResetDetectorPlugin");
}

#if DEBUG_RESET_DETECTOR
    void setup(SetupModeType mode)
    {
        _init();
    }
#endif


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
            args.getStream().printf_P(PSTR("safe mode: %d\nreset counter: %d\ninitial reset counter: %d\ncrash: %d\nreboot: %d\nreset: %d\nreset reason: %s\n"),
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
        return true;
    }
    return false;
}

#endif

#endif
