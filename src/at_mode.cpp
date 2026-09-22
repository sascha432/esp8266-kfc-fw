/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include <../include/Syslog.h>
#include <ReadADC.h>
#include <EventScheduler.h>
#include <MicrosTimer.h>
#include <StreamString.h>
#include <BitsToStr.h>
#include <Cat.h>
#include <vector>
#include <queue>
#include <JsonTools.h>
#include <ListDir.h>
#include <DumpBinary.h>
#include "at_mode_commands.h"
#include "kfc_fw_config.h"
#include "logger.h"
#include "misc.h"
#include "deep_sleep.h"
#include "save_crash.h"
#include "web_server.h"
#include "web_socket.h"
#include "async_web_response.h"
#include "serial_handler.h"
#include "blink_led_timer.h"
#include "plugins.h"
#include "PinMonitor.h"
#include "../src/plugins/plugins.h"
#include <stl_ext/memory.h>
#include "HeapSelector.h"

#if __LED_BUILTIN_WS2812_NUM_LEDS
#    include <NeoPixelEx.h>
#endif

#if ESP8266
#    include <umm_malloc/umm_malloc.h>
     extern "C" {
#       include <umm_malloc/umm_local.h>
     }
#    include <core_esp8266_waveform.h>
#    include <core_version.h>
#    if ARDUINO_ESP8266_MAJOR == 0
#        error Invalid core config
#   endif
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
using KFCConfigurationClasses::MainConfig;

#include <AsyncWebSocket.h>

static constexpr bool kCommandParserModeAllowShortPrefix = true;
static constexpr bool kCommandParserModeAllowNoPrefix = true;

extern void __kfcfw_queue_monitor(AsyncWebSocketMessage *dataMessage, AsyncClient *_client, AsyncWebSocket *_server);

void __kfcfw_queue_monitor(AsyncWebSocketMessage *dataMessage, AsyncClient *_client, AsyncWebSocket *_server)
{
    #if 0
        Serial.printf_P(PSTR("+WSQ: count=%u size=%u [%u:%u]"), _server->_getQueuedMessageCount(), _server->_getQueuedMessageSize(), WS_MAX_QUEUED_MESSAGES, WS_MAX_QUEUED_MESSAGES_SIZE);
    #if WS_MAX_QUEUED_MESSAGES_MIN_HEAP
        Serial.printf_P(PSTR(" heap %u/%u [%u:%u]"), ESP.getFreeHeap(), WS_MAX_QUEUED_MESSAGES_MIN_HEAP, WS_MIN_QUEUED_MESSAGES, WS_MIN_QUEUED_MESSAGES_SIZE);
    #endif
        Serial.println();
    #endif
}

#if HAVE_I2CSCANNER
#   include "i2c_scanner.h"
#endif

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(HEAP, "HEAP", "[interval in seconds|0=disable]", "Display free heap");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(RSSI, "RSSI", "[interval in seconds|0=disable]", "Display WiFi RSSI");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(GPIO, "GPIO", "[interval in seconds|0=disable]", "Display GPIO states");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(ADC, "ADC", "<off|display interval=1s>[,<period=1s>,<multiplier=1.0>,<unit=mV>,<read delay=5000us>]", "Read the ADC and display values");

#if DEBUG

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(DUMPIO, "DUMPIO", "<address=0x60000000>[,<end address|length=4>]", "Dump IO memory");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(DUMPM, "DUMPM", "<address>[,<length=32>][,<insecure=false>,<use ESP.flashRead()=true>]", "Dump memory (32bit aligned)");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(DUMPF, "DUMPF", "<start=0x40200000>[,<end address|length=32>]", "Dump flash memory");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(FLASH, "FLASH", "<e[rase]>,<address>|<r[ead]>,<address>[,<offset=0>,<length=4096>]|w[rite],<address>,<byte1>[,<byte2>[,...]]]", "Erase, read or write flash memory");
#if LOGGER
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(LOGDBG, "LOGDBG", "<1|0>", "Enable/disable writing debug output to log://debug");
#endif

#endif


class DisplayTimer;

extern DisplayTimer *displayTimer;

#if 1

class DisplayTimer {
public:
    enum class DisplayType {
        HEAP = 1,
        HEAP_UMM,
        RSSI,
        GPIO,
    };

    DisplayTimer() :
        _type(DisplayType::HEAP)
    {
        __DBG_assertf(displayTimer == nullptr, "displayTimer not null");
        stdex::reset(displayTimer, this);
    }

    ~DisplayTimer()
    {
        if (this == displayTimer) {
            displayTimer = nullptr;
        }
    }

    void setType(DisplayType type, Event::milliseconds interval)
    {
        _type = type;
        if (_type == DisplayType::HEAP || _type == DisplayType::HEAP_UMM) {
            LOOP_FUNCTION_ADD(loop);
        }
        else {
            LoopFunctions::remove(loop);
        }
        _Timer(_timer).add(interval, true, DisplayTimer::printTimerCallback);
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

    DisplayType getType() const
    {
        return _type;
    }

    void printHeap()
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

    void printGPIO()
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
                Serial.printf_P(PSTR("%u=%u%c"), i, digitalRead(i), (i == NUM_DIGITAL_PINS - 1) ? '\n' : ' ');
            }
            // Serial.println(PinMonitor::GPIO::read(), 2);
            // static const uint8_t pins[] PROGMEM = {36, 39};
            // auto ptr = pins;
            // for(uint8_t i = 0; i < sizeof(pins); i++) {
            //     auto pin = pgm_read_byte(ptr++);
            //     Serial.printf_P(PSTR(" A%u=%u"), pin, analogRead(pin));

            // }
            // Serial.println();

        #endif
        #if defined(HAVE_IOEXPANDER)
            IOExpander::config.dumpPins(Serial);
        #endif
    }

    void printRSSI()
    {
        int16_t rssi = WiFi.RSSI();
        _rssiMin = std::max(_rssiMin, rssi);
        _rssiMax = std::min(_rssiMax, rssi);
        Serial.printf_P(PSTR("+RSSI: %d dBm (min/max %d/%d)\n"), rssi, _rssiMin, _rssiMax);
    }

    void print()
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

    static void printTimerCallback(Event::CallbackTimerPtr timer)
    {
        if (displayTimer == nullptr) {
            timer->disarm();
            return;
        }
        displayTimer->print();
    }

    bool removeTimer()
    {
        return _Timer(_timer).remove();
    }

    void remove()
    {
        _Timer(_timer).remove();
        LoopFunctions::remove(loop);
        delete this;
    }

    void _loop()
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

    static void loop()
    {
        if (displayTimer) {
            displayTimer->_loop();
        }
    }

private:
    Event::Timer _timer;
    DisplayType _type;
    int16_t _rssiMin;
    int16_t _rssiMax;
    uint32_t _maxHeap;
    uint32_t _minHeap;
    #if ESP8266
        uint32_t _maxIram;
        uint32_t _minIram;
    #endif
};

DisplayTimer *displayTimer;

static void print_heap()
{
    if (displayTimer) {
        displayTimer->printHeap();
    }
    else {
        Serial.printf_P(PSTR("+HEAP: free=%u cpu=%dMHz frag=%u"), ESP.getFreeHeap(), ESP.getCpuFreqMHz(), ESP.getHeapFragmentation());
    }
}

#endif

void at_mode_wifi_callback(WiFiCallbacks::EventType event, void *payload)
{
    if (event == WiFiCallbacks::EventType::CONNECTED) {
        Serial.printf_P(PSTR("WiFi connected to %s - IP %s\n"), WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
    }
    else if (event == WiFiCallbacks::EventType::DISCONNECTED) {
        Serial.println(F("WiFi connection lost"));
    }
}

SerialHandler::Client *_client;
bool is_at_mode_enabled;

bool at_mode_enabled()
{
    return is_at_mode_enabled;
}

void at_mode_setup()
{
    is_at_mode_enabled = System::Flags::getConfig().is_at_mode_enabled;
    __LDBG_printf("AT_MODE_ENABLED=%u", is_at_mode_enabled);

    if (_client) {
        serialHandler.removeClient(*_client);
    }
    __LDBG_printf("installing serial handler");
    _client = &serialHandler.addClient(at_mode_serial_input_handler, SerialHandler::EventType::READ);

    __LDBG_printf("adding wificallback");
    WiFiCallbacks::add(WiFiCallbacks::EventType::CONNECTION, at_mode_wifi_callback);
    __LDBG_printf("setup done");
}

void enable_at_mode(Stream *output)
{
    if (!is_at_mode_enabled) {
        if (output) {
            output->println(F("Enabling AT MODE."));
        }
        is_at_mode_enabled = true;
        // if (!_client) {
        //     _client = &serialHandler.addClient(at_mode_serial_input_handler, SerialHandler::EventType::READ);
        // }
        // _client->start(SerialHandler::EventType::READ);
    }
}

void disable_at_mode(Stream *output)
{
    if (is_at_mode_enabled) {
        // if (_client) {
        //     serialHandler.removeClient(*_client);
        //     _client = nullptr;
        // }
        if (output) {
            output->println(F("Disabling AT MODE."));
        }
        #if DEBUG
            if (displayTimer) {
                displayTimer->remove();
            }
        #endif
        is_at_mode_enabled = false;
    }
}

void at_mode_print_help(Stream &output)
{
    output.println(F("try https://github.com/sascha432/esp8266-kfc-fw/blob/master/docs/AtModeHelp.md\n"));
    if (config.isSafeMode()) {
        output.println(F("SAFE MODE ENABLED"));
    }
}

void at_mode_print_invalid_command(Stream &output)
{
    output.print(F("ERROR - Invalid command. "));
    at_mode_print_help(output);
}

void at_mode_print_invalid_arguments(Stream &output, uint16_t num, uint16_t min, uint16_t max)
{
    static constexpr uint16_t UNSET = ~0;
    output.print(F("ERROR - "));
    if (min != UNSET) {
        if (min == max || max == UNSET) {
            output.printf_P(PSTR("Expected %u argument(s), got %u\n"), min, num);
        }
        else {
            output.printf_P(PSTR("Expected %u to %u argument(s), got %u\n"), min, max, num);
        }
    }
    else {
        output.println(F("Invalid arguments"));
    }
    at_mode_print_help(output);
}

void at_mode_print_prefix(Stream &output, const __FlashStringHelper *command)
{
    output.print('+');
    output.print(command);
    output.print(F(": "));
}

void at_mode_print_prefix(Stream &output, const char *command)
{
    output.printf_P(PSTR("+%s: "), command);
}

static void at_mode_adc_loop();
static void at_mode_adc_delete_object();

class AtModeADC {
public:
    AtModeADC() : _adcIntegralMultiplier(0), _adc(ADCManager::getInstance()) {
    }
    virtual ~AtModeADC() {
        _Timer(_displayTimer).remove();
        if (_adcIntegralMultiplier) {
            LoopFunctions::remove(at_mode_adc_loop);
        }
    }

    // useable values for integrating the average value is 1-100 (averagePeriodMillis)
    bool init(uint16_t averagePeriodMillis, float convertMultiplier, const String &unit, uint32_t readDelayMicros = 5000) {
        _Timer(_displayTimer).remove();
        if (!averagePeriodMillis) {
            return false;
        }
        _unit = unit;
        _convMultiplier = convertMultiplier;
        _adcIntegralMultiplier = 1.0 / (1000.0 / averagePeriodMillis);
        _adcIntegral = 0;
        _adcIntegral2 = 0;
        _timerSum = 0;
        _timerCount = 0;
        _lastUpdate = 0;
        _readDelay = readDelayMicros;
        LOOP_FUNCTION_ADD(at_mode_adc_loop);
        _timer.start();
        return true;
    }

    String getConvertedString() const {
        int prec = 0;
        prec = (_convMultiplier >= 10) ? 0 : ((_convMultiplier < 0.1) ? 6 : (_convMultiplier < 1) ? 3 : 1);
        return PrintString(F("%.*f%s"), prec, _adcIntegral * _convMultiplier, _unit.c_str());
    }

    uint16_t getValue() const {
        return round(_adcIntegral);
    }

    void printInfo(Print &output) {
        output.printf_P("value=%.1f value2=%.1f time=%uus (avg) samples=%u multiplier=%.6f\n", _adcIntegral, _adcIntegral2, _timerSum / _timerCount, _timerCount, _adcIntegralMultiplier);
        _adcIntegral2 = 0;
        _timerSum = 0;
        _timerCount = 0;
    }

private:
    virtual void processData(uint16_t reading, uint32_t diff, uint32_t micros) {
        float count = diff * _adcIntegralMultiplier;
        _adcIntegral = ((count * _adcIntegral) + (double)reading) / (count + 1.0);
        _adcIntegral2 = ((1.0 * _adcIntegral2) + (double)reading) / 2.0;
        _timerSum += diff;
        _timerCount++;
        if (_timerSum > 30 * 1000 * 1000) {
            _timerSum /= 2;
            _timerCount /= 2;
        }
    }

public:
    void loop() {
        auto time = micros();
        auto diff = _timer.getTime(time);
        if (diff < _readDelay) {
            return; // wait for the next loop
        }
        uint32_t tmp;
        auto reading = _adc.readValue(tmp);
        if (tmp == _lastUpdate) {
            return;
        }
        _lastUpdate = tmp;

        _timer.start();
        processData(reading, diff, time);
    }

    Event::Timer &getTimer() {
        return _displayTimer;
    }

protected:
    union {
        struct {
            float _convMultiplier;
            float _adcIntegral;
            float _adcIntegral2;
            uint32_t _timerSum;
            uint32_t _timerCount;
        };
        struct {
            uint32_t duration;
            uint32_t start;
            uint16_t packetSize;
            uint32_t sent;
            uint32_t dropped;
        } _webSocket;
    };
    uint32_t _readDelay;
    uint32_t _lastUpdate;
    float _adcIntegralMultiplier;
    String _unit;
    MicrosTimer _timer;
    Event::Timer _displayTimer;
    ADCManager &_adc;
};

class AtModeADCWebSocket : public AtModeADC
{
public:
    using AtModeADC::AtModeADC;

    bool init(uint32_t interval, uint32_t duration, uint16_t packetSize, AsyncWebSocketClient *client) {
        _Timer(_displayTimer).remove();
        if (!interval) {
            return false;
        }
        _adcIntegralMultiplier = 1;
        _webSocket.duration = duration;
        _webSocket.start = micros() / 1000U;
        _webSocket.packetSize = packetSize;
        _webSocket.dropped = 0;
        _webSocket.sent = 0;
        _client = client;

        resetBuffer();

        _lastUpdate = 0;
        _readDelay = interval;
        LOOP_FUNCTION_ADD(at_mode_adc_loop);
        _timer.start();
        return true;
    }

private:
    typedef struct __attribute__packed__ {
        uint32_t _time: 22;
        uint32_t _value: 10;
        uint16_t _value2;
    } Data_t;

    typedef struct __attribute__packed__ {
        WsClient::BinaryPacketType type;
        uint16_t flags;
        uint8_t packet_size;
    } Header_t;

    static constexpr uint16_t FLAGS_NONE = 0x0000;
    static constexpr uint16_t FLAGS_LAST_PACKET = 0x0001;
    static constexpr uint16_t FLAGS_DROPPED_PACKETS = 0x0002;

    void resetBuffer()
    {
        _buffer.setLength(0);
        _buffer.reserve(_webSocket.packetSize + 1);
        Header_t header = { WsClient::BinaryPacketType::ADC_READINGS, FLAGS_NONE, sizeof(Data_t) };
        _buffer.push_back(header);
    }

    virtual void processData(uint16_t reading, uint32_t diff, uint32_t micros) override
    {
        Data_t data;
        uint32_t ms = micros / 1000U;
        auto time = get_time_since(_webSocket.start, ms);

        if (_buffer.length() + sizeof(data) >= _webSocket.packetSize) {

            bool endReading = (time > _webSocket.duration);

            if (Http2Serial::getClientById(_client)) {
                if (_client->canSend()) {
                    _buffer.write(0); // terminate with NUL byte
                    size_t len = _buffer.length() - 1;
                    auto ptr = _buffer.get();
                    _webSocket.sent += len;

                    if (len >= sizeof(Header_t)) {
                        auto flags = FLAGS_NONE;
                        if (endReading) {
                            flags |= FLAGS_LAST_PACKET;
                        }
                        if (_webSocket.dropped) {
                            flags |= FLAGS_DROPPED_PACKETS;
                        }
                        ((Header_t *)&ptr[0])->flags |= flags;
                    }

                    auto wsBuffer = _client->server()->makeBuffer(ptr, len);
                    if (wsBuffer) {
                        _client->binary(wsBuffer);
                    }
                }
                else {
                    _webSocket.dropped += _buffer.length();
                }
            }

            if (endReading) {
                _buffer.clear();
                Serial.printf_P(PSTR("+ADC: Finished client=%p %u bytes @ %.2fKB/s%s\n"),
                    _client,
                    _webSocket.sent,
                    _webSocket.sent * (1000.0 / 1024.0) / _webSocket.duration,
                    _webSocket.dropped ? PrintString(F(" %u bytes dropped"), _webSocket.dropped).c_str() : emptyString.c_str()
                );
                LoopFunctions::remove(at_mode_adc_loop);
                LoopFunctions::callOnce(at_mode_adc_delete_object);
                return;
            }

            resetBuffer();
        }

        data._time = time;
        data._value = std::min(reading, (uint16_t)1023);
        #if IOT_BLINDS_CTRL
            data._value2 = BlindsControlPlugin::getInstance().getCurrent();
        #else
            data._value2 = 0;
        #endif

        _buffer.push_back(data);
    }

private:
    Buffer _buffer;
    AsyncWebSocketClient *_client;
};

AtModeADC *atModeADC;

static void at_mode_adc_delete_object()
{
    stdex::reset(atModeADC);
}

static void at_mode_adc_loop()
{
    if (atModeADC) {
        atModeADC->loop();
    }
}

static bool tokenizerCmpFuncCmdLineMode(char ch, int type)
{
    if (type == 1) { // command separator
        if (ch == ' ') {
            return true;
        }
    }
    else if (type == 2 && ch == ';') { // new command / new line separator
        return true;
    }
    else if (type == 3 && ch == '"') { // quotes
        return true;
    }
    else if (type == 4 && ch == '\\') { // escape character
        return true;
    }
    else if (type == 5 && ch == ' ') { // token separator
        return true;
    }
    else if (type == 6 && isspace(ch)) { // leading whitespace outside quotes
        return true;
    }
    return false;
}

#if DEBUG

static bool _writeAndVerifyFlash(uint32_t address, uint8_t *data, size_t size, uint8_t *compare, const AtModeArgs &args)
{
    if (ESP.flashWrite(address, data, size) == false) {
        args.print(F("flash write error address=%08x length=%u"), address, size);
        return false;
    }
    if (ESP.flashRead(address, compare, size) == false) {
        args.print(F("flash read error address=%08x length=%u"), address, size);
        return false;
    }
    if (memcmp(data, compare, size) != 0) {
        args.print(F("flash verify error address=%08x length=%u"), address, size);
        return false;
    }
    return true;
}

static uintptr_t translateAddress(String str) {
    str.trim('_');
    #if !ESP32
        if (str.equalsIgnoreCase(F("text")) || str.equalsIgnoreCase(F("irom0_text_start"))) {
            return (uintptr_t)&_irom0_text_start;
        }
        else if (str.equalsIgnoreCase(F("irom0_text_end"))) {
            return (uintptr_t)&_irom0_text_start;
        }
        else if (str.startsWithIgnoreCase(F("heap"))) {
            return (uintptr_t)&_heap_start;
        }
        else if (str.startsWithIgnoreCase(F("fs_s")) || str.equalsIgnoreCase(F("fs"))) {
            return (uintptr_t)&_FS_start;
        }
        else if (str.startsWithIgnoreCase(F("fs_e"))) {
            return (uintptr_t)&_FS_end;
        }
        else if (str.startsWithIgnoreCase(F("nvs"))) {
            return (uintptr_t)&_NVS_start;
        }
        else if (str.startsWithIgnoreCase(F("nvs_e"))) {
            return (uintptr_t)&_NVS_end;
        }
        #ifdef SECTION_NVS2_START_ADDRESS
            else if (str.startsWithIgnoreCase(F("nvs2"))) {
                return (uintptr_t)&_NVS2_start;
            }
            else if (str.startsWithIgnoreCase(F("nvs2_e"))) {
                return (uintptr_t)&_NVS2_end;
            }
        #endif
        else if (str.startsWithIgnoreCase(F("savecrash"))) {
            return (uintptr_t)&_SAVECRASH_start;
        }
        else if (str.startsWithIgnoreCase(F("savecrash_e"))) {
            return (uintptr_t)&_SAVECRASH_end;
        }
        else if (str.startsWithIgnoreCase(F("ee"))) {
            return (uintptr_t)&_EEPROM_start;
        }
        else if (str.equalsIgnoreCase(F("gpi"))) {
            return (uintptr_t)&GPI;
        }
        else if (str.startsWithIgnoreCase(F("gpo"))) {
            return (uintptr_t)&GPO;
        }
    #endif
    return (uintptr_t)~0;
}

#endif

void at_mode_serial_handle_event(String &commandString)
{
    auto &output = Serial;
    char *nextCommand = nullptr;
    AtModeArgs args(output);
    bool atModeCommands = true;

    // determine if query mode by checking for a trailing '?'
    commandString.trim();
    bool isQueryMode = commandString.endsWith('?');

    // __dump_binary_to(output, commandString.c_str(), commandString.length(), 16, nullptr, 4);

    // check command prefix
    if (commandString.startsWithIgnoreCase(F("AT"))) {
        // remove AT from the command
        commandString.remove(0, 2);
    }
    else if (kCommandParserModeAllowShortPrefix && commandString.startsWith('+')) {
        // allow using AT+COMMAND[?|=<args,...>] and +COMMAND[?|=<args,...>]
    }
    else if (kCommandParserModeAllowNoPrefix) {
        // allow using COMMAND [<arg1>][ <args...>] (or /COMMAND, --COMMAND -COMMAND)
        atModeCommands = false;
    }
    else {
        // display invalid command if it does not start with the allowed prefix
        at_mode_print_invalid_command(output);
        return;
    }

    // check for empty commands
    if (commandString.length() == 0) { // AT
        if (atModeCommands) { // display OK for empty AT commands and ignore empty lines
            args.ok();
        }
        return;
    }

    auto command = commandString.begin();
    // remove leading '+'
    if (*command == '+') {
        command++;
    }
    // removing leading '/', '-' and '--'
    else if (kCommandParserModeAllowNoPrefix && !atModeCommands) {
        if (*command == '/') {
            command++;
        }
        else if (*command == '-') {
            command++;
            if (*command == '-') {
                command++;
            }
        }
    }

    if (isQueryMode) {
        // remove trailing ?
        *strrchr(command,  '?') = 0;
        args.setQueryMode(true);
    }
    else if (atModeCommands) {
        // run tokenizer in AT command mode
        // command=arg1,arg2,arg3;command2=...
        __LDBG_printf("tokenizer('%s')", command);
        args.setQueryMode(false);
        tokenizer(command, args, true, &nextCommand);
        auto argsStr = implode(F("' '"), args.getArgs());
        __LDBG_printf("cmd=%s,argc=%d,args='%s',next_cmd='%s'", command, args.size(), argsStr.c_str(), __S(nextCommand));
    }
    else if __CONSTEXPR17 (kCommandParserModeAllowNoPrefix) {
        // run tokenizer in cli mode
        // command arg1 arg2 arg3 ; command2 ...

        args.setQueryMode(false);
        tokenizer(command, args, true, &nextCommand, tokenizerCmpFuncCmdLineMode);
        auto argsStr = implode(F("' '"), args.getArgs());
        __LDBG_printf("cmd=%s,argc=%u,args='%s',next_cmd='%s'", __S(command), args.size(), __S(implode(F("' '"), args.getArgs())), __S(nextCommand));
    }
    // store copy of command
    args.setCommand(command);

    if (ATModeCommands::handle(args)) {

    }else

    #if ESP8266 && DEBUG
        if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(DUMPIO))) {
/*
+dumpio=0x700,0x7ff
+dumpio=0x1200,0x1300
+dumpio=GPI
*/
            static constexpr auto kIOBase = 0x60000000U; // std::addressof(ESP8266_REG(0));
            uintptr_t addr = translateAddress(args.toString(0));
            if (addr == ~0U) {
                addr = args.toNumber(0, kIOBase);
            }
            if (addr < kIOBase) {
                addr += kIOBase;
            }
            uintptr_t toAddr = args.toNumber(1, sizeof(uint32_t));
            if (toAddr == 0) {
                toAddr = sizeof(uint32_t);
            }
            if (toAddr < addr) {
                toAddr += addr;
            }
            addr &= 0xffff;
            toAddr &= 0xffff;
            if (addr & 0x3)  {
                args.print(F("address=0x%08x not aligned"), addr);
            }
            else {
                while(addr < toAddr) {
                    auto data = ESP8266_REG(addr);
                    uint8_t len;
                    switch(toAddr - addr) {
                        case 1:
                            data &= 0xff;
                            len = 2;
                            break;
                        case 2:
                            data &= 0xffff;
                            len = 4;
                            break;
                        case 3:
                            data &= 0xffffff;
                            len = 6;
                            break;
                        default:
                            len = 8;
                            break;
                    }
                    Serial.printf_P(PSTR("address=0x%0*.*x data=0x%0*.*x %u %d %s\n"), len, len, addr + kIOBase, len, len, data, data, data, BitsToStr<32, false>(data).c_str() + ((8 - len) * 4));
                    addr += sizeof(uint32_t);
                    delay(1);
                }
            }
        }
        else
    #endif
    #if DEBUG
        if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(DUMPF))) {
            static constexpr size_t kFlashBufferSize = 32;
            uintptr_t addr = translateAddress(args.toString(0));
            if (addr == ~0U) {
                addr = args.toNumber(0, SECTION_FLASH_START_ADDRESS);
            }
            uintptr_t toAddr = args.toNumber(1, 32U);
            if (!toAddr) {
                addr -= SECTION_FLASH_START_ADDRESS;
                toAddr = addr + kFlashBufferSize;
            }
            else if (toAddr >= addr) { // length or address?
                addr -= SECTION_FLASH_START_ADDRESS;
                toAddr -= SECTION_FLASH_START_ADDRESS;
                if (addr == toAddr) {
                    toAddr += sizeof(4);
                }
            }
            else {
                addr -= SECTION_FLASH_START_ADDRESS;
                toAddr += addr;
            }
            uint8_t buf[kFlashBufferSize];
            std::fill_n(buf, sizeof(buf), 0xff);

            auto &stream = args.getStream();
            uint32_t start = millis();
            while(addr < toAddr) {
                if ((millis() - start) > 5000) {
                    stream.println(F("timeout..."));
                    break;
                }
                uint16_t len = toAddr - addr;
                if (len > sizeof(buf)) {
                    len = sizeof(buf);
                }
                auto result = ESP.flashRead(addr, buf, len);
                if (result) {
                    DumpBinary(stream, DumpBinary::kGroupBytesDefault, sizeof(buf), addr + SECTION_FLASH_START_ADDRESS).dump(buf, len);
                }
                else {
                    stream.printf_P(PSTR("address=0x%08x offset=%u len=%u read error\n"), addr + SECTION_FLASH_START_ADDRESS, addr, len);
                    break;
                }
                addr += len;
                delay(1);
            }
        }
        else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(FLASH))) {
            // +flash=<e[rase]>,<address>|<r[ead]>,<address>[,<offset=0>,<length=4096>]|w[rite],<address>,<byte1>[,<byte2>[,...]]]>
    /*

    +flash=r,0x405ab000,0,32
    +flash=r,0x405ab000,0,256
    +flash=e,0x405ab000
    +flash=w,0x405ab000,0,1,2,3,4,5,6,7,8
    +flash=r,0x405ab000,0,8


    // EEPROM
    +flash=r,EEPROM,0,1024
    +flash=r,EEPROM,0,128

    +flash=e,EEPROM

    */
            auto cmdStr = args.get(0);
            if (*cmdStr) {
                int cmd = stringlist_ifind_P(F("erase,e,read,r,write,w"), cmdStr);
                uintptr_t addr = translateAddress(args.toString(1));
                if (addr == ~0U) {
                    addr = static_cast<uint32_t>(args.toNumber(1, SECTION_FLASH_START_ADDRESS));
                }
                auto offset = static_cast<uint32_t>(args.toNumber(2, 0));
                addr += offset;
                auto length = static_cast<uint32_t>(args.toNumber(3, SPI_FLASH_SEC_SIZE));
                uint16_t sector = (addr - SECTION_FLASH_START_ADDRESS) / SPI_FLASH_SEC_SIZE;
                // recalculate address and offset
                offset = (addr - SECTION_FLASH_START_ADDRESS) - (sector * SPI_FLASH_SEC_SIZE);
                addr = (sector * SPI_FLASH_SEC_SIZE);
                bool rc = true;
                static constexpr size_t kFlashBufferSize = 32;
                switch(cmd) {
                    case 0: // erase
                    case 1: // e
                        args.print(F("erasing sector %u [%08X]"), sector, addr + SECTION_FLASH_START_ADDRESS);
                        if ((rc = ESP.flashEraseSector(sector)) == false) {
                            args.print(F("erase failed"));
                        }
                        break;
                    case 2: // read
                    case 3: // r
                        {
                            args.print(F("reading sector %u address 0x%08x offset %u length %u"), sector, addr + SECTION_FLASH_START_ADDRESS, offset, length);
                            uint32_t start = addr + offset;
                            uint32_t end = start + length;
                            uint8_t buf[kFlashBufferSize];
                            std::fill_n(buf, sizeof(buf), 0xff);
                            while (start < end) {
                                uint8_t len = std::min<size_t>(sizeof(buf), end - start);
                                if ((rc = ESP.flashRead(start, buf, len)) == false) {
                                    args.print(F("read error address=0x%08x length=%u"), start, length);
                                    break;
                                }
                                DumpBinary(args.getStream(), DumpBinary::kGroupBytesDefault, sizeof(buf), start + SECTION_FLASH_START_ADDRESS).dump(buf, len);
                                start += len;
                                delay(1);
                            }
                        }
                        break;
                    case 4: // write
                    case 5: // w
                        {
                            if (args.size() <= 3) {
                                args.print(F("no data to write"));
                            }
                            else {
                                length = args.size() - 3;
                                args.print(F("writing sector %u (0x%08x) offset %u length %u"), sector, addr + SECTION_FLASH_START_ADDRESS, offset, length);
                                uint16_t position = 0;
                                auto &stream = args.getStream();
                                auto data = std::unique_ptr<uint8_t[]>(new uint8_t[kFlashBufferSize]());
                                auto compare = std::unique_ptr<uint8_t[]>(new uint8_t[kFlashBufferSize]());
                                uint32_t start = addr + offset;
                                if (data && compare) {
                                    rc = 0;
                                    auto ptr = data.get();
                                    for(uint16_t i = 3; i < args.size(); i++) {
                                        if (i == 3) {
                                            stream.printf_P(PSTR("[%08X] "), start + SECTION_FLASH_START_ADDRESS);
                                        }
                                        auto value = static_cast<uint8_t>(args.toNumber(i, 0xff));
                                        *ptr++ = value;
                                        stream.printf_P(PSTR("%02x "), value);
                                        // once the buffer is full, write and verify
                                        if (++position % kFlashBufferSize == 0) {
                                            stream.println();
                                            if (!_writeAndVerifyFlash(start, data.get(), kFlashBufferSize, compare.get(), args)) {
                                                position = 0;
                                                break;
                                            }
                                            delay(1);
                                            // reset buffer ptr
                                            ptr = data.get();
                                            // move address ahead
                                            start += 32;
                                            if (i < args.size() - 1) {
                                                stream.printf_P(PSTR("[%08X] "), start + SECTION_FLASH_START_ADDRESS);
                                            }
                                        }
                                    }

                                    // data left?
                                    auto rest = position % kFlashBufferSize;
                                    if (rest != 0) {
                                        stream.println();
                                        _writeAndVerifyFlash(start, data.get(), rest, compare.get(), args);
                                    }
                                }
                                else {
                                    args.print(F("failed to allocate %u bytes"), length);
                                }
                            }
                        }
                        break;
                    default:
                        args.print(F("invalid command: %s"), cmdStr);
                        break;
                }
            }
            else {
                args.print(F("invalid command"));
            }

        }
        else
    #endif
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(RSSI)) || args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(HEAP)) || args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(GPIO))) {

        if (args.requireArgs(0, 2)) {
            auto interval = args.toMillis(0, 0, 3600 * 1000, 0, String('s'));
            auto umm = args.equalsIgnoreCase(1, F("umm"));
            if (interval < 250) {
                if (displayTimer) {
                    displayTimer->remove();
                    args.print(F("Interval disabled"));
                }
                    print_heap();
                    Serial.println();
            }
            else {
                if (!displayTimer) {
                    new DisplayTimer();
                }
                if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(RSSI))) {
                    displayTimer->setType(DisplayTimer::DisplayType::RSSI, Event::milliseconds(interval));
                }
                else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(GPIO))) {
                    displayTimer->setType(DisplayTimer::DisplayType::GPIO, Event::milliseconds(interval));
                }
                else {
                    displayTimer->setType(umm ? DisplayTimer::DisplayType::HEAP_UMM : DisplayTimer::DisplayType::HEAP, Event::milliseconds(interval));
                }
                args.print(F("Interval set to %ums"), interval);
            }
        }
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(ADC))) {
        if (args.requireArgs(1)) {
            if (args.equalsIgnoreCase(0, F("websocket"))) {

                // websocket,<client_id>,<read interval/microseconds>,<duration/ms>[,<packet size=1024>]
                // +ADC=websocket,0x3fff595c,2500,10,1024
                // +ADC=websocket,0,25000,5000,512
                // +ADC=websocket,0,500,5000,1024
                // +ADC=websocket,0x3fff2dcc,500,10000,1536

                uint32_t clientId = args.toNumber(1, 0, 16);
                auto interval = args.toIntMinMax(2, 250U, ~0U, 0xffffffffU);                            // 0.25ms up to 4000 seconds
                auto duration = args.toIntMinMax(3, 1U, (1U << 22), 10000U);                            // 1ms - 4194 seconds
                auto packetSize = args.toIntMinMax(4, 64U, TCP_SND_BUF - 64U, TCP_SND_BUF - 64U);       // ESP8266: TCP_SND_BUF = 536 * 2 = max. 1072 - overhead for the web socket
                AsyncWebSocketClient *client = Http2Serial::getClientById(reinterpret_cast<AsyncWebSocketClient *>(clientId));
                if (client) {
                    at_mode_adc_delete_object();
                    atModeADC = new AtModeADCWebSocket();
                    if (!atModeADC) {
                        __DBG_printf_E("memory allocation failed");
                    }
                    else if (reinterpret_cast<AtModeADCWebSocket *>(atModeADC)->init(interval, duration, packetSize, client)) {
                        args.print(F("Sending ADC readings to client=%p for %.2f seconds, interval=%.3f milliseconds, packet size=%u"), client, duration / 1000.0, interval / 1000.0, packetSize);
                    }
                    else {
                        args.print(F("Failed to initialize ADC"));
                        at_mode_adc_delete_object();
                    }
                }
                else {
                    args.print(F("Cannot find web socket client id=0x%08x"), clientId);
                }

            }
            else if (args.isAnyMatchIgnoreCase(0, F("0|off|stop"))) {
                args.print(F("ADC display off"));
                at_mode_adc_delete_object();
            }
            else {
                auto interval = args.toMillis(0, 100, ~0, 1000, String('s'));
                auto period = args.toIntMinMax(1, 1, 1000, 10);
                auto multiplier = args.toFloatMinMax(2, 0.1f, 100000.0f, 1000.0f);
                auto unit = args.toString(3, String('V'));
                auto readDelay = args.toIntMinMax(4, 0U, ~0U, 1250U);

                at_mode_adc_delete_object();
                atModeADC = new AtModeADC();
                if (!atModeADC) {
                    __DBG_printf_E("memory allocation failed");
                }
                else if (atModeADC->init(period, multiplier, unit, readDelay)) {

                    args.print(F("ADC display interval %ums"), interval);
                    auto &stream = args.getStream();
                    atModeADC->getTimer().add(interval, true, [&stream](Event::CallbackTimerPtr) {
                        stream.printf_P(PSTR("+ADC: %u (%umV) converted=%s "), atModeADC->getValue(), atModeADC->getValue(), atModeADC->getConvertedString().c_str());
                        atModeADC->printInfo(stream);
                    });
                }
                else {
                    args.print(F("Failed to initialize ADC"));
                    at_mode_adc_delete_object();
                }

            }
        }
    }
#if DEBUG
    #if ESP8266
        else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(DUMPM))) {
            if (args.requireArgs(1, 4)) {
                uintptr_t start = translateAddress(args.toString(0));
                if (start == ~0U) {
                    start = args.toNumber(0, 0U);
                }
                auto len = args.toNumber(1, 32U);
                bool insecure = args.isTrue(2, false);
                bool flashRead = args.isTrue(3, true);
                if (!insecure && ((start < UMM_MALLOC_CFG_HEAP_ADDR || start >= 0x3FFFFFFFUL) && (start < SECTION_FLASH_START_ADDRESS || start >= SECTION_FLASH_END_ADDR(irom0_text)))) {
                    args.print(F("address=0x%08x not HEAP (%08x-%08x) or FLASH (%08x-%08x), use unsecure mode"), start, UMM_MALLOC_CFG_HEAP_ADDR, 0x3FFFFFFFUL, SECTION_FLASH_START_ADDRESS, SECTION_FLASH_END_ADDR(irom0_text) - 1);
                }
                else if (start & 0x3) {
                    args.print(F("address=%0x08x not aligned"), start);
                }
                else if (start == 0) {
                    args.print(F("address missing"));
                }
                else if (len == 0) {
                    args.print(F("length missing"));
                }
                else {
                    auto end = start + len;
                    args.print(F("start=0x%08x end=0x%08x length=%u"), start, end, end - start);
                    uint8_t buf[32];
                    std::fill_n(buf, sizeof(buf), 0xff);
                    while(start < end) {
                        auto len = std::min<size_t>(sizeof(buf), end - start);
                        if (flashRead && start >= SECTION_FLASH_START_ADDRESS && start < SECTION_FLASH_END_ADDRESS) {
                            if (!ESP.flashRead(start - SECTION_FLASH_START_ADDRESS, buf, len)) {
                                break;
                            }
                        }
                        else {
                            memcpy_P(buf, (const void *)start, len);
                        }
                        DumpBinary(args.getStream(), DumpBinary::kGroupBytesDefault, len, start).dump(buf, len);
                        start += len;
                    }
                }
            }
        }
    #endif
    #if LOGGER
        else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(LOGDBG))) {
            if (args.requireArgs(1, 1)) {
                bool enable = args.isTrue(0);
                static File debugLog;
                if (enable) {
#pragma push_macro("DEBUG")
#undef DEBUG
                    if (!debugLog) {
                        _logger.setExtraFileEnabled(Logger::Level::DEBUG, true);
                        _logger.__rotate(Logger::Level::DEBUG);
                        debugLog = _logger.__openLog(Logger::Level::DEBUG, true);
                        if (debugLog) {
                            debugStreamWrapper.add(&debugLog);
                            args.print(F("enabled=%s"), debugLog.fullName());
                        }
                    }
                }
                else {
                    if (debugLog) {
                        debugStreamWrapper.remove(&debugLog);
                        debugLog.close();
                        _logger.__rotate(Logger::Level::DEBUG);
                        _logger.setExtraFileEnabled(Logger::Level::DEBUG, false);
                    }
                }
#pragma pop_macro("DEBUG")
                if (!debugLog) {
                    args.print(FSPGM(disabled));
                }
            }
        }
    #endif
#endif
    else {
        bool commandWasHandled = false;
        for(const auto plugin: PluginComponents::Register::getPlugins()) { // send command to plugins
            if (plugin->hasAtMode()) {
                if (true == (commandWasHandled = plugin->atModeHandler(args))) {
                    break;
                }
            }
        }
        if (!commandWasHandled) {
            at_mode_print_invalid_command(output);
        }
    }

    if (nextCommand) {
        auto cmd = String(nextCommand);
        cmd.trim();
        if (cmd.length()) {
            at_mode_serial_handle_event(cmd);
        }
    }
    return;
}

void at_mode_serial_input_handler(Stream &client)
{
    if (is_at_mode_enabled) {
        static bool lastWasCR = false;
        auto &line = serialHandler.inputBuffer;

        auto serial = StreamWrapper(serialHandler.getStreams(), serialHandler.getInput()); // local output only
        while(client.available()) {
            int ch = client.read();
            // __DBG_printf("read %u (%c) cr=%u", (unsigned)((uint8_t)ch), isprint(ch) ? ch : '-', lastWasCR);
            if (lastWasCR == true && ch == '\n') {
                lastWasCR = false;
                continue;
            }
            switch(ch) {
                case 128:
                case -1:
                case 0:
                    break;
                case 9:
                    if (!line.length()) {
                        line = F("AT+");
                        serial.print(line);
                    }
                    break;
                case 8:
                    if (line.length()) {
                        line.remove(line.length() - 1, 1);
                        serial.print(F("\b \b"));
                    }
                    break;
                case '\n':
                    lastWasCR = false;
                    serial.write('\n');
                    at_mode_serial_handle_event(line);
                    line.clear();
                    break;
                case '\r':
                    lastWasCR = true;
                    serial.println();
                    at_mode_serial_handle_event(line);
                    line.clear();
                    break;
                default:
                    if (ch > 128) {
                        serial.printf_P(PSTR("Serial input - invalid character %u\r\n"), ch);
                        break;
                    }
                    line += (char)ch;
                    serial.write(ch);
                    if (line.length() >= SERIAL_HANDLER_INPUT_BUFFER_MAX) {
                        serial.write('\n');
                        at_mode_serial_handle_event(line);
                        line.clear();
                    }
                    break;
            }
        }
    }
}
