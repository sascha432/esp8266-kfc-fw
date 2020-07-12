/**
 * Author: sascha_lammers@gmx.de
 */

#include "../include/templates.h"
#include <LoopFunctions.h>
#include <EventScheduler.h>
#include <EventTimer.h>
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

    auto mqttClient = MQTTClient::getClient();
    if (mqttClient) {
        mqttClient->setUseNodeId(true);
        for (uint8_t i = 0; i < _channels.size(); i++) {
            _channels[i].setup(this, i);
            mqttClient->registerComponent(&_channels[i]);
        }
        mqttClient->registerComponent(this);
    }

    _beginButtons();
    _getChannels();
}

void Driver_DimmerModule::_end()
{
    _debug_println();

    _endButtons();

    _debug_println(F("removing mqtt client"));
    auto mqttClient = MQTTClient::getClient();
    if (mqttClient) {
        mqttClient->unregisterComponent(this);
        for(uint8_t i = 0; i < _channels.size(); i++) {
            mqttClient->unregisterComponent(&_channels[i]);
        }
    }

    Dimmer_Base::_end();
}

void Driver_DimmerModule::_beginButtons()
{
#if IOT_DIMMER_MODULE_HAS_BUTTONS
    PinMonitor &monitor = *PinMonitor::createInstance();
    auto config = getButtonConfig();

    for(uint8_t i = 0; i < _channels.size() * 2; i++) {
        auto pinNum = config.pins[i];
        _debug_printf_P(PSTR("Channel %u button %u PIN %u\n"), i / 2, i % 2, pinNum);
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
                button.onHoldRepeat(config.longpress_time, config.repeat_time, Driver_DimmerModule::onButtonHeld);
                button.onRelease(Driver_DimmerModule::onButtonReleased);
            }
            else {
                _debug_printf_P(PSTR("Failed to add PIN %u\n"), pinNum);
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

MQTTComponent::MQTTAutoDiscoveryPtr Driver_DimmerModule::nextAutoDiscovery(MQTTAutoDiscovery::FormatType format, uint8_t num)
{
    return nullptr;
}

// MQTTComponent::MQTTAutoDiscoveryPtr Driver_DimmerModule::nextAutoDiscovery(MQTTAutoDiscovery::FormatType format, uint8_t num)
// {
//     if (num >= getAutoDiscoveryCount()) {
//         return nullptr;
//     }
//     auto discovery = new MQTTAutoDiscovery();
//     switch(num) {
//         case 0:
//             break;
//         case 1:
//             break;
//         case 2:
//             break;
//         case 3:
//             break;
//         case 4:
//             break;
//         case 5:
//             break;
//         case 6:
//             break;
//         case 7:
//             break;
//         case 8:
//             break;
//         case 9:
//             break;
//         case 0:
//             break;
//         case ß:
//             break;
//     }
//     discovery->finalize();
//     return discovery;
// }


// // void Driver_DimmerModule::createAutoDiscovery(MQTTAutoDiscovery::FormatType format, MQTTComponent::MQTTAutoDiscoveryVector &vector)
// // {
// //     if (format == MQTTAutoDiscovery::FORMAT_YAML) {
// //         for(uint8_t i = 0; i < _channels.size(); i++) {
// //             _channels[i].createAutoDiscovery(format, vector);
// //         }
// //     }

//     auto discovery = new MQTTAutoDiscovery();
//     discovery->create(this, F("temp"), format);
//     discovery->addStateTopic(_getMetricsTopics(0));
//     discovery->addUnitOfMeasurement(F("\u00b0C"));
//     discovery->finalize();
//     vector.emplace_back(discovery);

//     discovery = new MQTTAutoDiscovery();
//     discovery->create(this, F("temp2"), format);
//     discovery->addStateTopic(_getMetricsTopics(1));
//     discovery->addUnitOfMeasurement(F("\u00b0C"));
//     discovery->finalize();
//     vector.emplace_back(discovery);

//     discovery = new MQTTAutoDiscovery();
//     discovery->create(this, F("vcc"), format);
//     discovery->addStateTopic(_getMetricsTopics(2));
//     discovery->addUnitOfMeasurement(F("V"));
//     discovery->finalize();
//     vector.emplace_back(discovery);

//     discovery = new MQTTAutoDiscovery();
//     discovery->create(this, F("frequency"), format);
//     discovery->addStateTopic(_getMetricsTopics(3));
//     discovery->addUnitOfMeasurement(F("Hz"));
//     discovery->finalize();
//     vector.emplace_back(discovery);
// }

uint8_t Driver_DimmerModule::getAutoDiscoveryCount() const
{
    return 0; //4;
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
            _debug_printf_P(PSTR("%s\n"), str.c_str());
#endif
        }
        _wire.unlock();

#if IOT_SENSOR_HLW80xx_ADJUST_CURRENT
    // sensors are initialized after the dimmer plugin
    Scheduler.addTimer(100, false, [this](EventScheduler::TimerPtr) {
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
        time = _fadeTime;
    }
    _channels[channel].setLevel(level);
    _fade(channel, level, time);
    writeEEPROM();
    _channels[channel].publishState();
}

void Driver_DimmerModule::_onReceive(size_t length)
{
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
    _debug_printf_P(PSTR("onButtonPressed %p\n"), &btn);
}

void Driver_DimmerModule::onButtonHeld(Button& btn, uint16_t duration, uint16_t repeatCount)
{
    _debug_printf_P(PSTR("onButtonHeld %p duration %u repeat %u\n"), &btn, duration, repeatCount);

    uint8_t pressed, channel;
    bool buttonUp;
    if (dimmer_plugin._findButton(btn, pressed, channel, buttonUp)) {
        dimmer_plugin._buttonRepeat(channel, buttonUp, repeatCount);
    }
}

void Driver_DimmerModule::onButtonReleased(Button& btn, uint16_t duration)
{
    _debug_printf_P(PSTR("onButtonReleased %p duration %u\n"), &btn, duration);

    uint8_t pressed, channel;
    bool buttonUp;
    auto config = dimmer_plugin.getButtonConfig();

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
    _debug_printf_P(PSTR("pressed=%u channel=%u button=%s\n"), pressed, channel, channel == 0xff ? F("N/A") : (buttonUp ? F("up") : F("down")));

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
    _debug_printf_P(PSTR("channel=%d dir=%s repeat=%u\n"), channel, up ? F("up") : F("down"), _turnOffTimerRepeat[channel]);
    if (getChannelState(channel)) {
        auto config = dimmer_plugin.getButtonConfig();

        // single short press, start timer
        // _turnOffTimerRepeat[channel] will be 0 for a single button down press

        if (_turnOffTimer[channel].active()) {
            _turnOffTimerRepeat[channel]++;
            _turnOffTimer[channel]->rearm(config.shortpress_no_repeat_time, false);     // rearm timer after last keypress
        }
        else {
            _turnOffLevel[channel] = getChannel(channel);
            _turnOffTimerRepeat[channel] = 0;
            _turnOffTimer[channel].add(config.shortpress_no_repeat_time, false, [this, channel](EventScheduler::TimerPtr timer) {
                _debug_printf_P(PSTR("turn off channel=%u timer expired, repeat=%d\n"), channel, _turnOffTimerRepeat[channel]);
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
    _debug_printf_P(PSTR("channel=%d dir=%s\n"), channel, up ? F("up") : F("down"));
    //if (getChannelState(channel))
    {
        auto config = dimmer_plugin.getButtonConfig();
        if (up) {
            // long press up = increase to max brightness
            setChannel(channel, IOT_DIMMER_MODULE_MAX_BRIGHTNESS * config.longpress_max_brightness / 100, config.longpress_fadetime);
        }
        else {
            // long press down = decrease to min brightness
            setChannel(channel, IOT_DIMMER_MODULE_MAX_BRIGHTNESS * config.longpress_min_brightness / 100, config.longpress_fadetime);
        }
    }
}

void Driver_DimmerModule::_buttonRepeat(uint8_t channel, bool up, uint16_t repeatCount)
{
    auto level = getChannel(channel);
    auto config = dimmer_plugin.getButtonConfig();
    int16_t change = IOT_DIMMER_MODULE_MAX_BRIGHTNESS * config.shortpress_step / 100;
    if (!up) {
        change = -change;
    }
    int16_t newLevel = max(IOT_DIMMER_MODULE_MAX_BRIGHTNESS * config.min_brightness / 100, min(IOT_DIMMER_MODULE_MAX_BRIGHTNESS, level + change));
    if (level != newLevel) {
        _debug_printf_P(PSTR("channel=%d dir=%s repeat=%d change=%d new_level=%d\n"), channel, up ? F("up") : F("down"), repeatCount, change, newLevel);
        setChannel(channel, newLevel, config.shortpress_fadetime);
    }
}

#endif

DimmerModulePlugin dimmer_plugin;

PGM_P DimmerModulePlugin::getName() const
{
    return PSTR("dimmer");
}

void DimmerModulePlugin::setup(PluginSetupMode_t mode)
{
    setupWebServer();
    _begin();
}

void DimmerModulePlugin::reconfigure(PGM_P source)
{
    if (source == nullptr) {
        writeConfig();
        auto dimmer = config._H_GET(Config().dimmer);
        _fadeTime = dimmer.fade_time;
        _onOffFadeTime = dimmer.on_off_fade_time;

        _endButtons();
        _beginButtons();
    }
    else if (!strcmp_P_P(source, SPGM(http))) {
        setupWebServer();
    }
}

bool DimmerModulePlugin::hasReconfigureDependecy(PluginComponent *plugin) const
{
    return plugin->nameEquals(FSPGM(http));
}

void DimmerModulePlugin::shutdown()
{
    _end();
}

bool DimmerModulePlugin::hasWebUI() const
{
    return true;
}

WebUIInterface *DimmerModulePlugin::getWebUIInterface()
{
    return this;
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
    // row->addBadgeSensor(F("dimmer_frequency"), F("Dimmer Frequency"), F("Hz"));
    // row->addBadgeSensor(F("dimmer_int_temp"), F("Dimmer ATmega"), F("\u00b0C"));
    // row->addBadgeSensor(F("dimmer_ntc_temp"), F("Dimmer NTC"), F("\u00b0C"));
}


bool DimmerModulePlugin::hasStatus() const
{
    return true;
}

#if AT_MODE_SUPPORTED

#include "at_mode.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(DIMG, "DIMG", "Get level");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(DIMS, "DIMS", "<channel>,<level>[,<time>]", "Set level");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(DIMW, "DIMW", "Write EEPROM");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(DIMR, "DIMR", "Reset ATmega");

bool DimmerModulePlugin::hasAtMode() const
{
    return true;
}

void DimmerModulePlugin::atModeHelpGenerator()
{
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(DIMG), getName());
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(DIMS), getName());
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(DIMW), getName());
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(DIMR), getName());
}

bool DimmerModulePlugin::atModeHandler(AtModeArgs &args)
{
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(DIMW))) {
        writeEEPROM();
        args.ok();
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(DIMG))) {
        for(uint8_t i = 0; i < _channels.size(); i++) {
            args.printf_P(PSTR("%u: %d"), i, getChannel(i));
        }
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(DIMR))) {
        args.printf_P(PSTR("Pulling GPIO%u low for 10ms"), STK500V1_RESET_PIN);
        dimmer_plugin.resetDimmerMCU();
        args.printf_P(PSTR("GPIO%u set to input"), STK500V1_RESET_PIN);
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(DIMS))) {
        if (args.requireArgs(2, 2)) {
            size_t channel = args.toInt(0);
            if (channel >= 0 && channel < _channels.size()) {
                int level = args.toIntMinMax(1, 0, IOT_DIMMER_MODULE_MAX_BRIGHTNESS);
                float time = args.toFloat(2, -1);
                args.printf_P(PSTR("Set %u: %d (time %.2f)"), channel, level, time);
                setChannel(channel, level, time);
            }
            else {
                args.print(F("Invalid channel"));
            }
        }
        return true;
    }
    return false;
}

#endif
