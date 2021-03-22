/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "reset_detector.h"
#include <LoopFunctions.h>
#include <PluginComponent.h>
#include <PrintHtmlEntitiesString.h>
#include "save_crash.h"

#if 1
#include <debug_helper_enable.h>
#else
#undef __LDBG_printf
#if DEBUG_RESET_DETECTOR
#define __LDBG_printf(fmt, ...) ::printf_P(PSTR("RD%04u: " fmt "\n"), micros() / 1000, ##__VA_ARGS__)
#else
#define __LDBG_printf(...)
#endif
#endif


ResetDetectorUnitialized resetDetector __attribute__((section(".noinit")));;

extern "C" {

#if defined(ESP8266)

    #include "user_interface.h"

#endif

#if !ENABLE_DEEP_SLEEP

    void preinit(void)
    {
        resetDetector.init();
        resetDetector.begin();
    }

#endif

}


// extern void testspi();

// void testspi()
// {
//     __DBG_printf("create FlashStorage");
//     auto fs = FlashStorage(0x3F8, 0x3FA);
//         //(((uint32_t)&_SAVECRASH_start) - 0x40200000) / SPI_FLASH_SEC_SIZE, (((uint32_t)&_SAVECRASH_end) - 0x40200000) / SPI_FLASH_SEC_SIZE);


//     __DBG_printf("init %04x - %04x - %04x",fs.begin(), fs.end(), (SECTION_KFCFW_START_ADDRESS-0x40200000)/SPI_FLASH_SEC_SIZE);

//     uint32_t buf[10]={};

//     auto result = fs.read(buf, sizeof(buf), fs.begin(), 0);
//     if (result._result==FlashResultType::SUCCESS) {
//         __DBG_printf("result=%u size=%u data=%s", result._result, result.readSize(), printable_string(buf, result.readSize()).c_str());
//     }
//     else {
//         __DBG_printf("result=%u size=%u data=ERROR", result._result, result.readSize());
//     }

//     __DBG_printf("----------------------------------------------------------------------");

//     __DBG_printf("init");
//     result = fs.init(fs.begin());

//     __DBG_printf("append");
//     memset(buf, 0, sizeof(buf));
//     strcpy_P((char *)buf, PSTR("test123"));
//     fs.append(buf, 8, result);

//     __DBG_printf("finalize");
//     fs.finalize(result);
//     __DBG_printf("result=%u sector=%u size=%u crc=0x%08x", result._result, result._sector, result.size(), result._crc);

//     __DBG_printf("validate size=%u", result._size);
//     fs.validate(result);

//     __DBG_printf("----------------------------------------------------------------------");

//     __DBG_printf("copy");
//     result = fs.copy(fs.begin(), fs.begin() + 1);

//     __DBG_printf("append size=%u", result._size);
//     strcpy_P((char *)buf, PSTR("123test"));
//     fs.append(buf, 8, result);

//     __DBG_printf("finalize size=%u", result._size);
//     fs.finalize(result);
//     __DBG_printf("result=%u sector=%u size=%u crc=0x%08x", result._result, result._sector, result.size(), result._crc);

//     __DBG_printf("validate size=%u", result._size);
//     fs.validate(result);

//     __DBG_printf("----------------------------------------------------------------------");

//     result = fs.read(buf, sizeof(buf), fs.begin()+1, 0);
//     if (result._result==FlashResultType::SUCCESS) {
//         __DBG_printf("result=%u size=%u data=%s", result._result, result.readSize(), printable_string(buf, result.readSize()).c_str());
//     }
//     else {
//         __DBG_printf("result=%u size=%u data=ERROR", result._result, result.readSize());
//     }
//     __DBG_printf("----------------------------------------------------------------------");

//     result = fs.read(buf, sizeof(buf), fs.begin(), 0);
//     if (result._result==FlashResultType::SUCCESS) {
//         __DBG_printf("result=%u size=%u data=%s", result._result, result.readSize(), printable_string(buf, result.readSize()).c_str());
//     }
//     else {
//         __DBG_printf("result=%u size=%u data=ERROR", result._result, result.readSize());
//     }

//     __DBG_printf("----------------------------------------------------------------------");
//     for(const auto &result: fs.find(true)) {
//         __LDBG_printf("find: sector=%04x size=%u space=%u", result._sector, result.size(), result.space());

//     }

// }

void ResetDetector::end()
{
    if (_uart) {
        __LDBG_printf("end");
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
#if DEBUG_RESET_DETECTOR
    if (_uart) {
        __LDBG_printf("begin() called multiple times without end()");
        end();
    }
#endif
    _uart = uart_init(UART0, 115200, (int) SERIAL_8N1, (int) SERIAL_FULL, 1, 64, false);

#if DEBUG_RESET_DETECTOR
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
    if (static_cast<int>(reason) == 254) {
        return F("User exception (panic/abort/assert)");
    }
    // copy of ESP.getResetInfo() below returning PROGMEM string
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
    "Reset Detector",   // friendly name


    "safecrash",        // web_templates
    "safecrash",        // config_forms


    "",                 // reconfigure_dependencies
    PluginComponent::PriorityType::RESET_DETECTOR,
    PluginComponent::RTCMemoryId::RESET_DETECTOR,
    static_cast<uint8_t>(PluginComponent::MenuType::NONE), //(CUSTOM),
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
    PluginComponentInitRegisterEx();
    REGISTER_PLUGIN(this, "ResetDetectorPlugin");
}

void ResetDetectorPlugin::getStatus(Print &output)
{
    // int counter = espSaveCrash.count();
    // String name = FSPGM(crash_dump_file);
    // auto pos = name.indexOf('%');
    // if (pos != -1) {
    //     name.remove(pos);
    // }
    // auto path = String('/');
    // if ((pos = name.indexOf('/', 1)) != -1) {
    //     path = name.substring(0, pos);
    // }
    // size_t size = 0;
    // ListDir dir(path);
    // while(dir.next()) {
    //     if (dir.isFile() && dir.fileName().startsWith(name)) {
    //         counter++;
    //         size += dir.fileSize();
    //     }
    // }
    // __LDBG_printf("path=%s name=%s size=%u counter=%u espcounter=%u", path.c_str(), name.c_str(), size, counter, espSaveCrash.count());
    auto info = SaveCrash::createFlashStorage().getInfo();
    output.printf_P(PSTR("%u crash report(s), total size "), info._counter);
    output.print(formatBytes(info._size));
    output.printf_P(PSTR(HTML_S(br) "%u out of %u 4KB sectors used ("), info._sector_used, info._sectors_total);
    output.print(formatBytes(info._sectors_total * 4096));
    output.print(')');
}


#if !RESET_DETECTOR_INCLUDE_HPP_INLINE
#include "reset_detector.hpp"
#endif
