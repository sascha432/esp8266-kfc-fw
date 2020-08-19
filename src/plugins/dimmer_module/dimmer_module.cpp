/**
 * Author: sascha_lammers@gmx.de
 */

#include "../include/templates.h"
#include <EventScheduler.h>
#include "plugins.h"
#include "dimmer_module.h"
#include "WebUISocket.h"

#include "firmware_protocol.h"

#if DEBUG_IOT_DIMMER_MODULE
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

Driver_DimmerModule::Driver_DimmerModule() : MQTTComponent(ComponentTypeEnum_t::SENSOR), Dimmer_Base()
{
}

void DimmerModulePlugin::getStatus(Print &out)
{
    out.printf_P(PSTR("%u Channel MOSFET Dimmer "), _channels.size());
    if (_isEnabled()) {
        out.print(F("enabled on "));
#if IOT_DIMMER_MODULE_INTERFACE_UART
        out.print(F("Serial Port"));
#else
        out.print(F("I2C"));
#endif
        _printStatus(out);
    }
    else {
        out.print(F("disabled"));
    }
}

void Driver_DimmerModule::_begin()
{
    _debug_println();
    Dimmer_Base::_begin();
    _beginMqtt();
    _beginButtons();
    _getChannels();
}

void Driver_DimmerModule::_end()
{
    _debug_println();
    _endButtons();
    _endMqtt();
    Dimmer_Base::_end();
}

void Driver_DimmerModule::_beginMqtt()
{
    auto mqttClient = MQTTClient::getClient();
    if (mqttClient) {
        for (uint8_t i = 0; i < _channels.size(); i++) {
            _channels[i].setup(this, i);
            mqttClient->registerComponent(&_channels[i]);
        }
        mqttClient->registerComponent(this);
    }
}

void Driver_DimmerModule::_endMqtt()
{
    _debug_println();
    auto mqttClient = MQTTClient::getClient();
    if (mqttClient) {
        mqttClient->unregisterComponent(this);
        for(uint8_t i = 0; i < _channels.size(); i++) {
            mqttClient->unregisterComponent(&_channels[i]);
        }
    }
}

void Driver_DimmerModule::_beginButtons()
{
#if IOT_DIMMER_MODULE_HAS_BUTTONS
    PinMonitor &monitor = *PinMonitor::createInstance();

    for(uint8_t i = 0; i < _channels.size() * 2; i++) {
        auto pinNum = _config.pins[i];
        __LDBG_printf("Channel %u button %u PIN %u", i / 2, i % 2, pinNum);
        if (pinNum) {
            auto pin = monitor.addPin(pinNum, pinCallback, this, IOT_DIMMER_MODULE_PINMODE);
            if (pin) {
                _buttons.emplace_back(pinNum);
                auto &button = _buttons.back().getButton();
#if DEBUG_IOT_DIMMER_MODULE
                // not used, debug only
                button.onPress(Driver_DimmerModule::onButtonPressed);
#else
                button.onPress(nullptr);    // the callback needs to be set to nullptr since it is not initialized in the button class
#endif
                button.onHoldRepeat(_config.longpress_time, _config.repeat_time, Driver_DimmerModule::onButtonHeld);
                button.onRelease(Driver_DimmerModule::onButtonReleased);
            }
            else {
                __LDBG_printf("Failed to add PIN %u", pinNum);
                monitor.removePin(pinNum, this);
            }
        }
    }
#endif
}

void Driver_DimmerModule::_endButtons()
{
#if IOT_DIMMER_MODULE_HAS_BUTTONS
    _debug_println(F("removing timers"));
    for(uint8_t i = 0; i < _channels.size(); i++) {
        _turnOffTimer[i].remove();
    }

    _debug_println(F("removing pin monitor"));
    PinMonitor *monitor = PinMonitor::getInstance();
    if (monitor) {
        for(auto &button: _buttons) {
            monitor->removePin(button.getPin(), this);
        }
        if (!monitor->size()) {
            PinMonitor::deleteInstance();
        }
    }
    LoopFunctions::remove(Driver_DimmerModule::loop);
#endif
}

void Driver_DimmerModule::onConnect(MQTTClient *client)
{
#if !IOT_DIMMER_MODULE_INTERFACE_UART
    _fetchMetrics();
#endif
}

void Driver_DimmerModule::_printStatus(Print &out)
{
    out.print(F(", Fading enabled" HTML_S(br)));
    for(uint8_t i = 0; i < _channels.size(); i++) {
        out.printf_P(PSTR("Channel %u: "), i);
        if (_channels[i].getOnState()) {
            out.printf_P(PSTR("on - %.1f%%" HTML_S(br)), _channels[i].getLevel() / (float)IOT_DIMMER_MODULE_MAX_BRIGHTNESS * 100.0);
        } else {
            out.print(F("off" HTML_S(br)));
        }
    }
    Dimmer_Base::_printStatus(out);
}


bool Driver_DimmerModule::on(uint8_t channel)
{
    return _channels[channel].on();
}

bool Driver_DimmerModule::off(uint8_t channel)
{
    return _channels[channel].off();
}

// get brightness values from dimmer
void Driver_DimmerModule::_getChannels()
{
    _debug_println();

    if (_wire.lock()) {
        _wire.beginTransmission(DIMMER_I2C_ADDRESS);
        _wire.write(DIMMER_REGISTER_COMMAND);
        _wire.write(DIMMER_COMMAND_READ_CHANNELS);
        _wire.write(_channels.size() << 4);
        int16_t level;
        const int len = _channels.size() * sizeof(level);
        if (_wire.endTransmission() == 0 && _wire.requestFrom(DIMMER_I2C_ADDRESS, len) == len) {
            for(uint8_t i = 0; i < _channels.size(); i++) {
                _wire.read(level);
                _channels[i].setLevel(level);
            }
#if DEBUG_IOT_DIMMER_MODULE
            String str;
            for(uint8_t i = 0; i < _channels.size(); i++) {
                str += String(_channels[i].getLevel());
                str += ' ';
            }
            __LDBG_printf("%s", str.c_str());
#endif
        }
        _wire.unlock();

#if IOT_SENSOR_HLW80xx_ADJUST_CURRENT
    // sensors are initialized after the dimmer plugin
    _Scheduler.add(100, false, [this](Event::TimerPtr &) {
        _setDimmingLevels();
    });
#endif
    }
}

int16_t Driver_DimmerModule::getChannel(uint8_t channel) const
{
    return _channels[channel].getLevel();
}

bool Driver_DimmerModule::getChannelState(uint8_t channel) const
{
    return _channels[channel].getOnState();
}

void Driver_DimmerModule::setChannel(uint8_t channel, int16_t level, float time)
{
    if (time == -1) {
        time = _config.fade_time;
    }
    _channels[channel].setLevel(level);
    _fade(channel, level, time);
    writeEEPROM();
    _channels[channel].publishState();
}

void Driver_DimmerModule::_onReceive(size_t length)
{
    __LDBG_printf("length=%u type=%02x", length, _wire.peek());
    if (_wire.peek() == DIMMER_FADING_COMPLETE) {
        _wire.read();
        length--;

        dimmer_fading_complete_event_t event;
        while(length >= sizeof(event)) {
            length -= _wire.read(event);
            if (event.channel < _channels.size()) {
                if (_channels[event.channel].getLevel() != event.level) {  // update level if out of sync
                    _channels[event.channel].setLevel(event.level);
                    _channels[event.channel].publishState();
                }
            }
        }

        // update MQTT
        _forceMetricsUpdate(5);
    }
    else {
        Dimmer_Base::_onReceive(length);
    }
}


// buttons
#if IOT_DIMMER_MODULE_HAS_BUTTONS

void Driver_DimmerModule::pinCallback(void *arg)
{
    auto pin = reinterpret_cast<PinMonitor::Pin *>(arg);
    reinterpret_cast<Driver_DimmerModule *>(pin->getArg())->_pinCallback(*pin);
}

void Driver_DimmerModule::loop()
{
    dimmer_plugin._loop();
}

void Driver_DimmerModule::onButtonPressed(Button& btn)
{
    __LDBG_printf("onButtonPressed %p", &btn);
}

void Driver_DimmerModule::onButtonHeld(Button& btn, uint16_t duration, uint16_t repeatCount)
{
    __LDBG_printf("onButtonHeld %p duration %u repeat %u", &btn, duration, repeatCount);

    uint8_t pressed, channel;
    bool buttonUp;
    if (dimmer_plugin._findButton(btn, pressed, channel, buttonUp)) {
        dimmer_plugin._buttonRepeat(channel, buttonUp, repeatCount);
    }
}

void Driver_DimmerModule::onButtonReleased(Button& btn, uint16_t duration)
{
    __LDBG_printf("onButtonReleased %p duration %u", &btn, duration);

    uint8_t pressed, channel;
    bool buttonUp;
    auto &config = dimmer_plugin._getConfig();

    if (dimmer_plugin._findButton(btn, pressed, channel, buttonUp)) {
        if (duration < config.shortpress_time) {   // short press
            dimmer_plugin._buttonShortPress(channel, buttonUp);
        }
        else if (duration < config.longpress_time) { // long press
            dimmer_plugin._buttonLongPress(channel, buttonUp);
        }
        else { // held

        }
    }
    if (!pressed) { // no button pressed anymore, remove main loop functions
        LoopFunctions::remove(Driver_DimmerModule::loop);
    }
}

bool Driver_DimmerModule::_findButton(Button &btn, uint8_t &pressed, uint8_t &channel, bool &buttonUp)
{
    uint8_t num = 0;

    channel = 0xff;
    pressed = 0;
    for(auto &button: dimmer_plugin._buttons) {
        if (btn.is(button.getButton())) {
            channel = num / 2;
            buttonUp = !(num % 2);
        }
        if (button.getButton().isPressed()) {
            pressed++;
        }
        num++;
    }
    __LDBG_printf("pressed=%u channel=%u button=%s", pressed, channel, channel == 0xff ? PSTR("N/A") : (buttonUp ? PSTR("up") : PSTR("down")));

    return channel != 0xff;
}


void Driver_DimmerModule::_pinCallback(PinMonitor::Pin &pin)
{
    // add main loop function to handle timings and debouncing
    LoopFunctions::add(Driver_DimmerModule::loop);
    // find button and update
    for(auto &button: _buttons) {
        if (button.getPin() == pin.getPin()) {
            button.getButton().update();
        }
    }
}

void Driver_DimmerModule::_loop()
{
    // update all buttons
    for(auto &button: _buttons) {
        button.getButton().update();
    }
}

void Driver_DimmerModule::_buttonShortPress(uint8_t channel, bool up)
{
    __LDBG_printf("channel=%d dir=%s repeat=%u", channel, up ? PSTR("up") : PSTR("down"), _turnOffTimerRepeat[channel]);
    if (getChannelState(channel)) {
        // single short press, start timer
        // _turnOffTimerRepeat[channel] will be 0 for a single button down press

        if (_turnOffTimer[channel].active()) {
            _turnOffTimerRepeat[channel]++;
            _turnOffTimer[channel]->rearm(_config.shortpress_no_repeat_time, false);     // rearm timer after last keypress
        }
        else {
            _turnOffLevel[channel] = getChannel(channel);
            _turnOffTimerRepeat[channel] = 0;
            _Timer(_turnOffTimer[channel]).add(_config.shortpress_no_repeat_time, false, [this, channel](Event::TimerPtr &timer) {
                __LDBG_printf("turn off channel=%u timer expired, repeat=%d", channel, _turnOffTimerRepeat[channel]);
                if (_turnOffTimerRepeat[channel] == 0) { // single button down press detected, turn off
                    if (off(channel)) {
                        _channels[channel].setStoredBrightness(_turnOffLevel[channel]); // restore level to when the button was pressed
                    }
                }
            });
        }
        if (up) {
            _buttonRepeat(channel, true, 0);        // short press up = fire repeat event
            _turnOffTimerRepeat[channel]++;
        } else {
            _buttonRepeat(channel, false, 0);       // short press down = fire repeat event
        }
    }
    else {
        if (up) {                                   // short press up while off = turn on
            on(channel);
        }
        else {                                      // short press down while off = ignore
        }
    }
}

void Driver_DimmerModule::_buttonLongPress(uint8_t channel, bool up)
{
    __LDBG_printf("channel=%d dir=%s", channel, up ? PSTR("up") : PSTR("down"));
    //if (getChannelState(channel))
    {
        if (up) {
            // long press up = increase to max brightness
            setChannel(channel, IOT_DIMMER_MODULE_MAX_BRIGHTNESS * _config.longpress_max_brightness / 100, _config.longpress_fadetime);
        }
        else {
            // long press down = decrease to min brightness
            setChannel(channel, IOT_DIMMER_MODULE_MAX_BRIGHTNESS * _config.longpress_min_brightness / 100, _config.longpress_fadetime);
        }
    }
}

void Driver_DimmerModule::_buttonRepeat(uint8_t channel, bool up, uint16_t repeatCount)
{
    auto level = getChannel(channel);
    int16_t change = IOT_DIMMER_MODULE_MAX_BRIGHTNESS * _config.shortpress_step / 100;
    if (!up) {
        change = -change;
    }
    int16_t newLevel = max(IOT_DIMMER_MODULE_MAX_BRIGHTNESS * _config.min_brightness / 100, min(IOT_DIMMER_MODULE_MAX_BRIGHTNESS, level + change));
    if (level != newLevel) {
        __LDBG_printf("channel=%d dir=%s repeat=%d change=%d new_level=%d", channel, up ? PSTR("up") : PSTR("down"), repeatCount, change, newLevel);
        setChannel(channel, newLevel, _config.shortpress_fadetime);
    }
}

#endif

DimmerModulePlugin dimmer_plugin;

PROGMEM_DEFINE_PLUGIN_OPTIONS(
    DimmerModulePlugin,
    "dimmer",           // name
    "Dimmer",           // friendly name
    "",                 // web_templates
    "dimmer_cfg",       // config_forms
    "mqtt,http",        // reconfigure_dependencies
    PluginComponent::PriorityType::DIMMER_MODULE,
    PluginComponent::RTCMemoryId::NONE,
    static_cast<uint8_t>(PluginComponent::MenuType::AUTO),
    false,              // allow_safe_mode
    false,              // setup_after_deep_sleep
    true,               // has_get_status
    true,               // has_config_forms
    true,               // has_web_ui
    false,              // has_web_templates
    true,               // has_at_mode
    0                   // __reserved
);

DimmerModulePlugin::DimmerModulePlugin() : PluginComponent(PROGMEM_GET_PLUGIN_OPTIONS(DimmerModulePlugin))
{
    REGISTER_PLUGIN(this, "DimmerModulePlugin");
}


void DimmerModulePlugin::setup(SetupModeType mode)
{
    setupWebServer();
    _begin();
}

void DimmerModulePlugin::reconfigure(const String &source)
{
    if (String_equals(source, SPGM(http))) {
        setupWebServer();
    }
    else if (String_equals(source, SPGM(mqtt))) {
        _endMqtt();
        _beginMqtt();
    }
    else {
        writeConfig();
        _endButtons();
        _beginButtons();
    }
}

void DimmerModulePlugin::shutdown()
{
    _end();
}

void DimmerModulePlugin::createWebUI(WebUI &webUI)
{
    auto row = &webUI.addRow();
    row->setExtraClass(JJ(title));
    row->addGroup(F("Dimmer"), true);

    for (uint8_t i = 0; i < _channels.size(); i++) {
        row = &webUI.addRow();
        row->addSlider(PrintString(F("dimmer_channel%u"), i), PrintString(F("dimmer_channel%u"), i), 0, IOT_DIMMER_MODULE_MAX_BRIGHTNESS, true);
    }

    row = &webUI.addRow();
    auto sensor = getMetricsSensor();
    if (sensor) {
        sensor->_createWebUI(webUI, &row);
    }
    // row->addBadgeSensor(F("dimmer_vcc"), F("Dimmer VCC"), 'V');
    // row->addBadgeSensor(F("dimmer_frequency"), F("Dimmer Frequency"), FSPGM(Hz));
    // row->addBadgeSensor(F("dimmer_int_temp"), F("Dimmer ATmega"), FSPGM(_degreeC));
    // row->addBadgeSensor(F("dimmer_ntc_temp"), F("Dimmer NTC"), FSPGM(_degreeC));
}

