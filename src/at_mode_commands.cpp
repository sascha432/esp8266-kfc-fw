/**
  Author: sascha_lammers@gmx.de
*/

#include "at_mode_commands.h"
#include "kfc_fw_config.h"
#include "save_crash.h"
#include "web_server.h"
#include <Cat.h>
#include <DumpBinary.h>
#include <EventScheduler.h>
#include <ListDir.h>
#include "blink_led_timer.h"
#include "HeapSelector.h"
#include "PinMonitor.h"
#include "plugins.h"
#include "plugins_menu.h"
#include "deep_sleep.h"
#include <limits>
#include <stl_ext/memory.h>

#if HAVE_I2CSCANNER
#    include "i2c_scanner.h"
#endif

#if __LED_BUILTIN_WS2812_NUM_LEDS
#    include <NeoPixelEx.h>
#endif

#if ESP8266
#    include <umm_malloc/umm_malloc.h>
     extern "C" {
#        include <umm_malloc/umm_local.h>
     }
#    include <core_esp8266_waveform.h>
#    include <core_version.h>
#endif

#if ESP32
#    include "esp32_perfmon.hpp"
#endif

#if DEBUG_AT_MODE
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

using KFCConfigurationClasses::System;
using KFCConfigurationClasses::Network;

#if DEBUG

AtModePrintLoop *atModePrintLoop;

AtModePrintLoop::AtModePrintLoop() :
    _type(DisplayType::HEAP)
{
    stdex::reset(atModePrintLoop, this);
}

AtModePrintLoop::~AtModePrintLoop()
{
    if (this == atModePrintLoop) {
        atModePrintLoop = nullptr;
    }
}

void AtModePrintLoop::setType(DisplayType type, Event::milliseconds interval)
{
    _type = type;
    if (_type == DisplayType::HEAP || _type == DisplayType::HEAP_UMM) {
        LOOP_FUNCTION_ADD(loop);
    }
    else {
        LoopFunctions::remove(loop);
    }
    _Timer(_timer).add(interval, true, AtModePrintLoop::printTimerCallback);
    #if ESP8266
        _maxIram = 0;
        _minIram = 0;
    #endif
    #if HAS_MULTI_HEAP
        {
            SELECT_IRAM();
            _minIram = ESP.getFreeHeap();
        }
    #endif
    {
        SELECT_DRAM();
        _maxHeap = 0;
        _minHeap = ESP.getFreeHeap();
    }
    _rssiMin = std::numeric_limits<decltype(_rssiMin)>::min();
    _rssiMax = 0;
}

AtModePrintLoop::DisplayType AtModePrintLoop::getType() const
{
    return _type;
}

void AtModePrintLoop::printHeap()
{
    #if ESP32
        Serial.printf_P(PSTR("+HEAP: heap=%u(min=%u/max=%u) psram=%u(min=%u) cpu=%dMHz uptime=%us\n"),
            ESP.getFreeHeap(),
            _minHeap,
            _maxHeap,
            ESP.getFreePsram(),
            ESP.getMinFreePsram(),
            ESP.getCpuFreqMHz(),
            getSystemUptime()
        );
    #else
        uint32_t freeIram = 0;
        uint32_t freeDram;
        #if HAS_MULTI_HEAP
            {
                SELECT_IRAM();
                freeIram = ESP.getFreeHeap();
            }
            HeapSelectDram dRam;
        #endif
        {
            SELECT_DRAM();
            freeDram = ESP.getFreeHeap();
        }

        Serial.printf_P(PSTR("+HEAP: free=%u(min=%u/max=%u) iram=%u(min=%u/max=%u) cpu=%dMHz frag=%u uptime=%us\n"),
            freeDram,
            _minHeap,
            _maxHeap,
            freeIram,
            _minIram,
            _maxIram,
            ESP.getCpuFreqMHz(),
            ESP.getHeapFragmentation(),
            getSystemUptime()
        );
        #if defined(UMM_STATS) || defined(UMM_STATS_FULL)
            if (_type == DisplayType::HEAP_UMM) {
                SELECT_DRAM();
                umm_print_stats(2);
                #if HAS_MULTI_HEAP
                    {
                        SELECT_IRAM();
                        umm_print_stats(2);
                    }
                #endif
            }
        #endif
    #endif
}

void AtModePrintLoop::printGPIO()
{
    Serial.print(F("+GPIO: "));
    #if defined(ESP8266)
        for(uint8_t i = 0; i < NUM_DIGITAL_PINS; i++) {
            if (i == 10 || (i != 1 && !isFlashInterfacePin(i))) { // do not display TX and flash SPI
                // pinMode(i, INPUT);
                Serial.printf_P(PSTR("%u=%u "), i, digitalRead(i));
                #if ESP8266
                    if (i == 16) {
                        Serial.print((GP16E & 1) ? F("IN ") : F("OUT "));
                    }
                    else {
                        String tmp;
                        tmp = GPO & (1 << i) ? F("OUT") : F("IN");
                        tmp += GPF(i) & (1 << GPFPU) ? F("_PULLUP ") : F(" ");
                        Serial.print(tmp);
                    }
                #endif
            }
        }
        Serial.printf_P(PSTR("A0=%u\n"), analogRead(A0));
    #elif defined(ESP32)
        for(uint8_t i = 0; i < NUM_DIGITAL_PINS; i++) {
            Serial.printf_P(PSTR("%u=%u%c"), i, digitalRead(i), ((i == NUM_DIGITAL_PINS - 1) || ((i % 8) == 7)) ? '\n' : ' ');
        }
    #endif
    #if defined(HAVE_IOEXPANDER)
        IOExpander::config.dumpPins(Serial);
    #endif
}

void AtModePrintLoop::printRSSI()
{
    int16_t rssi = WiFi.RSSI();
    _rssiMin = std::max(_rssiMin, rssi);
    _rssiMax = std::min(_rssiMax, rssi);
    Serial.printf_P(PSTR("+RSSI: %d dBm (min/max %d/%d)\n"), rssi, _rssiMin, _rssiMax);
}

void AtModePrintLoop::print()
{
    switch(_type) {
        case DisplayType::HEAP:
        case DisplayType::HEAP_UMM:
            printHeap();
            break;
        case DisplayType::GPIO:
            printGPIO();
            break;
        case DisplayType::RSSI:
            printRSSI();
        default:
            break;
    }
}

void AtModePrintLoop::printTimerCallback(Event::CallbackTimerPtr timer)
{
    if (atModePrintLoop == nullptr) {
        timer->disarm();
        return;
    }
    atModePrintLoop->print();
}

bool AtModePrintLoop::removeTimer()
{
    return _Timer(_timer).remove();
}

void AtModePrintLoop::remove()
{
    _Timer(_timer).remove();
    LoopFunctions::remove(loop);
    delete this;
}

void AtModePrintLoop::_loop()
{
    #if HAS_MULTI_HEAP
        {
            SELECT_IRAM();
            _minIram = std::min<uint32_t>(ESP.getFreeHeap(), _minIram);
            _maxIram = std::max<uint32_t>(ESP.getFreeHeap(), _maxIram);
        }
    #endif
    SELECT_DRAM();
    _minHeap = std::min<uint32_t>(ESP.getFreeHeap(), _minHeap);
    _maxHeap = std::max<uint32_t>(ESP.getFreeHeap(), _maxHeap);
}

void AtModePrintLoop::loop()
{
    if (atModePrintLoop) {
        atModePrintLoop->_loop();
    }
}

static void printHeapOnce()
{
    if (atModePrintLoop) {
        atModePrintLoop->printHeap();
    }
    else {
        Serial.printf_P(PSTR("+HEAP: free=%u cpu=%dMHz frag=%u"), ESP.getFreeHeap(), ESP.getCpuFreqMHz(), getHeapFragmentation());
    }
}

static void atModePrintLoopCommand(AtModeArgs &args, bool isHeap, AtModePrintLoop::DisplayType type)
{
    if (args.requireArgs(0, 2)) {
        auto interval = args.toMillis(0, 0, 3600 * 1000, 0, String('s'));
        auto umm = isHeap && args.equalsIgnoreCase(1, F("umm"));
        if (interval < 250) {
            if (atModePrintLoop) {
                atModePrintLoop->remove();
                args.print(F("Interval disabled"));
            }
            printHeapOnce();
            Serial.println();
        }
        else {
            if (!atModePrintLoop) {
                new AtModePrintLoop();
            }
            atModePrintLoop->setType(umm ? AtModePrintLoop::DisplayType::HEAP_UMM : type, Event::milliseconds(interval));
            args.print(F("Interval set to %ums"), interval);
        }
    }
}

// +HEAP=<interval[,umm]>  (DEBUG)
// Display heap usage every interval (can be 1s or 1000ms, 0 shows it once). If `umm` is added, the umm heap statistics are displayed (ESP8266 only)

void ATModeCommands::HeapCommand(AtModeArgs &args)
{
    atModePrintLoopCommand(args, true, AtModePrintLoop::DisplayType::HEAP);
}

// +RSSI=[interval in seconds|0=disable]  (DEBUG)
// Display the WiFi RSSI every interval (can be 1s or 1000ms, 0 shows it once)

void ATModeCommands::RssiCommand(AtModeArgs &args)
{
    atModePrintLoopCommand(args, false, AtModePrintLoop::DisplayType::RSSI);
}

// +GPIO=<interval>  (DEBUG)
// Display GPIO pin states every interval (can be 1s or 1000ms, 0 shows it once)

void ATModeCommands::GpioCommand(AtModeArgs &args)
{
    atModePrintLoopCommand(args, false, AtModePrintLoop::DisplayType::GPIO);
}

#endif

// +AT
// Print OK
// +AT?
// Show help

// +REM
// Ignore comment

void ATModeCommands::IgnoreCommand(AtModeArgs &args)
{
    // does nothing
}

#ifndef DISABLE_TWO_WIRE

// +I2CS=<pin-sda>,<pin-scl>[,<speed=100000>,<clock-stretch=45000>,<start|stop>]
// Configure I2C Bus

void ATModeCommands::I2CSetupCommand(AtModeArgs &args)
{
    auto sda = args.toIntMinMax<uint8_t>(0, 0, NUM_DIGITAL_PINS, KFC_TWOWIRE_SDA);
    auto scl = args.toIntMinMax<uint8_t>(1, 0, NUM_DIGITAL_PINS, KFC_TWOWIRE_SCL);
    uint32_t speed = args.toInt(2, KFC_TWOWIRE_CLOCK_SPEED);
    uint32_t stretch = args.toInt(3, KFC_TWOWIRE_CLOCK_STRETCH);
    bool stop = args.has(F("stop"));
    if (isFlashInterfacePin(sda) || isFlashInterfacePin(scl)) {
        args.print(F("Pins 6, 7, 8, 9, 10 and 11 cannot be used"));
    }
    else if (stop) {
        pinMode(sda, INPUT);
        pinMode(scl, INPUT);
        Wire.begin(255, 255);
        args.print(F("I2C stopped"));
    }
    else {
        config.initTwoWire(true);
        Wire.begin(sda, scl);
        #if ESP8266
            // the stock ESP32 Wire has no clock stretch limit
            Wire.setClockStretchLimit(stretch);
        #endif
        Wire.setClock(speed);
        args.print(F("I2C started on %u:%u (sda:scl), speed %u, clock stretch %u"), sda, scl, speed, stretch);
    }
}

// +I2CTM=<address>,<data,...>
// Transmit data to slave

void ATModeCommands::I2CTransmitCommand(AtModeArgs &args)
{
    if (args.requireArgs(1)) {
        uint8_t address = args.toNumber(0, 0x48);
        uint8_t written = 0;
        Wire.beginTransmission(address);
        for(uint8_t i = 1; i < args.size(); i++) {
            written += Wire.write(args.toNumber(i, 0xff));
        }
        uint8_t error;
        if ((error = Wire.endTransmission(true)) == 0) {
            args.print(F("slave 0x%02X: transmitted %u bytes"), address, written);
        }
        else {
            args.print(F("slave 0x%02X: transmitted %u bytes, error %u"), address, written, error);
        }
    }
}

// +I2CRQ=<address>,<length>
// Request data from slave

void ATModeCommands::I2CReceiveCommand(AtModeArgs &args)
{
    if (args.requireArgs(1, 2)) {
        uint8_t address = args.toNumber(0, 0x48);
        uint8_t length = args.toNumber(1, 0);
        if (Wire.requestFrom(address, length) == length) {
            auto pbuf = std::unique_ptr<uint8_t[]>(new uint8_t[length + 1]());
            auto buf = pbuf.get();
            if (!buf) {
                args.print(F("failed to allocate memory. %u bytes"), length + 1);
            }
            else {
                auto read = Wire.readBytes(buf, length);
                args.print(F("slave 0x%02X: requested %u byte. data:"), address, read);
                DumpBinary(args.getStream(), DumpBinary::kGroupBytesDefault, 16).dump(buf, read);
            }
        }
        else {
            args.print(F("slave 0x%02X: requesting data failed, length=%u"), address, length);
        }
    }
}

#endif

#if HAVE_I2CSCANNER

// +I2CSCAN=[<start-address=1>][,<end-address=127>][,<sda=4|any|no-init>,<scl=5>]
// Scan I2C Bus. If 'any' is passed as third argument, all available PINs are probed for I2C devices

void ATModeCommands::I2CScanForDevicesCommand(AtModeArgs &args)
{
    auto startAddress = args.toIntMinMax<uint8_t>(0, 1, 255, 1);
    auto endAddress = args.toIntMinMax<uint8_t>(1, startAddress, 255, 127);
    auto sda = args.toIntMinMax<uint8_t>(2, 0, 16, KFC_TWOWIRE_SDA);
    auto scl = args.toIntMinMax<uint8_t>(3, 0, 16, KFC_TWOWIRE_SCL);
    if (args.has(F("list"))) {
        scanPorts(args.getStream(), I2C_SCANNER_LIST_PIN_PAIRS, 0);

    }
    else  if (args.has(F("any"))) {
        scanPorts(args.getStream(), startAddress, endAddress);
    }
    else {
        if (args.has(F("noinit")) || args.has(F("no-init"))) {
            sda = scl = 0xff;
        }
        scanI2C(args.getStream(), sda, scl, startAddress, endAddress);
    }
}

#endif

// +DSLP=[<milliseconds>[,<mode>]]
// Enter deep sleep

void ATModeCommands::DeepSleepCommand(AtModeArgs &args)
{
    KFCFWConfiguration::milliseconds time(args.toMillis(0));
    auto mode = (RFMode)args.toInt(1, RF_DEFAULT);
    #if ESP32
        args.print(F("Entering deep sleep... time=%ums"), time.count());
        ESP.deepSleep(time.count() * 1000ULL);
    #else
        args.print(F("Entering deep sleep... time=%ums deep_sleep_max=%.0fms mode=%u"), time.count(), (ESP.deepSleepMax() / 1000.0), mode);
        #if ENABLE_DEEP_SLEEP
            DeepSleep::deepSleepParams.enterDeepSleep(time, mode);
        #else
            WiFi.disconnect(true);
            ESP.deepSleep(time.count() * 1000ULL, mode);
            ESP.deepSleep(ESP.deepSleepMax() / 2, mode);
            ESP.deepSleep(0, mode);
        #endif
    #endif
}

// +RST=[<s>]
// Soft reset. 's' enables safe mode

void ATModeCommands::ResetCommand(AtModeArgs &args)
{
    if (args.startsWith(0, F("s"))) {
        args.print(F("Software reset, safe mode enabled..."));
        LoopFunctions::callOnce([]() {
            config.restartDevice(true);
        });
    }
    else {
        args.print(F("Software reset..."));
        LoopFunctions::callOnce([]() {
            config.restartDevice(false);
        });
    }
}

// +LOAD
// Discard changes and load settings from EEPROM

void ATModeCommands::LoadCommand(AtModeArgs &args)
{
    config.read();
    args.ok();
}

// +STORE
// Store current settings in EEPROM

void ATModeCommands::StoreCommand(AtModeArgs &args)
{
    config.write();
    args.ok();
}

// +IMPORT=<filename|set_dirty>[,<handle>[,<handle>,...]]
// Import settings from JSON file

void ATModeCommands::ImportCommand(AtModeArgs &args)
{
    if (args.requireArgs(1)) {
        auto res = false;
        auto filename = args.get(0);
        if (StrView(F("set_dirty")).equals(filename)) {
            config.setConfigDirty(true);
            args.print(F("Configuration marked dirty"));
        }
        else {
            auto file = KFCFS.open(filename, fs::FileOpenMode::read);
            if (file) {
                args.print(filename);
                auto &output = args.getStream();
                Configuration::Handle_t *handlesPtr = nullptr;
                Configuration::Handle_t handles[AT_MODE_MAX_ARGUMENTS + 1];
                auto iterator = args.begin();
                if (++iterator != args.end()) {
                    output.print(F(": "));
                    handlesPtr = &handles[0];
                    auto count = 0;
                    while(iterator != args.end() && count < AT_MODE_MAX_ARGUMENTS) {
                        handles[count++] = (uint16_t)strtoul(*iterator, nullptr, 0); // auto detect base
                        handles[count] = 0;
                        output.printf_P(PSTR("0x%04x "), handles[count - 1]);
                        ++iterator;
                    }
                }
                output.println();
                res = config.importJson(file, handlesPtr);
            }
            if (res) {
                args.ok();
                config.write();
                config.setConfigDirty(true);
            } else {
                args.print(F("Failed to import: %s"), filename);
            }
        }
    }
}

// +FACTORY
// Restore factory settings (but do not store in EEPROM)

void ATModeCommands::FactoryCommand(AtModeArgs &args)
{
    config.restoreFactorySettings();
    args.ok();
}

// +FSR
// FACTORY, STORE, RST in sequence

void ATModeCommands::FactoryStoreResetCommand(AtModeArgs &args)
{
    config.restoreFactorySettings();
    config.write();
    args.ok();
    LoopFunctions::callOnce([]() {
        config.restartDevice(false);
    });
}

#if defined(HAVE_NVS_FLASH)

class NVSDebugAccess {
public:
    static esp_err_t open() {
        return config._nvs_open(false);
    }
    static void close() {
        return config._nvs_close();
    }
};

// +NVS=<format|dump>
// Format NVS partition and do factory reset or dump debug info

void ATModeCommands::NVSCommand(AtModeArgs &args)
{
    auto cmd = args.toString(0);
    if (F("format") == cmd) {
        config.formatNVS();
        config.restoreFactorySettings();
        config.write();
        args.ok();
    }
    else if (F("stats") == cmd) {
        auto stream = &args.getStream();
        LoopFunctions::callOnce([stream]() {
            stream->println(F("+NVS: Stats"));
                esp_err_t err;
                if ((err = NVSDebugAccess::open()) == ESP_OK) {
                    nvs_stats_t stats;
                    #ifdef KFC_CFG_NVS_PARTITION_NAME
                        err = nvs_get_stats(KFC_CFG_NVS_PARTITION_NAME, &stats);
                    #else
                        err = nvs_get_stats(NVS_DEFAULT_PART_NAME, &stats);
                    #endif
                    NVSDebugAccess::close();
                    if (err == ESP_OK) {
                        #if ESP8266
                            stream->printf_P(PSTR("+NVS: used_entries=%d free_entries=%d total_entries=%d namespace_count=%d max_seq_number=%d sectors=%d config_version=%u\n"), (int)stats.used_entries, (int)stats.free_entries, (int)stats.total_entries, (int)stats.namespace_count, (int)stats.max_seq_number, (int)stats.sectors, config.getVersion());
                        #else
                            stream->printf_P(PSTR("+NVS: used_entries=%d free_entries=%d total_entries=%d namespace_count=%d config_version=%u\n"), (int)stats.used_entries, (int)stats.free_entries, (int)stats.total_entries, (int)stats.namespace_count, config.getVersion());
                        #endif
                    }
                    else {
                        stream->printf_P(PSTR("+NVS: nvs_get_stats failed err=%x\n"), err);
                    }
                }
                else {
                    stream->printf_P(PSTR("+NVS: Failed to open err=%x\n"), err);
                }
        });
    }
    #if 0
        else if (F("dump") == cmd) {
            auto stream = &args.getStream();
            LoopFunctions::callOnce([stream]() {
                stream->println(F("+NVS: Dumping partition"));
                esp_err_t err;
                if ((err = NVSDebugAccess::open()) == ESP_OK) {
                    #ifdef KFC_CFG_NVS_PARTITION_NAME
                        nvs_dump(KFC_CFG_NVS_PARTITION_NAME);
                    #else
                        nvs_dump(NVS_DEFAULT_PART_NAME);
                    #endif
                    NVSDebugAccess::close();
                    stream->println(F("+NVS: OK"));
                }
                else {
                    stream->printf_P(PSTR("+NVS: Failed to open err=%x\n"), err);
                }
            });
        }
        else {
            args.invalidArgument(0, F("format|stats|dump"));
        }
    #else
        else {
            args.invalidArgument(0, F("format|stats"));
        }
    #endif
}

#endif

// +TOUCH=<filename>
// Touch file

void ATModeCommands::TouchCommand(AtModeArgs &args)
{
    if (args.requireArgs(1, 1)) {
        auto filename = args.get(0);
        __LDBG_printf("md=%s exists=%u", filename, KFCFS.exists(filename));
        auto result = createFileRecursive(filename, fs::FileOpenMode::append);
        args.print(F("%s: %s"), filename, result ? PSTR("success") : PSTR("failure"));
    }
}

// +MD=<directory>
// Create directory

void ATModeCommands::MkdirCommand(AtModeArgs &args)
{
    if (args.requireArgs(1, 1)) {
        auto filename = args.get(0);
        __LDBG_printf("md=%s exists=%u", filename, KFCFS.exists(filename));
        auto result = KFCFS.mkdir(filename); // first try to remove directory
        args.print(F("%s: %s"), filename, result ? PSTR("success") : PSTR("failure"));
    }
}

// +RM=<path>
// Delete file or directory

void ATModeCommands::RemoveCommand(AtModeArgs &args)
{
    if (args.requireArgs(1, 1)) {
        auto filename = args.get(0);
        __LDBG_printf("rm=%s exists=%u", filename, KFCFS.exists(filename));
        auto result = KFCFS.rmdir(filename); // first try to remove directory
        if (!result) {
            result = KFCFS.remove(filename);
        }
        args.print(F("%s: %s"), filename, result ? PSTR("success") : PSTR("failure"));
    }
}

// +RN=<path>,<new path>
// Rename file or directory

void ATModeCommands::RenameCommand(AtModeArgs &args)
{
    if (args.requireArgs(2, 2)) {
        auto filename = args.get(0);
        auto newFilename = args.get(1);
        auto result = KFCFS.rename(filename, newFilename);
        args.print(F("%s => %s: %s"), filename, newFilename, result ? PSTR("success") : PSTR("failure"));
    }
}

// +LS=[<directory>[,<hidden=true|false>,<subdirs=true|false>]]
// List files and directories

void ATModeCommands::ListCommand(AtModeArgs &args)
{
    auto &output = args.getStream();
    auto dir = ListDir(args.toString(0), !args.isTrue(2, true), args.isFalse(1, true));
    while(dir.next()) {
        args.print();
        if (dir.isFile()) {
            output.printf_P(PSTR("%8.8s "), formatBytes(dir.fileSize()).c_str());
        }
        else {
            output.print(F("[...]    "));
        }
        output.println(dir.fileName());
    }
}

// +LSR=[<directory>]
// List files and directories using FS.openDir()

void ATModeCommands::ListRecursiveCommand(AtModeArgs &args)
{
    auto &output = args.getStream();
    auto dir = KFCFS_openDir(args.toString(0));
    while(dir.next()) {
        args.print();
        if (dir.isDirectory()) {
            output.print(F("[...]    "));
        }
        else {
            output.printf_P(PSTR("%8.8s "), formatBytes(dir.fileSize()).c_str());
        }
        output.println(dir.fileName());
    }
}

// +CAT=<filename>
// Display text file

void ATModeCommands::CatCommand(AtModeArgs &args)
{
    if (args.requireArgs(1, 1)) {
        StreamOutput::Cat::dump(args.toString(0), args.getStream(), StreamOutput::Cat::kPrintInfo|StreamOutput::Cat::kPrintCrLfAsText);
    }
}

enum class WiFiCommandsType : uint8_t {
    RESET = 0,
    ST_ON,
    ST_OFF,
    ST_LIST,
    ST_CFG,
    AP_ON,
    AP_OFF,
    AP_STBY,
    DIAG,
    AVAIL_ST_LIST,
    NEXT,
};

#define WIFI_COMMANDS "reset|on|off|list|cfg|ap_on|ap_off|ap_standby|diag|stl|next"

static void at_mode_print_WiFi_info(AtModeArgs &args, uint8_t num, const Network::WiFi::StationModeSettings &cfg, bool showPassword = false)
{
    auto ssid = Network::WiFi::getSSID(num);
    bool isConfigured = (ssid && *ssid && cfg.isEnabled());
    bool isActive = (config.getWiFiConfigurationId() == num);

    constexpr size_t kLineLength = 42;
    char line[kLineLength + 1];
    std::fill(std::begin(line), std::end(line), '-');
    line[kLineLength] = 0;
    args.print(F("%s"), line);

    args.print(F("Connection #%u%s"),
        num,
        isConfigured ? PrintString(F(" (Priority %s%s)"), cfg.getPriorityStr().c_str(), isActive ? (WiFi.isConnected() ? PSTR(", connected") : PSTR(", active")) : emptyString.c_str()).c_str() : PSTR(" (not configured)")
    );
    if (isConfigured) {
        if (ssid && *ssid) {
            args.print(    F("SSID      %s"), (ssid && *ssid) ? ssid : PSTR("<none>"));
            args.print(    F("Password  %s"), showPassword ? Network::WiFi::getPassword(num) : PSTR("*********"));
        }
        if (cfg.isDHCPEnabled()) {
            if (isActive && WiFi.isConnected()) {
                args.print(F("DHCP IP   %s"), WiFi.localIP().toString().c_str());
                args.print(F("Subnet    %s"), WiFi.subnetMask().toString().c_str());
                args.print(F("Gateway   %s"), WiFi.gatewayIP().toString().c_str());
                args.print(F("DNS       %s, %s"), WiFi.dnsIP(0).toString().c_str(), WiFi.dnsIP(1).toString().c_str());
            }
            else {
                args.print(F("DHCP client enabled"));
            }
        }
        else {
            args.print(    F("Static IP %s"), cfg.getLocalIp().toString().c_str());
            args.print(    F("Subnet    %s"), cfg.getSubnet().toString().c_str());
            args.print(    F("Gateway   %s"), cfg.getGateway().toString().c_str());
            args.print(    F("DNS       %s, %s"), cfg.getDns1().toString().c_str(), cfg.getDns2().toString().c_str());
        }
    }
}

// +WIFI=<reset|on|off|list|cfg|ap_on|ap_off|ap_standby|diag|stl|next>
// Manage WiFi. The connection number of a station is the number displayed by 'list' and 'stl' (0..<max>)
//     reset                       Reset WiFi connection
//     on                          Enable WiFi station mode
//     off                         Disable WiFi station mode
//     list[,<1=show passwords>]   List WiFi networks, the active connection is marked
//     cfg,<connection>,<1|0|remove>,<SSID>,<password>[,<DHCP>|<IP>,<subnet>,<gateway>[,<DNS1|global>,<DNS2|global>]]
//                                 Configure the WiFi network <connection> and reconnect.
//                                 1/0 enables/disables the network, 'remove' deletes SSID and
//                                 password and keeps the current connection. The fifth argument
//                                 selects DHCP (starts with 'dhcp') or the static
//                                 <IP>,<subnet>,<gateway>. 'global' uses the DNS servers from the
//                                 network settings
//     ap_on                       Enable WiFi AP mode
//     ap_off                      Disable WiFi AP mode
//     ap_standby                  Set AP to stand-by mode (turns AP mode on if station mode cannot connect)
//     diag                        Print diagnostic information
//     stl                         List configured WiFi stations (id, SSID, priority, BSSID)
//     next                        Switch to the next enabled WiFi station (wraps around to the first one)

void ATModeCommands::WiFiCommand(AtModeArgs &args)
{
    if (args.requireArgs(1)) {
        auto cmd = static_cast<WiFiCommandsType>(stringlist_find_P_P(PSTR(WIFI_COMMANDS), args.get(0), '|'));
        switch(cmd) {
            case WiFiCommandsType::RESET:
                config.reconfigureWiFi(F("Reconfiguring WiFi adapter"));
                break;
            case WiFiCommandsType::ST_ON:
                args.print(F("enabling station mode"));
                WiFi.enableSTA(true);
                WiFi.reconnect();
                break;
            case WiFiCommandsType::ST_OFF:
                args.print(F("disabling station mode"));
                WiFi.enableSTA(false);
                break;
            case WiFiCommandsType::ST_LIST: {
                    auto network = Network::Settings::getConfig();
                    for(uint8_t i = 0; i < Network::WiFi::kNumStations; i++) {
                        at_mode_print_WiFi_info(args, i, network.stations[i], args.isTrue(1));
                    }
                }
                break;
            case WiFiCommandsType::ST_CFG: {
                    if (args.size() < 3) {
                        args.print(F("+WIFI=cfg,<connection=%u-%u>,<enable=1|disable=0|remove>,<SSID>,<password>[,<DHCP>|<IP>,<subnet>,<gateway>[,<DNS1|global>,<DNS2|global>]"),
                            Network::WiFi::StationConfigType::CFG_0, Network::WiFi::StationConfigType::CFG_LAST
                        );
                        auto num = args.toIntMinMax<uint8_t>(1, 0, Network::WiFi::kNumStations - 1, config.getWiFiConfigurationId());
                        auto &network = Network::Settings::getWriteableConfig();
                        auto &cfg = network.stations[num];
                        if (args.startsWithIgnoreCase(2, F("remove"))) {
                            args.print(F("removed SSID %s"), Network::WiFi::getSSID(num));
                            cfg.enabled = false;
                            Network::WiFi::setSSID(num, emptyString);
                            Network::WiFi::setPassword(num, emptyString);
                        }
                        else {
                            auto SSID = args.toString(3);
                            auto password = args.toString(4);
                            auto ip = args.toString(5);
                            cfg.enabled = args.isTrue(2);
                            if (StrView(ip).startsWithIgnoreCase(F("dhcp"))) {
                                cfg.dhcp = true;
                            }
                            else {
                                auto subnet = args.toString(6);
                                auto gateway = args.toString(7);
                                auto dns1 = args.toString(8);
                                auto dns2 = args.toString(9);
                                cfg.dhcp = false;
                                cfg.local_ip = IPAddress().fromString(ip);
                                cfg.subnet = IPAddress().fromString(subnet);
                                cfg.gateway = IPAddress().fromString(gateway);
                                cfg.dns1 = StrView(dns1).startsWithIgnoreCase(F("glob")) ? Network::Settings::kGlobalDNS : IPAddress().fromString(dns1);
                                cfg.dns2 = StrView(dns2).startsWithIgnoreCase(F("glob")) ? Network::Settings::kGlobalDNS : IPAddress().fromString(dns2);
                            }
                            Network::WiFi::setSSID(num, SSID);
                            Network::WiFi::setPassword(num, password);
                            network.activeNetwork = num;
                            config.write();

                            at_mode_print_WiFi_info(args, num, network.stations[num]);
                            // activate the connection (id) and reconnect
                            config.setWiFiConfigurationId(static_cast<Network::WiFi::StationConfigType>(num));
                            config.reconfigureWiFi(F("Reconfiguring WiFi adapter"));
                        }
                    }
                }
                break;
            case WiFiCommandsType::AP_ON:
                args.print(F("enabling AP mode"));
                WiFi.enableAP(true);
                break;
            case WiFiCommandsType::AP_OFF:
                args.print(F("disabling AP mode"));
                WiFi.enableAP(false);
                break;
            case WiFiCommandsType::AP_STBY: {
                    args.print(F("disabling AP mode (stand-by is enabled)"));
                    WiFi.enableAP(false);
                    auto &flags = System::Flags::getWriteableConfig();
                    flags.is_softap_standby_mode_enabled = true;
                    flags.is_softap_enabled = false;
                    config.write();
                    WiFiCallbacks::add(WiFiCallbacks::EventType::CONNECTION, KFCFWConfiguration::apStandbyModeHandler);
                }
                break;
            case WiFiCommandsType::DIAG:
                config.printDiag(args.getStream(), F("+WIFI: "));
                break;
            case WiFiCommandsType::AVAIL_ST_LIST: {
                    for(const auto &station: KFCConfigurationClasses::Network::WiFi::getStations(nullptr)) {
                        args.print(F("%02u: %-32.32s %03u %s"), station._id, station._SSID.c_str(), station._priority, mac2String(station._bssid).c_str());
                    }
                }
                break;
            case WiFiCommandsType::NEXT:
                config.setWiFiErrors(0xff - 2);
                config.registerWiFiError();
                args.print(F("switching WiFi network"));
                LoopFunctions::callOnce([]() {
                    config.reconfigureWiFi(nullptr);
                });
                break;
        }
    }
}

#if ENABLE_ARDUINO_OTA

// +AOTA=<start|stop>
// Start/stop Arduino OTA

void ATModeCommands::AOTACommand(AtModeArgs &args)
{
    auto &plugin = WebServer::Plugin::getInstance();
    if (args.equalsIgnoreCase(0, F("start"))) {
        args.print(F("starting ArduinoOTA..."));
        plugin.ArduinoOTAbegin();
    }
    #if 1
        else if (args.equalsIgnoreCase(0, F("stop"))) {
            args.print(F("stopping ArduinoOTA..."));
            plugin.ArduinoOTAend();
        }
        else {
            args.print(F("ArduinoOTA status:"));
            plugin.ArduinoOTADumpInfo(args.getStream());
        }
    #endif
}

#endif

#if __LED_BUILTIN_WS2812_NUM_LEDS

// +NEOPX=<pin>,<num>,<r>,<g>,<b>
// Set NeoPixel color for given pin

void ATModeCommands::NeoPixelCommand(AtModeArgs &args)
{
    // +neopx=16,3,25,0,0
    // +neopx=,,25,0,0
    if (args.requireArgs(3, 5)) {
        auto pin = args.toUint8(0, __LED_BUILTIN_WS2812_PIN);
        auto num = args.toIntMinMax<uint16_t>(1, 1, __LED_BUILTIN_WS2812_NUM_LEDS, __LED_BUILTIN_WS2812_NUM_LEDS);
        auto red = args.toUint8(2);
        auto green = args.toUint8(3, red);
        auto blue = args.toUint8(4, red);
        uint32_t color = (red << 16) | (green << 8) | blue;
        args.print(F("pin=%u num=%u color=#%06x"), pin, num, color);
        if (ledTimer) {
            delete ledTimer;
            ledTimer = nullptr;
        }
        digitalWrite(pin, LOW);
        pinMode(pin, OUTPUT);
        #if HAVE_FASTLED
            fill_solid(WS2812LEDTimer::_pixels, __LED_BUILTIN_WS2812_NUM_LEDS, CRGB(0));
            fill_solid(WS2812LEDTimer::_pixels, num, CRGB(color));
            FastLED.show();
        #else
            WS2812LEDTimer::_pixels.fill(0);
            WS2812LEDTimer::_pixels.fill(num, color);
            WS2812LEDTimer::_pixels.show();
        #endif
    }
}

#endif

#if __LED_BUILTIN != IGNORE_BUILTIN_LED_PIN_ID

// +LED=<slow,fast,flicker,off,solid,sos,pattern>,[,color=0xff0000|pattern=10110...][,pin]
// Set LED mode

void ATModeCommands::LEDCommand(AtModeArgs &args)
{
    if (args.requireArgs(1, 4)) {
        BlinkLEDTimer::BlinkType type = BlinkLEDTimer::BlinkType::INVALID;
        String mode = args.toString(0);
        auto delay = args.toUint16(2, 50);
        auto pin = args.toUint8(3, __LED_BUILTIN);
        if (mode.startsWith(F("pat"))) {
            #if BUILTIN_LED_NEOPIXEL
                if (pin == BlinkLEDTimer::NEOPIXEL_PIN) {
                    args.print(F("Pattern not supported with NeoPixel"));
                }
                else
            #endif
            {
                auto patternStr = args.toString(1);
                auto pattern = BlinkLEDTimer::Bitset();
                pattern.fromString(patternStr);
                args.print(F("pattern %s delay %u"), pattern.toString().c_str(), delay);
                BlinkLEDTimer::setPattern(pin, delay, std::move(pattern));
                //+led=pattern,111111111111111111111111111111111111111111111111111111,100
                //+led=pattern,1010,100
            }
        }
        else {
            // +LED=slow,200000,1000,10
            // +LED=slow,000000,1000,16
            // pwm 16 output 0
            // pwm 15 output 0
            // led off
            auto color = static_cast<int32_t>(args.toNumber(1, 0xff00ff, 16));
            if (__LED_BUILTIN == pin && !BlinkLEDTimer::isPinValid(pin)) {
                args.print(F("Invalid PIN"));
            }
            else {
                if (mode.equalsIgnoreCase(F("slow"))) {
                    BlinkLEDTimer::setBlink(pin, type = BlinkLEDTimer::BlinkType::SLOW, color);
                }
                else if (mode.equalsIgnoreCase(F("fast"))) {
                    BlinkLEDTimer::setBlink(pin, type = BlinkLEDTimer::BlinkType::FAST, color);
                }
                else if (mode.equalsIgnoreCase(F("flicker"))) {
                    BlinkLEDTimer::setBlink(pin, type = BlinkLEDTimer::BlinkType::FLICKER, color);
                }
                else if (mode.equalsIgnoreCase(F("solid"))) {
                    BlinkLEDTimer::setBlink(pin, type = BlinkLEDTimer::BlinkType::SOLID, color);
                }
                else if (mode.equalsIgnoreCase(F("sos"))) {
                    BlinkLEDTimer::setBlink(pin, type = BlinkLEDTimer::BlinkType::SOS, color);
                }
                else {
                    mode = F("OFF");
                    color = 0;
                    BlinkLEDTimer::setBlink(pin, type = BlinkLEDTimer::BlinkType::OFF);
                }
                args.print(F("LED pin=%u mode=%s type=%u color=0x%06x"), pin, mode.c_str(), type, color);
            }
        }
    }
}

#endif

// +PWM=<pin>,<input|input_pullup|waveform|level=0-1023>[,<frequency=100-40000Hz>[,<duration/ms>]]
// PWM output on PIN, min./max. level set it to LOW/HIGH

void ATModeCommands::PWMCommand(AtModeArgs &args)
{
    if (args.requireArgs(2, 7)) {
        auto pin = args.toUint8(0);
        if (args.equalsIgnoreCase(1, F("waveform"))) {
            #if ESP8266
                if (pin > 16) {
                    args.print(F("%u does not support waveform"), pin);
                }
                else {
                    uint32_t timeHighUS = args.toInt(2, ~0U);
                    uint32_t timeLowUS = args.toInt(3, ~0U);
                    uint32_t runTimeUS = args.toInt(4, 0);
                    uint32_t increment = args.toInt(5, 0);
                    auto delayTime = args.toUint32(6);
                    if (timeHighUS == ~0U || timeLowUS == ~0U) {
                        digitalWrite(pin, LOW);
                        #if ESP8266
                            stopWaveform(pin);
                        #endif
                        pinMode(pin, INPUT);
                        args.print(F("usage: <pin>,waveform,<high-time cycles>,<low-time cycles>[,<run-time cycles|0=unlimited>,<increment>,<delay ms>]"));
                    }
                    else {
                        digitalWrite(pin, LOW);
                        pinMode(pin, OUTPUT);
                        if (increment) {
                            args.print(F("pin=%u high=%u low=%u runtime=%u increment=%u delay=%u"), pin, timeHighUS, timeLowUS, runTimeUS, increment, delayTime);
                            uint32_t start = 0;
                            _Scheduler.add(Event::milliseconds(delayTime), true, [args, delayTime, pin, timeHighUS, timeLowUS, runTimeUS, increment, start](Event::CallbackTimerPtr timer) mutable {
                                start += increment;
                                if (start >= timeHighUS) {
                                    start = timeHighUS;
                                    timer->disarm();
                                }
                                startWaveformClockCycles(pin, start, timeLowUS, runTimeUS);
                                if (delayTime > 20) {
                                    args.print(F("pin=%u high=%u low=%u runtime=%u"), pin, start, timeLowUS, runTimeUS);
                                }
                            }, Event::PriorityType::TIMER);

                        } else {
                            args.print(F("pin=%u high=%u low=%u runtime=%u"), pin, timeHighUS, timeLowUS, runTimeUS);
                            startWaveformClockCycles(pin, timeHighUS, timeLowUS, runTimeUS);
                        }
                    }
                }
            #else
                args.print(F("ESP32 does not support waveform"));
            #endif
        }
        else if (args.equalsIgnoreCase(1, F("input"))) {
            digitalWrite(pin, LOW);
            pinMode(pin, INPUT);
            args.print(F("set pin=%u to INPUT"), pin);
        }
        else if (args.equalsIgnoreCase(1, F("input_pullup"))) {
            digitalWrite(pin, LOW);
            #if ESP8266
                pinMode(pin, pin == 16 ? INPUT_PULLDOWN_16 : INPUT_PULLUP);
                args.print(F("set pin=%u to %s"), pin, pin == 16 ? PSTR("INPUT_PULLDOWN_16") : PSTR("INPUT_PULLUP"));
            #else
                pinMode(pin, INPUT_PULLUP);
                args.print(F("set pin=%u to %s"), pin, PSTR("INPUT_PULLUP"));
            #endif
        }
        else {

            auto level = (uint16_t)args.toIntMinMax(1, 0, PWMRANGE, 0);
            if (level == 0) {
                if (args.isAnyMatchIgnoreCase(1, F("h|hi|high"))) {
                    level = PWMRANGE;
                }
                else if (args.isAnyMatchIgnoreCase(1, F("l|lo|low"))) {
                    level = 0;
                }
            }
            auto freq = (uint16_t)args.toIntMinMax(2, 100, 40000, 1000);
            auto duration = (uint16_t)args.toMillis(3);
            String durationStr;
            if (duration > 0 && duration < 10) {
                duration = 10;
            }

            auto type = PSTR("digitalWrite");

            pinMode(pin, OUTPUT);
            #if defined(ESP8266)
                analogWriteFreq(freq);
                analogWriteRange(PWMRANGE);
            #else
                freq = 0;
            #endif
            if (level == 0)  {
                digitalWrite(pin, LOW);
                freq = 0;
            }
            else if (level >= PWMRANGE - 1) {
                digitalWrite(pin, HIGH);
                level = 1;
                freq = 0;
            }
            else {
                type = PSTR("analogWrite");
                analogWrite(pin, level);
            }
            if (duration) {
                durationStr = PrintString(F(" for %ums"), duration);
            }
            if (freq == 0) {
                args.print(F("%s(%u, %u)%s"), type, pin, level, durationStr.c_str());
            }
            else {
                float cycle = (1000000 / (float)freq);
                float dc = cycle * (level / (float)PWMRANGE);
                args.print(F("%s(%u, %u) duty cycle=%.2f cycle=%.2fµs f=%uHz%s"), type, pin, level, dc, cycle, freq, durationStr.c_str());
            }
            if (duration) {
                auto &stream = args.getStream();
                _Scheduler.add(duration, false, [pin, &stream](Event::CallbackTimerPtr) mutable {
                    stream.printf_P(PSTR("+PWM: digitalWrite(%u, 0)\n"), pin);
                    digitalWrite(pin, LOW);
                }, Event::PriorityType::TIMER);
            }
        }
    }
}

// +PLG=<list|start|stop|add-blacklist|add|remove>[,<name>]
// Plugin management

void ATModeCommands::PLGCommand(AtModeArgs &args)
{
    if (args.requireArgs(1, 2)) {
        auto cmds = PSTR("list|start|stop|add-blacklist|add|remove");
        int cmd = stringlist_find_P_P(cmds, args.get(0), '|');
        __DBG_printf("cmd=%d arg0=%s cmds=%s", cmd, args.get(0), cmds);
        if (cmd == 0) {
            PluginComponents::RegisterEx::getInstance().dumpList(args.getStream());
            args.print(F("Blacklist=%s"), PluginComponent::getBlacklist());
        }
        else if (args.requireArgs(2, 2)) {
            PluginComponent *plugin = nullptr;
            plugin = PluginComponent::findPlugin(FPSTR(args.get(1)), false);
            if (!plugin) {
                args.print(F("Cannot find plugin '%s'"), args.get(1));
            }
            else {
                switch(cmd) {
                    case 1: // start
                        if (plugin->getSetupTime() == 0) {
                            args.print(F("Calling %s.setup()"), plugin->getName());
                            plugin->setSetupTime();
                            PluginComponents::DependenciesPtr deps(new PluginComponents::Dependencies());
                            plugin->setup(PluginComponent::SetupModeType::DEFAULT, deps);
                        }
                        else {
                            args.print(F("%s already running"), plugin->getName());
                        }
                        break;
                    case 2: // stop
                        if (plugin->getSetupTime() != 0) {
                            args.print(F("Calling %s.shutdown()"), plugin->getName());
                            plugin->shutdown();
                            plugin->clearSetupTime();
                        }
                        else {
                            args.print(F("%s not running"), plugin->getName());
                        }
                        break;
                    case 3:     // add-blacklist
                    case 4:     // add
                    case 5:     // remove
                        {
                            bool flag;
                            if (cmd == 5) {
                                flag = PluginComponent::removeFromBlacklist(plugin->getName());
                            }
                            else {
                                flag = PluginComponent::addToBlacklist(plugin->getName());
                            }
                            if (flag) {
                                config.write();
                            }
                            args.printf_P("Blacklist=%s action=%s", PluginComponent::getBlacklist(), flag ? SPGM(success, "success") : SPGM(failure));
                        } break;
                    default:
                        args.print(F("expected <%s>"), cmds);
                        break;
                }
            }
        }
    }
}

// +DLY=<milliseconds>
// Call delay(milliseconds)

void ATModeCommands::DelayCommand(AtModeArgs &args)
{
    auto delayTime = args.toMillis(0, 1, 3600 * 1000, 250, F("ms"));
    args.print(F("%ums"), delayTime);
    delay(delayTime);
}

// +CPU  (ESP32)
// Toggle displaying CPU usage
// +CPU=[<80|160>]  (ESP8266, core < 3.x)
// Set CPU speed
// +CPU?
// Display CPU speed

void ATModeCommands::CPUCommand(AtModeArgs &args)
{
    #if ESP32
        if (perfmon_start(&const_cast<Stream &>(args.getStream())) == ESP_OK) {
            args.print(F("started"));
        }
        else if (perfmon_stop() == ESP_OK) {
            args.print(F("stopped"));
        }
        else {
            args.print(F("unknown error"));
        }
    #elif defined(ESP8266) && (ARDUINO_ESP8266_MAJOR < 3)
        if (args.size() == 1) {
            auto speed = (uint8_t)args.toInt(0, ESP.getCpuFreqMHz());
            auto result = system_update_cpu_freq(speed);
            args.print(F("Set %d MHz = %d"), speed, result);
        }
        args.print(F("%d MHz"), ESP.getCpuFreqMHz());
    #endif
}

#if RTC_SUPPORT

// +RTC=[<set>]
// Set RTC time
// +RTC?
// Display RTC time

void ATModeCommands::RTCCommand(AtModeArgs &args)
{
    auto &output = args.getStream();
    if (args.empty()) {
        auto status = config.getRTCStatus();
        args.print(F("Time=" TIME_T_FMT ", rtc=" TIME_T_FMT ", lostPower=%u, status=%s"), time(nullptr), status.time, status.lostPower, status.toString());
        config.printRTCStatus(output, status);
        output.println();
    }
    else {
        bool res = config.setRTC(time(nullptr));
        args.print(F("Set=%u, rtc=" TIME_T_FMT), res, config.getRTCStatus().time);
    }
}
#endif

#if DEBUG

// +DUMP=[<dirty|config.name>]
// Display settings

void ATModeCommands::DumpCommand(AtModeArgs &args)
{
    auto &output = args.getStream();
    auto version = System::Device::getConfig().config_version;
    auto versionStr = SaveCrash::Data::FirmwareVersion(version).toString();
    args.print(F("stored config version 0x%08x, %s, dirty=%u"), version, versionStr.c_str(), config.isDirty());

    if (args.equalsIgnoreCase(0, F("dirty"))) {
        config.dump(output, true);
    }
    else if (args.equalsIgnoreCase(0, F("json"))) {
        config.exportAsJson(output, versionStr);
    }
    else if (args.size() == 1) {
        config.dump(output, false, args.toString(0));
    }
    else {
        config.dump(output);
    }
}

// +DUMPT
// Dump timers

void ATModeCommands::DumpTimersCommand(AtModeArgs &args)
{
    dumpTimers(args.getStream());
}

static inline void dumpFileSystemInfo(Print &output)
{
    FSInfo info;
    getFSInfo(info);
    output.printf_P(PSTR(
        "+FS: Block size           %d\n"
        "+FS: Max. open files      %d\n"
        "+FS: Max. path length     %d\n"
        "+FS: Page size            %d\n"
        "+FS: Total bytes          %d\n"
        "+FS: Used bytes           %d (%.2f%%)\n"),
        info.blockSize, info.maxOpenFiles, info.maxPathLength, info.pageSize, info.totalBytes, info.usedBytes, info.usedBytes * 100.0 / info.totalBytes
    );
}

// +DUMPFS
// Display file system information

void ATModeCommands::DumpFsCommand(AtModeArgs &args)
{
    dumpFileSystemInfo(args.getStream());
}

#if DEBUG_CONFIGURATION_GETHANDLE

// +DUMPH=[<log|panic|clear>]
// Dump configuration handles

void ATModeCommands::DumpHandlesCommand(AtModeArgs &args)
{
    if (args.toLowerChar(0) == 'c') {
        ConfigurationHelper::writeHandles(true);
    }
    else if (args.toLowerChar(0) == 'p') {
        ConfigurationHelper::setPanicMode(true);
    }
    else {
        ConfigurationHelper::dumpHandles(args.getStream(), args.toLowerChar(0) == 'l');
    }
}

#endif

// +METRICS
// Display system metrics

void ATModeCommands::MetricsCommand(AtModeArgs &args)
{
    #if 1
        args.print(F("Device name: %s"), System::Device::getName());
        #if ESP32
            args.print(F("Framework Arduino ESP32 " ARDUINO_ESP32_RELEASE));
            args.print(F("ESP-IDF version %s-dev version"), esp_get_idf_version());
        #elif ESP8266
            args.print(F("Framework Arduino ESP8266 " ARDUINO_ESP8266_RELEASE), ARDUINO_ESP8266_GIT_VER);
        #endif
        // args.print(F("Git describe: " KFCFW_GIT_DESCRIBE));
        #if defined(HAVE_GDBSTUB) && HAVE_GDBSTUB
        {
            String options;
            #if GDBSTUB_USE_OWN_STACK
            options += F("USE_OWN_STACK ");
            #endif
            #if GDBSTUB_BREAK_ON_EXCEPTION
            options += F("BREAK_ON_EXCEPTION ");
            #endif
            #if GDBSTUB_CTRLC_BREAK
            options += F("CTRLC_BREAK ");
            #endif
            #if GDBSTUB_REDIRECT_CONSOLE_OUTPUT
            options += F("REDIRECT_CONSOLE_OUTPUT ");
            #endif
            #if GDBSTUB_BREAK_ON_INIT
            options += F("BREAK_ON_INIT ");
            #endif

            args.print(F("GDBStub: %s"), StrWrapper(options).trim().c_str());
        }
        #endif
        args.print(F("Uptime: %u seconds / %s"), getSystemUptime(), formatTime(getSystemUptime(), true).c_str());
        args.print(F("Free heap/fragmentation: %u / %u"), ESP.getFreeHeap(), getHeapFragmentation());
        #if ARDUINO_ESP8266_MAJOR >= 3
            #ifdef UMM_HEAP_IRAM
                {
                    HeapSelectIram ephemeral;
                    args.print(F("Free IRAM: %u"), ESP.getFreeHeap());
                }
            #endif
            #if (UMM_NUM_HEAPS != 1)
                {
                    HeapSelectDram ephemeral;
                    args.print(F("Free DRAM: %u"), ESP.getFreeHeap());
                }
            #endif
        #endif

        #if ESP8266
            args.print(F("irom0.text: 0x%08x-0x%08x"), SECTION_FLASH_START_ADDR(irom0_text), SECTION_FLASH_END_ADDR(irom0_text));
        #endif
        args.print(F("FS: 0x%x-0x%x/%u"), SECTION_FLASH_START_ADDR(FS), SECTION_FLASH_END_ADDR(FS), SECTION_CALC_SIZE(FS));
        args.print(F("EEPROM: 0x%x-0x%x/%u"), SECTION_FLASH_START_ADDR(EEPROM), SECTION_FLASH_END_ADDR(EEPROM), SECTION_CALC_SIZE(EEPROM));
        args.print(F("SaveCrash: 0x%x-0x%x/%u"), SECTION_FLASH_START_ADDR(SAVECRASH), SECTION_FLASH_END_ADDR(SAVECRASH), SECTION_CALC_SIZE(SAVECRASH));
        args.print(F("NVS: 0x%x-0x%x/%u"), SECTION_FLASH_START_ADDR(NVS), SECTION_FLASH_END_ADDR(NVS), SECTION_CALC_SIZE(NVS));
        #ifdef SECTION_NVS2_START_ADDRESS
            args.print(F("NVS2: 0x%x-0x%x/%u"), SECTION_FLASH_START_ADDR(NVS2), SECTION_FLASH_END_ADDR(NVS2), SECTION_CALC_SIZE(NVS2));
        #endif
        #if ESP8266
            args.print(F("DRAM: 0x%08x-0x%08x/%u"), SECTION_DRAM_START_ADDRESS, SECTION_DRAM_END_ADDRESS, SECTION_DRAM_END_ADDRESS - SECTION_DRAM_START_ADDRESS);
            args.print(F("HEAP: 0x%08x-0x%08x/%u"), SECTION_HEAP_START_ADDRESS, SECTION_HEAP_END_ADDRESS, SECTION_HEAP_END_ADDRESS - SECTION_HEAP_START_ADDRESS);
            {
                int stackAddress = 0;
                args.print(F("Stack: 0x%08x-0x%08x/%u"), (uint32_t)&stackAddress, SECTION_STACK_END_ADDRESS, SECTION_STACK_END_ADDRESS - (uint32_t)&stackAddress);
            }
        #endif
        args.print(F("CPU frequency: %uMHz %u core(s)"), ESP.getCpuFreqMHz(), ESPGetChipCores());
        #if ESP32
            args.print(F("Chip model %s (%s)"), KFCFWConfiguration::getChipModel(), ESP.getChipModel());
            args.print(F("Flash size / mode: %s / %s"), formatBytes(ESP.getFlashChipSize()).c_str(), ESPGetFlashChipSpeedAndModeStr().c_str());
        #elif ESP8266
            args.print(F("Chip model %s"), KFCFWConfiguration::getChipModel());
            args.print(F("Flash size / vendor / mode: %s / %02x / %s"), formatBytes(ESP.getFlashChipRealSize()).c_str(), ESP.getFlashChipVendorId(), ESPGetFlashChipSpeedAndModeStr().c_str());
            args.print(F("SDK / Core: %s / %s"), ESP.getSdkVersion(), ESP.getFullVersion().c_str());
            args.print(F("Boot mode: %u, %u"), ESP.getBootVersion(), ESP.getBootMode());
        #endif
        args.print(F("Firmware size: %s"), formatBytes(ESP.getSketchSize()).c_str());
        args.print(F("Version (uint32): %s (0x%08x)"), SaveCrash::Data::FirmwareVersion().toString().c_str(), SaveCrash::Data::FirmwareVersion().__version);
        auto version = System::Device::getConfig().config_version;
        args.print(F("Config version 0x%08x, %s"), version, SaveCrash::Data::FirmwareVersion(version).toString().c_str());
        args.print(F("WiFiCallbacks: size=%u count=%u"), sizeof(WiFiCallbacks::Entry), WiFiCallbacks::getVector().size());
        args.print(F("LoopFunctions: size=%u count=%u"), sizeof(LoopFunctions::Entry), LoopFunctions::getVector().size());

        #if PIN_MONITOR
            PrintString tmp;
            PinMonitor::pinMonitor.printStatus(tmp);
            tmp.replace(F(HTML_S(br)), "\n");
            StrWrapper(tmp).rtrim('\n');
            args.print(tmp);
        #endif

    #endif
}

// +RTCM=<list|dump|clear|set|get|quickconnect>[,<id>[,<data>]]
// RTC memory access

void ATModeCommands::RtcMemoryCommand(AtModeArgs &args)
{
    if (args.requireArgs(1)) {
        auto &stream = args.getStream();
        auto memoryId = static_cast<RTCMemoryManager::RTCMemoryId>(args.toNumber<uint32_t>(1));
        if (args.equalsIgnoreCase(0, F("qc")) || args.equalsIgnoreCase(0, F("quickconnect"))) {
            #if ENABLE_DEEP_SLEEP
                config.storeQuickConnect(WiFi.BSSID(), WiFi.channel());
                config.storeStationConfig(WiFi.localIP(), WiFi.subnetMask(), WiFi.gatewayIP());
                args.print(F("Quick connect stored"));
            #else
                args.print(F("Quick connect not available"));
            #endif
        }
        else if (args.equalsIgnoreCase(0, F("list")) || args.equalsIgnoreCase(0, F("info"))) {
            auto rtc = RTCMemoryManager::readTime();
            args.print(F("RTC time=%u status=%s"), rtc.getTime(), rtc.getStatus());
            args.print(F("RTC memory ids:"));
            for(uint8_t i = static_cast<uint8_t>(RTCMemoryManager::RTCMemoryId::NONE) + 1; i < static_cast<uint8_t>(RTCMemoryManager::RTCMemoryId::MAX); i++) {
                stream.printf_P(PSTR("0x%02x      %s (%u)\n"), i, PluginComponent::getMemoryIdName(i), RTCMemoryManager::read(RTCMemoryManager::RTCMemoryId(i), nullptr, 0xff));
            }
        }
        else if (args.equalsIgnoreCase(0, F("remove")) || args.equalsIgnoreCase(0, F("del")) || args.equalsIgnoreCase(0, F("delete")) || args.equalsIgnoreCase(0, F("rem"))) {
            if (memoryId != RTCMemoryManager::RTCMemoryId::NONE) {
                uint32_t tmp;
                if (RTCMemoryManager::read(memoryId, &tmp, sizeof(tmp))) {
                    RTCMemoryManager::remove(memoryId);
                    args.print(F("0x%02x: removed"), memoryId);
                }
                else {
                    args.print(F("0x%02x: no data"), memoryId);
                }
            }
        }
        else if (args.equalsIgnoreCase(0, F("clr")) || args.equalsIgnoreCase(0, F("clear"))) {
            RTCMemoryManager::clear();
            args.print(F("cleared"));
        }
        else if (args.equalsIgnoreCase(0, F("set")) || args.equalsIgnoreCase(0, F("write"))) {
            uint32_t data[32];
            uint8_t lengthInBytes = 0;
            for (uint8_t i = 2; i < 32 + 2 && i < args.size(); i++) {
                data[lengthInBytes++] = args.toNumber(i, ~0U);
            }
            if (lengthInBytes == 0) {
                args.print(F("0x%02x: no data"), memoryId);
            }
            else {
                if (RTCMemoryManager::write(memoryId, &data, lengthInBytes)) {
                    args.print(F("0x%02x: written %u bytes"), memoryId, lengthInBytes);
                }
                else {
                    args.print(F("0x%02x: write error"), memoryId);
                }
            }
        }
        else if (args.equalsIgnoreCase(0, F("get")) || args.equalsIgnoreCase(0, F("read"))) {
            uint32_t data[32];
            auto lengthInBytes = RTCMemoryManager::read(memoryId, &data, sizeof(data));
            stream.printf_P(PSTR("0x%02x: length=%u cmd="), memoryId, lengthInBytes);
            if (lengthInBytes == 0) {
                args.print(F("remove,0x%02x"), memoryId);
            }
            else {
                stream.printf_P(PSTR("+RTCM=set,0x%02x,"), memoryId);
                for (uint8_t i = 0; i < lengthInBytes; i++) {
                    stream.printf_P(PSTR("0x%02x%c"), data[i], i == lengthInBytes - 1 ? '\n' : ',');
                }
            }
        }
        else if (args.equalsIgnoreCase(0, F("dump"))) {
            #if DEBUG
                auto result = RTCMemoryManager::dump(args.getStream(), memoryId);
                if (result == -1) {
                    args.print(F("0x%02x: dump error"), memoryId);
                }
                else if (result == 0) {
                    if (memoryId != RTCMemoryManager::RTCMemoryId::NONE) {
                        args.print(F("0x%02x: no data"), memoryId);
                    }
                    else {
                        args.print(F("no data"));
                    }
                }
            #else
                args.print(F("dump is not supported"))
            #endif
        }
        else {
            args.invalidArgument(0, F("list|set|remove|clear|dump|quickconnect"));
        }
    }
}

// +ATMODE=<1|0>
// Enable/disable AT Mode

void ATModeCommands::AtModeCommand(AtModeArgs &args)
{
    if (args.requireArgs(1, 1)) {
        if (args.isTrue(0)) {
            atMode.enable(&args.getStream());
        }
        else {
            atMode.disable(&args.getStream());
        }
    }
}

// +PANIC=[<address|wdt|hwdt|alloc>]
// Cause an exception by calling panic(), writing zeros to memory <address> or triggering the (hardware)WDT

void ATModeCommands::PanicCommand(AtModeArgs &args)
{
    if (args.equalsIgnoreCase(0, F("wdt"))) {
        args.print(F("starting a loop to trigger the WDT"));
        for(;;) {}
    }
    else if (args.equalsIgnoreCase(0, F("hwdt"))) {
        #if ESP8266
            ESP.wdtDisable();
            args.print(F("starting a loop to trigger the hardware WDT"));
            for(;;) {}
        #else
            args.print(F("not supported"));
        #endif
    }
    else if (args.equalsIgnoreCase(0, F("alloc"))) {
        uint32_t address = 0;
        args.print(F("writing zeros to memory @ 0x%08x (after malloc fails)"), address);
        delay(1000);
        while(malloc(4096)) {
        }
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wnonnull"
        memset((void *)address, 0, 2147483647);
        memset((void *)2147483647, 0, 2147483647);
        memset((void *)0, 0, 2147483647);
    }
    else if (args.size()) {
        auto address = args.toNumber<uint32_t>(0);
        args.print(F("writing zeros to memory @ 0x%08x"), address);
        delay(1000);
        memset((void *)address, 0, 2147483647);
        memset((void *)2147483647, 0, 2147483647);
        memset((void *)0, 0, 2147483647);
        #pragma GCC diagnostic pop
    }
    else {
        args.print(F("calling panic()"));
        panic();
    }
}

#endif

// command string definitions in PROGMEM

PROGMEM_STRING_DEF(RemarkCommandString, "REM");
#ifndef DISABLE_TWO_WIRE
    PROGMEM_STRING_DEF(Serial2WireTransmitCommandString, "I2CT");
    PROGMEM_STRING_DEF(Serial2WireAnswerCommandString, "I2CA");
    PROGMEM_STRING_DEF(Serial2WireReceiveCommandString, "I2CR");
    PROGMEM_STRING_DEF(I2CSetupCommandString, "I2CS");
    PROGMEM_STRING_DEF(I2CTMCommandString, "I2CTM");
    PROGMEM_STRING_DEF(I2CRQCommandString, "I2CRQ");
#endif
#if HAVE_I2CSCANNER
    PROGMEM_STRING_DEF(I2CScanForDevicesCommandString, "I2CSCAN");
#endif
PROGMEM_STRING_DEF(DeepSleepCommandString, "DSLP");
PROGMEM_STRING_DEF(ResetCommandString, "RST");
PROGMEM_STRING_DEF(LoadCommandString, "LOAD");
PROGMEM_STRING_DEF(StoreCommandString, "STORE");
PROGMEM_STRING_DEF(ImportCommandString, "IMPORT");
PROGMEM_STRING_DEF(FactoryCommandString, "FACTORY");
PROGMEM_STRING_DEF(FactoryStoreResetCommandString, "FSR");
#if defined(HAVE_NVS_FLASH)
    PROGMEM_STRING_DEF(NVSCommandString, "NVS");
#endif
PROGMEM_STRING_DEF(TouchCommandString, "TOUCH");
PROGMEM_STRING_DEF(MkdirCommandString, "MD");
PROGMEM_STRING_DEF(RemoveCommandString, "RM");
PROGMEM_STRING_DEF(RenameCommandString, "RN");
PROGMEM_STRING_DEF(ListCommandString, "LS");
PROGMEM_STRING_DEF(ListRecursiveCommandString, "LSR");
PROGMEM_STRING_DEF(CatCommandString, "CAT");
PROGMEM_STRING_DEF(WiFiCommandString, "WIFI");
#if ENABLE_ARDUINO_OTA
    PROGMEM_STRING_DEF(AOTACommandString, "AOTA");
#endif
#if __LED_BUILTIN_WS2812_NUM_LEDS
    PROGMEM_STRING_DEF(NeoPixelCommandString, "NEOPX");
#endif
#if __LED_BUILTIN != IGNORE_BUILTIN_LED_PIN_ID
    PROGMEM_STRING_DEF(LEDCommandString, "LED");
#endif
PROGMEM_STRING_DEF(PWMCommandString, "PWM");
PROGMEM_STRING_DEF(PLGCommandString, "PLG");
PROGMEM_STRING_DEF(DelayCommandString, "DLY");
#if DEBUG
    PROGMEM_STRING_DEF(HeapCommandString, "HEAP");
    PROGMEM_STRING_DEF(RssiCommandString, "RSSI");
    PROGMEM_STRING_DEF(GpioCommandString, "GPIO");
#endif
#if ESP32 || (defined(ESP8266) && (ARDUINO_ESP8266_MAJOR < 3))
    PROGMEM_STRING_DEF(CPUCommandString, "CPU");
#endif
#if RTC_SUPPORT
    PROGMEM_STRING_DEF(RTCCommandString, "RTC");
#endif
#if DEBUG
    PROGMEM_STRING_DEF(DumpCommandString, "DUMP");
    PROGMEM_STRING_DEF(DumpTimersCommandString, "DUMPT");
    PROGMEM_STRING_DEF(DumpFsCommandString, "DUMPFS");
    #if DEBUG_CONFIGURATION_GETHANDLE
        PROGMEM_STRING_DEF(DumpHandlesCommandString, "DUMPH");
    #endif
    PROGMEM_STRING_DEF(MetricsCommandString, "METRICS");
    PROGMEM_STRING_DEF(RtcMemoryCommandString, "RTCM");
    PROGMEM_STRING_DEF(AtModeCommandString, "ATMODE");
    PROGMEM_STRING_DEF(PanicCommandString, "PANIC");
#endif

// commands table in PROGMEM
// the help/arguments/query text is documented in a comment block above each command handler, see docs/AtModeHelp.md

static const ATModeCommands::Item PROGMEM ATModeCommandsTable[] = {
    ATModeCommands::Item(ATModeCommands::IgnoreCommand, SPGM(RemarkCommandString)),
    #ifndef DISABLE_TWO_WIRE
        ATModeCommands::Item(ATModeCommands::IgnoreCommand, SPGM(Serial2WireTransmitCommandString)),
        ATModeCommands::Item(ATModeCommands::IgnoreCommand, SPGM(Serial2WireAnswerCommandString)),
        ATModeCommands::Item(ATModeCommands::IgnoreCommand, SPGM(Serial2WireReceiveCommandString)),
        ATModeCommands::Item(ATModeCommands::I2CSetupCommand, SPGM(I2CSetupCommandString)),
        ATModeCommands::Item(ATModeCommands::I2CTransmitCommand, SPGM(I2CTMCommandString)),
        ATModeCommands::Item(ATModeCommands::I2CReceiveCommand, SPGM(I2CRQCommandString)),
    #endif
    #if HAVE_I2CSCANNER
        ATModeCommands::Item(ATModeCommands::I2CScanForDevicesCommand, SPGM(I2CScanForDevicesCommandString)),
    #endif
    ATModeCommands::Item(ATModeCommands::DeepSleepCommand, SPGM(DeepSleepCommandString)),
    ATModeCommands::Item(ATModeCommands::ResetCommand, SPGM(ResetCommandString)),
    ATModeCommands::Item(ATModeCommands::LoadCommand, SPGM(LoadCommandString)),
    ATModeCommands::Item(ATModeCommands::StoreCommand, SPGM(StoreCommandString)),
    ATModeCommands::Item(ATModeCommands::ImportCommand, SPGM(ImportCommandString)),
    ATModeCommands::Item(ATModeCommands::FactoryCommand, SPGM(FactoryCommandString)),
    ATModeCommands::Item(ATModeCommands::FactoryStoreResetCommand, SPGM(FactoryStoreResetCommandString)),
    #if defined(HAVE_NVS_FLASH)
        ATModeCommands::Item(ATModeCommands::NVSCommand, SPGM(NVSCommandString)),
    #endif
    ATModeCommands::Item(ATModeCommands::TouchCommand, SPGM(TouchCommandString)),
    ATModeCommands::Item(ATModeCommands::MkdirCommand, SPGM(MkdirCommandString)),
    ATModeCommands::Item(ATModeCommands::RemoveCommand, SPGM(RemoveCommandString)),
    ATModeCommands::Item(ATModeCommands::RenameCommand, SPGM(RenameCommandString)),
    ATModeCommands::Item(ATModeCommands::ListCommand, SPGM(ListCommandString)),
    ATModeCommands::Item(ATModeCommands::ListRecursiveCommand, SPGM(ListRecursiveCommandString)),
    ATModeCommands::Item(ATModeCommands::CatCommand, SPGM(CatCommandString)),
    ATModeCommands::Item(ATModeCommands::WiFiCommand, SPGM(WiFiCommandString)),
    #if ENABLE_ARDUINO_OTA
        ATModeCommands::Item(ATModeCommands::AOTACommand, SPGM(AOTACommandString)),
    #endif
    #if __LED_BUILTIN_WS2812_NUM_LEDS
        ATModeCommands::Item(ATModeCommands::NeoPixelCommand, SPGM(NeoPixelCommandString)),
    #endif
    #if __LED_BUILTIN != IGNORE_BUILTIN_LED_PIN_ID
        ATModeCommands::Item(ATModeCommands::LEDCommand, SPGM(LEDCommandString)),
    #endif
    ATModeCommands::Item(ATModeCommands::PWMCommand, SPGM(PWMCommandString)),
    ATModeCommands::Item(ATModeCommands::PLGCommand, SPGM(PLGCommandString)),
    ATModeCommands::Item(ATModeCommands::DelayCommand, SPGM(DelayCommandString)),
    #if DEBUG
        ATModeCommands::Item(ATModeCommands::HeapCommand, SPGM(HeapCommandString)),
        ATModeCommands::Item(ATModeCommands::RssiCommand, SPGM(RssiCommandString)),
        ATModeCommands::Item(ATModeCommands::GpioCommand, SPGM(GpioCommandString)),
    #endif
    #if ESP32
        ATModeCommands::Item(ATModeCommands::CPUCommand, SPGM(CPUCommandString)),
    #elif defined(ESP8266) && (ARDUINO_ESP8266_MAJOR < 3)
        ATModeCommands::Item(ATModeCommands::CPUCommand, SPGM(CPUCommandString)),
    #endif
    #if RTC_SUPPORT
        ATModeCommands::Item(ATModeCommands::RTCCommand, SPGM(RTCCommandString)),
    #endif
    #if DEBUG
        ATModeCommands::Item(ATModeCommands::DumpCommand, SPGM(DumpCommandString)),
        ATModeCommands::Item(ATModeCommands::DumpTimersCommand, SPGM(DumpTimersCommandString)),
        ATModeCommands::Item(ATModeCommands::DumpFsCommand, SPGM(DumpFsCommandString)),
        #if DEBUG_CONFIGURATION_GETHANDLE
            ATModeCommands::Item(ATModeCommands::DumpHandlesCommand, SPGM(DumpHandlesCommandString)),
        #endif
        ATModeCommands::Item(ATModeCommands::MetricsCommand, SPGM(MetricsCommandString)),
        ATModeCommands::Item(ATModeCommands::RtcMemoryCommand, SPGM(RtcMemoryCommandString)),
        ATModeCommands::Item(ATModeCommands::AtModeCommand, SPGM(AtModeCommandString)),
        ATModeCommands::Item(ATModeCommands::PanicCommand, SPGM(PanicCommandString)),
    #endif
};

bool ATModeCommands::handle(AtModeArgs &args)
{
    for(size_t i = 0; i < sizeof(ATModeCommandsTable) / sizeof(ATModeCommandsTable[0]); i++) {
        auto item = &ATModeCommandsTable[i];
        auto command = reinterpret_cast<const __FlashStringHelper *>(pgm_read_ptr(&item->command));
        if (args.isCommand(command)) {
            auto handler = reinterpret_cast<ATModeCommands::ATModeCommandCallback>(pgm_read_ptr(&item->handler));
            handler(args);
            return true;
        }
    }
    return false;
}
