/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_CLOCK

#include "clock.h"
#include <Timezone.h>
#include <MicrosTimer.h>
#include <LoopFunctions.h>
#include <WiFiCallbacks.h>
#include <EventTimer.h>
#include "blink_led_timer.h"
#include "progmem_data.h"
#include "./plugins/mqtt/mqtt_client.h"

#if DEBUG_IOT_CLOCK
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

static ClockPlugin plugin;

ClockPlugin::ClockPlugin() :
    MQTTComponent(LIGHT),
#if IOT_CLOCK_BUTTON_PIN
    _button(IOT_CLOCK_BUTTON_PIN, PRESSED_WHEN_HIGH),
    _buttonCounter(0),
#endif
    _color(0, 0, 80), _updateTimer(0), _time(0), _updateRate(1000), _isSyncing(1), _timeFormat24h(true)
{
    _brightness = 255;
    _colors[0] = 0;
    _colors[1] = 0;
    _colors[2] = 80;
    memset(&_animationData, 0, sizeof(_animationData));
    setBlinkColon(SOLID);
    register_plugin(this);
}

PGM_P ClockPlugin::getName() const
{
    return PSTR("clock");
}

void ClockPlugin::setup(PluginSetupMode_t mode)
{
    _debug_println(F("ClockPlugin::setup()"));

    _display = new SevenSegmentPixel(IOT_CLOCK_NUM_DIGITS, IOT_CLOCK_NUM_PIXELS, IOT_CLOCK_NUM_COLONS);

    auto cfg = updateConfig();
    _setSevenSegmentDisplay(cfg);

#if IOT_CLOCK_BUTTON_PIN
    _button.onHoldRepeat(800, 100, onButtonHeld);
    _button.onRelease(onButtonReleased);
#endif

    LoopFunctions::add(loop);
    WiFiCallbacks::add(WiFiCallbacks::CONNECTED, wifiCallback);

    if (!IS_TIME_VALID(time(nullptr))) {
        setSyncing();
    }

    auto mqttClient = MQTTClient::getClient();
    if (mqttClient) {
        mqttClient->registerComponent(this);
    }
}


void ClockPlugin::reconfigure(PGM_P source)
{
    _debug_println(F("ClockPlugin::reconfigure()"));
    auto cfg = updateConfig();
    _setSevenSegmentDisplay(cfg);
}

const String ClockPlugin::getStatus()
{
    return F("Clock Plugin");
}


#if AT_MODE_SUPPORTED

#include "at_mode.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(CLOCKPX, "CLOCKPX", "<led>,<r>,<g>,<b>", "Set level of a single pixel");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(CLOCKP, "CLOCKP", "<00[:.]00[:.]00>", "Display strings");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(CLOCKC, "CLOCKC", "<r>,<g>,<b>", "Set color");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(CLOCKTS, "CLOCKTS", "<num>,<segment>", "Set segment for digit <num>");
PROGMEM_AT_MODE_HELP_COMMAND_DEF(CLOCKA, "CLOCKA", "<num>", "Set animation", "Display available animations");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(CLOCKD, "CLOCKD", "Dump pixel addresses");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(CLOCKF, "CLOCKF", "Toggle 12/24h time format");

void ClockPlugin::atModeHelpGenerator()
{
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(CLOCKPX));
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(CLOCKP));
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(CLOCKC));
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(CLOCKTS));
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(CLOCKA));
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(CLOCKD));
}

bool ClockPlugin::atModeHandler(Stream &serial, const String &command, int8_t argc, char **argv)
{
    if (constexpr_String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(CLOCKTS))) {
        if (argc == 2) {
            enable(false);
            uint8_t digit = atoi(argv[0]);
            uint8_t segment = atoi(argv[1]);
            serial.printf_P(PSTR("+CLOCKTS: digit=%u, segment=%c, color=#%06x\n"), digit, _display->getSegmentChar(segment), _color.get());
            _display->setSegment(digit, segment, _color);
            _display->show();
        }
        else {
            enable(true);
        }
        return true;
    }
    else if (constexpr_String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(CLOCKP))) {
        if (argc < 1) {
            serial.printf_P(PSTR("+CLOCKP: clear\n"));
            _display->clear();
            _display->show();
            enable(true);
        }
        else {
            enable(false);
            serial.printf_P(PSTR("+CLOCKP: '%s'\n"), argv[0]);
            _display->print(argv[0], _color);
            _display->show();
        }
        return true;
    }
    else if (constexpr_String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(CLOCKA))) {
        if (argc == -1) {
            serial.printf_P(PSTR(
                    "+CLOCKA: %u - blink colon twice per second\n"
                    "+CLOCKA: %u - blink colon once per second\n"
                    "+CLOCKA: %u - solid colon\n"
                    "+CLOCKA: %u - rainbow animation\n"
                    "+CLOCKA: %u - flashing\n"
                    "+CLOCKA: 1000 = disable clock\n"
                    "+CLOCKA: 1001 = enable clock\n"
                    "+CLOCKA: 1002 = all pixels on\n"
                ),
                BLINK_COLON,
                FAST_BLINK_COLON,
                SOLID_COLON,
                RAINBOW,
                FLASHING
            );
        }
        else if (argc == 1) {
            int value = atoi(argv[0]);
            if (value == 1000) {
                enable(false);
            }
            else if (value == 1001) {
                enable(true);
            }
            else if (value == 1002) {
                enable(false);
                _display->setColor(0xffffff);
            }
            else {
                setAnimation((AnimationEnum_t)value);
            }
        }
        else {
            setAnimation(SOLID_COLON);
        }
        return true;
    }
    else if (constexpr_String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(CLOCKC))) {
        if (argc == 3) {
            _color = Color(atoi(argv[0]), atoi(argv[1]), atoi(argv[2]));
            serial.printf_P(PSTR("+CLOCKC: color=#%06x\n"), _color.get());
        }
        else  {
            at_mode_print_invalid_arguments(serial);
        }
        return true;
    }
    else if (constexpr_String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(CLOCKPX))) {
        if (argc == 4) {
            enable(false);

            int num = atoi(argv[0]);
            Color color(atoi(argv[1]), atoi(argv[2]), atoi(argv[3]));
            if (num < 0) {
#if IOT_CLOCK_NEOPIXEL
                serial.printf_P(PSTR("+CLOCKPX: pixel=0-%u, color=#%06x\n"), _display->getTotalPixelCount(), color.get());
#else
                serial.printf_P(PSTR("+CLOCKPX: pixel=0-%u, color=#%06x\n"), FastLED.size(), color.get());
#endif
                _display->setColor(color);
            }
            else {
                serial.printf_P(PSTR("+CLOCKPX: pixel=%u, color=#%06x\n"), num, color.get());
                _display->setColor(num, color);
            }
        }
        else {
            at_mode_print_invalid_arguments(serial);
        }
        return true;
    }
    else if (constexpr_String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(CLOCKD))) {
        _display->dump(serial);
        return true;
    }
    else if (constexpr_String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(CLOCKF))) {
        _timeFormat24h = !_timeFormat24h;
        serial.printf_P(PSTR("+CLOCKF: time format 24h=%u\n"), _timeFormat24h);
        return true;
    }
    return false;
}

#endif


void ClockPlugin::createAutoDiscovery(MQTTAutoDiscovery::Format_t format, MQTTAutoDiscoveryVector &vector) {
    _debug_printf_P(PSTR("ClockPlugin::createAutoDiscovery(): format=%u\n"), format);
    String topic = MQTTClient::formatTopic(0, F("/"));

    auto discovery = _debug_new MQTTAutoDiscovery();
    discovery->create(this, 0, format);
    discovery->addStateTopic(topic + F("state"));
    discovery->addCommandTopic(topic + F("set"));
    discovery->addPayloadOn(FSPGM(1));
    discovery->addPayloadOff(FSPGM(0));
    discovery->addBrightnessStateTopic(topic + F("brightness/state"));
    discovery->addBrightnessCommandTopic(topic + F("brightness/set"));
    discovery->addBrightnessScale(255);
    discovery->addRGBStateTopic(topic + F("color/state"));
    discovery->addRGBCommandTopic(topic + F("color/set"));
    discovery->finalize();
    vector.emplace_back(MQTTAutoDiscoveryPtr(discovery));

    _qos = MQTTClient::getDefaultQos();
}

void ClockPlugin::onConnect(MQTTClient *client)
{
    _debug_printf_P(PSTR("ClockPlugin::onConnect()\n"));
    String topic = MQTTClient::formatTopic(0, F("/"));

    client->subscribe(this, topic + F("set"), _qos);
    client->subscribe(this, topic + F("color/set"), _qos);
    client->subscribe(this, topic + F("brightness/set"), _qos);

    publishState(client);
}

void ClockPlugin::onMessage(MQTTClient *client, char *topic, char *payload, size_t len)
{
    _debug_printf_P(PSTR("ClockPlugin::onMessage(): topic=%s, payload=%*.*s\n"), topic, len, len, payload);

    if (strstr(topic, "brightness/set")) {
        _brightness = atoi(payload);
        _setColor();
    }
    else if (strstr(topic, "color/set")) {
        char *r, *g, *b;
        r = strtok(payload, ",");
        if (r) {
            g = strtok(nullptr, ",");
            if (g) {
                b = strtok(nullptr, ",");
                if (b) {
                    _colors[0] = atoi(r);
                    _colors[1] = atoi(g);
                    _colors[2] = atoi(b);
                    _setColor();
                }
            }
        }
    }
    else if (strstr(topic, "/set")) {
        auto value = atoi(payload);
        if (value && _color == 0) {
            _setColor();
        }
        else if (!value) {
            _color = 0;
        }
    }
}

void ClockPlugin::_setColor()
{
    _color = Color(_colors[0] * _brightness / 255, _colors[1] * _brightness / 255, _colors[2] * _brightness / 255);
    _updateTimer = millis();
}

void ClockPlugin::publishState(MQTTClient *client)
{
    _debug_printf_P(PSTR("ClockPlugin::publishState(%p)\n"), client);
    if (!client) {
        client = MQTTClient::getClient();
    }
    String topic = MQTTClient::formatTopic(0, F("/"));

    client->publish(topic + F("state"), _qos, 1, String(_color ? 1 : 0));
    client->publish(topic + F("brightness/state"), _qos, 1, String(_brightness));
    PrintString str(F("%u,%u,%u"), _colors[0], _colors[1], _colors[2]);
    client->publish(topic + F("color/state"), _qos, 1, str);
}


void ClockPlugin::loop()
{
    plugin._loop();
}

void ClockPlugin::wifiCallback(uint8_t event, void *payload)
{
    // turn LED off after wifi has been connected
    BlinkLEDTimer::setBlink(BlinkLEDTimer::BlinkDelayEnum_t::OFF);
}

void ClockPlugin::enable(bool enable)
{
    _debug_printf_P(PSTR("ClockPlugin::enable(%u)\n"), enable);
    _off();
    if (enable) {
        LoopFunctions::add(loop);
    }
    else {
        LoopFunctions::remove(loop);
    }
}

void ClockPlugin::setSyncing() {
    _isSyncing = 1;
    _off();
}

void ClockPlugin::setBlinkColon(BlinkColonEnum_t value)
{
    _blinkColon = value;
}

void ClockPlugin::setAnimation(AnimationEnum_t animation)
{
    _debug_printf_P(PSTR("ClockPlugin::setAnimation(%d)\n"), animation);
    switch(animation) {
        case BLINK_COLON:
            setBlinkColon(NORMAL);
            _display->setCallback(nullptr);
            _updateRate = 1000;
            break;
        case FAST_BLINK_COLON:
            setBlinkColon(FAST);
            _display->setCallback(nullptr);
            _updateRate = 500;
            break;
        case SOLID_COLON:
            setBlinkColon(SOLID);
            _display->setCallback(nullptr);
            _updateRate = 1000;
            break;
        case RAINBOW:
            _animationData.rainbow.movementSpeed = 30;
            _display->setCallback([this](SevenSegmentPixel::pixel_address_t addr, SevenSegmentPixel::color_t color) {
                float factor1, factor2;
                uint16_t ind = addr * 4.285714285714286;
                ind += (millis() / _animationData.rainbow.movementSpeed);
                uint8_t idx = ((ind % 120) / 40);
                factor1 = 1.0 - ((float)(ind % 120 - (idx * 40)) / 40);
                factor2 = (float)((int)(ind - (idx * 40)) % 120) / 40;
                switch(idx) {
                    case 0:
                        return Color::get(255 * factor1, 255 * factor2, 0);
                    case 1:
                        return Color::get(0, 255 * factor1, 255 * factor2);
                    case 2:
                        return Color::get(255 * factor2, 0, 255 * factor1);
                }
                return color;
            });
            _updateRate = 1000 / 30;
            break;
        case FLASHING:
            _animationData.flashing.color = _color;
            _display->setCallback([this](SevenSegmentPixel::pixel_address_t addr, SevenSegmentPixel::color_t color) {
                return (millis() / _updateRate) % 2 == 0 ? _animationData.flashing.color : 0;
            });
            _updateRate = 50;
            break;
        case NONE:
        default:
            _display->setCallback(nullptr);
            _updateRate = _blinkColon == FAST ? 500 : 1000;
            break;
    }
    _updateTimer = 0;
}

Clock ClockPlugin::updateConfig()
{
    auto cfg = config._H_GET(Config().clock);

    _blinkColon = (BlinkColonEnum_t)cfg.blink_colon;
    _color = Color(cfg.solid_color);
    _timeFormat24h = cfg.time_format_24h;
    setAnimation((AnimationEnum_t)cfg.animation);

    return cfg;
}

#if IOT_CLOCK_BUTTON_PIN

void ClockPlugin::onButtonHeld(Button& btn, uint16_t duration, uint16_t repeatCount) {
    _debug_printf_P(PSTR("ClockPlugin::onButtonHeld() duration %u repeat %u\n"), duration, repeatCount);
    if (repeatCount == 1) {
        plugin.updateConfig();
        plugin._buttonCounter = 0;
    }
    if (repeatCount == 12) {    // start flashing after 2 seconds, hard reset occurs ~2.5s
        plugin.setAnimation(FLASHING);
        plugin._animationData.flashing.color = Color(255, 0, 0);
        plugin._updateRate = 50;

        Scheduler.addTimer(1000, false, [](EventScheduler::TimerPtr) {   // call restart if no reset occured
            _debug_printf_P(PSTR("ClockPlugin::onButtonHeld() restart device\n"));
            config.restartDevice();
        });
    }
}

void ClockPlugin::onButtonReleased(Button& btn, uint16_t duration) {
    _debug_printf_P(PSTR("ClockPlugin::onButtonReleased() duration %u\n"), duration);
    if (duration < 800) {
        plugin._onButtonReleased(duration);
    }
}

void ClockPlugin::_onButtonReleased(uint16_t duration) {
    _debug_printf_P(PSTR("ClockPlugin::onButtonReleased(): press=%u\n"), _buttonCounter);
    switch(_buttonCounter) {
        case 0:
            setAnimation(FAST_BLINK_COLON);
            break;
        case 1:
            setAnimation(BLINK_COLON);
            break;
        case 2:
            setAnimation(SOLID_COLON);
            break;
        case 3:
            setAnimation(RAINBOW);
            break;
        case 4:
            setAnimation(FLASHING);
            break;
        case 5:
            setAnimation(FLASHING);
            _updateRate = 500;
            _buttonCounter = -1;
            break;
    }
    _buttonCounter++;
}

#endif

void ClockPlugin::_loop()
{
#if IOT_CLOCK_BUTTON_PIN
    _button.update();
#endif
    auto now = time(nullptr);
    bool forceUpdate = false;
    if (_time != now) {
        _time = now;
        forceUpdate = true;
    }

    if (_isSyncing) {
        if (get_time_diff(_updateTimer, millis()) >= 100) {
            _updateTimer = millis();

            // show syncing animation until the time is valid
            if (IS_TIME_VALID(now)) {
                _isSyncing = 0;
                forceUpdate = true;
                _off();
            }
            else {
                _isSyncing++;
                if (_isSyncing > IOT_CLOCK_NUM_PIXELS * (SevenSegmentPixel::SegmentEnum_t::NUM - 1)) {
                    _isSyncing = 1;
                }
                for(uint8_t i = 0; i < _display->numDigits(); i++) {
                    _display->rotate(i, _isSyncing - 1, _color);
                }
                _display->show();
            }
        }
    }


    if (!_isSyncing && (forceUpdate || get_time_diff(_updateTimer, millis()) >= _updateRate)) {
        _updateTimer = millis();

        uint32_t color = _color;
        struct tm *tm = timezone_localtime(&now);
        // struct tm *tm = localtime(&now);
        uint8_t hour = tm->tm_hour;
        if (!_timeFormat24h) {
            hour = ((hour + 23) % 12) + 1;
        }

#if 0
static int time = 0,timecnt=0;
if (time!=hour*100+tm->tm_min) {
time=hour*100+tm->tm_min;
timecnt++;
Serial.printf("%04u %u\n",time,timecnt);
}
#endif

        _display->setDigit(0, hour / 10, color);
        _display->setDigit(1, hour % 10, color);
        _display->setDigit(2, tm->tm_min / 10, color);
        _display->setDigit(3, tm->tm_min % 10, color);
#if IOT_CLOCK_NUM_DIGITS == 6
        _display->setDigit(4, tm->tm_sec / 10, color);
        _display->setDigit(5, tm->tm_sec % 10, color);
#endif

        if (_blinkColon != SOLID && (millis() / (_blinkColon == FAST ? 500 : 1000)) % 2 == 0) {
            _display->clearColon(0);
#if IOT_CLOCK_NUM_COLONS == 2
            _display->clearColon(1);
#endif
        }
        else {
            _display->setColon(0, SevenSegmentPixel::BOTH, color);
#if IOT_CLOCK_NUM_COLONS == 2
            _display->setColon(1, SevenSegmentPixel::BOTH, color);
#endif
        }

        _display->show();
    }
}

void ClockPlugin::_setSevenSegmentDisplay(Clock &cfg)
{
    SevenSegmentPixel::pixel_address_t addr = 0;
    for(uint8_t i = 0; i < IOT_CLOCK_NUM_DIGITS + IOT_CLOCK_NUM_COLONS; i++) {
        if (cfg.order[i] < 0) {
            _debug_printf_P(PSTR("ClockPlugin::_setSevenSegmentDisplay(): address=%u, colon=%u\n"), addr, abs(cfg.order[i]) - 1);
            addr = _display->setColons(abs(cfg.order[i]) - 1, addr + 1, addr);
        }
        else {
            _debug_printf_P(PSTR("ClockPlugin::_setSevenSegmentDisplay(): address=%u, digit=%u\n"), addr, cfg.order[i]);
            addr = _display->setSegments(cfg.order[i], addr);
        }
    }
}

#endif
