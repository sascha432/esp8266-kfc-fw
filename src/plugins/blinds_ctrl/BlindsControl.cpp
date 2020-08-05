/**
 * Author: sascha_lammers@gmx.de
 */

#include "WebUISocket.h"
#include "BlindsControl.h"
#include "blinds_plugin.h"
#include "../src/plugins/mqtt/mqtt_client.h"

#if DEBUG_IOT_BLINDS_CTRL
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

extern int8_t operator *(const BlindsControl::ChannelType type);

int8_t operator *(const BlindsControl::ChannelType type) {
    return static_cast<int8_t>(type);
}

BlindsControl::BlindsControl() : MQTTComponent(ComponentTypeEnum_t::SWITCH), _queue(*this), _activeChannel(ChannelType::NONE)
{
}

void BlindsControl::_setup()
{
    __LDBG_println();
    _readConfig();
    for(auto pin : _config.pins) {
        digitalWrite(pin, LOW);
        pinMode(pin, OUTPUT);
    }

    analogWriteFreq(IOT_BLINDS_CTRL_PWM_FREQ);

#if IOT_BLINDS_CTRL_RPM_PIN
    attachScheduledInterrupt(digitalPinToInterrupt(IOT_BLINDS_CTRL_RPM_PIN), BlindsControl::rpmIntCallback, RISING);
    pinMode(IOT_BLINDS_CTRL_RPM_PIN, INPUT);
#endif
}

MQTTComponent::MQTTAutoDiscoveryPtr BlindsControl::nextAutoDiscovery(MQTTAutoDiscovery::FormatType format, uint8_t num)
{
    if (num >= getAutoDiscoveryCount()) {
        return nullptr;
    }
    auto discovery = new MQTTAutoDiscovery();
    if (num < kChannelCount) {
        ChannelType channel = (ChannelType)num;
        discovery->create(this, PrintString(FSPGM(channel__u, "channel_%u"), channel), format);
        discovery->addStateTopic(_getTopic(channel, TopicType::STATE));
        discovery->addCommandTopic(_getTopic(channel, TopicType::SET));
        discovery->addPayloadOn(1);
        discovery->addPayloadOff(0);
    }
    else if (num == kChannelCount) {
        discovery->create(this, FSPGM(channels), format);
        discovery->addStateTopic(_getTopic(ChannelType::ALL, TopicType::STATE));
        discovery->addCommandTopic(_getTopic(ChannelType::ALL, TopicType::SET));
        discovery->addPayloadOn(1);
        discovery->addPayloadOff(0);
    }
    else if (num == kChannelCount + 1) {
        discovery->create(MQTTComponent::ComponentType::SENSOR, FSPGM(binary), format);
        discovery->addStateTopic(_getTopic(ChannelType::NONE, TopicType::METRICS));
        discovery->addValueTemplate(FSPGM(binary));
    }
    else if (num == kChannelCount + 2) {
        discovery->create(MQTTComponent::ComponentType::SENSOR, FSPGM(busy, "busy"), format);
        discovery->addStateTopic(_getTopic(ChannelType::NONE, TopicType::METRICS));
        discovery->addValueTemplate(FSPGM(busy));
    }
    else if (num >= kChannelCount + 3) {
        ChannelType channel = (ChannelType)(num - (kChannelCount + 3));
        discovery->create(MQTTComponent::ComponentType::SENSOR, PrintString(FSPGM(channel__u), channel), format);
        discovery->addStateTopic(_getTopic(ChannelType::NONE, TopicType::CHANNELS));
        discovery->addValueTemplate(PrintString(FSPGM(channel__u), channel));
    }
    discovery->finalize();
    return discovery;
}

uint8_t BlindsControl::getAutoDiscoveryCount() const
{
    return kChannelCount * 2 + 3;
}

void BlindsControl::getValues(JsonArray &array)
{
    JsonUnnamedObject *obj;

    obj = &array.addObject(3);
    obj->add(JJ(id), FSPGM(set_all, "set_all"));
    obj->add(JJ(value), _states[0].isOpen() || _states[1].isOpen() ? 1 : 0);
    obj->add(JJ(state), true);

    for(const auto channel: _states.channels()) {

        String prefix = PrintString(FSPGM(channel__u), channel);

        obj = &array.addObject(3);
        obj->add(JJ(id), prefix + F("_state"));
        obj->add(JJ(value), _getStateStr(channel));
        obj->add(JJ(state), true);

        obj = &array.addObject(3);
        obj->add(JJ(id), prefix + F("_set"));
        obj->add(JJ(value), _states[channel].isOpen() ? 1 : 0);
        obj->add(JJ(state), true);

    }

}

void BlindsControl::onConnect(MQTTClient *client)
{
    __LDBG_printf("client=%p", client);
    _publishState(client);
    client->subscribe(this, _getTopic(ChannelType::ALL, TopicType::SET));
    for(const auto channel: _states.channels()) {
        client->subscribe(this, _getTopic(channel, TopicType::SET));
    }
}


void BlindsControl::onMessage(MQTTClient *client, char *topic, char *payload, size_t len)
{
    ChannelType channel;
    if (strstr_P(topic, PSTR("/channel_0/"))) {
        channel = ChannelType::CHANNEL0;
    }
    else if (strstr_P(topic, PSTR("/channel_1/"))) {
        channel = ChannelType::CHANNEL1;
    }
    else {
        channel = ChannelType::ALL;
    }

    auto state = atoi(payload); // payload is NUL terminated
    __LDBG_printf("topic=%s state=%u payload=%s", topic, state, payload);
    _executeAction(channel, state);
}


void BlindsControl::setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState)
{
    __LDBG_printf("id=%s hasValue=%u value=%s hasState=%u state=%u", id.c_str(), hasValue, value.c_str(), hasState, state);
    if (hasValue) {
        bool open = value.toInt();
        if (String_endsWith(id, PSTR("_set"))) {
            size_t channel = atoi(id.c_str() + id.indexOf('_') + 1);
            if (channel < _states.size()) {
                _executeAction(static_cast<ChannelType>(channel), open);
            }
        }
        else if (String_equals(id, SPGM(set_all))) {
            _executeAction(ChannelType::ALL, open);
        }
    }
}

void BlindsControl::_executeAction(ChannelType channel, bool open)
{
    __LDBG_printf("channel=%u open=%u queue=%u", channel, open, _queue.size());
    if (_queue.empty()) {
        // create queue from open or close automation
        auto &automation = open ? _config.open : _config.close;
        for(size_t i = 0; i < sizeof(automation) / sizeof(automation[0]); i++) {
            auto action = Actions::cast_enum_type(automation[i].type);
            if (action != ActionType::NONE) {
                uint16_t delay = automation[i].delay;
                __LDBG_printf("queue action=%u channel=%u delay=%u", action, channel, delay);
                _queue.emplace_back(action, channel, delay);
            }
        }
    }
    else {
        // abort current operation
        auto activeChannel = _activeChannel;
        _stop();
        _queue.clear();
        for(const auto channel: _states.channels()) {
            if (channel == activeChannel) {
                _states[channel] = StateType::STOPPED; // mark as stopped if it was running
            }
        }
        LoopFunctions::callOnce([this]() {
            _publishState();
            _saveState();
        });
    }
}

void BlindsControl::_startMotor(ChannelType channel, bool open)
{
    __LDBG_printf("channel=%u open=%u", channel, open);

    // disables all motor pins
    _stop();
    // set active channel
    _activeChannel = channel;

    auto &cfg = _config.channels[*channel];
    _setMotorSpeed(channel, cfg.pwm_value, open);
    _states[channel] = open ? StateType::OPEN : StateType::CLOSED;

    _publishState();

    _motorTimeout.set(open ? cfg.open_time : cfg.close_time);
    // clear ADC last
    _clearAdc();
}

void BlindsControl::_monitorMotor(ChannelAction &action)
{
    if (_motorTimeout.isActive()) {
        bool finished = false;

        _updateAdc();

        auto &_channel = _config.channels[*_activeChannel];
        uint16_t currentLimit = _channel.current_limit * BlindsControllerConversion::kConvertCurrentToADCValueMulitplier;
        // assuming this loop runs at least once per millisecond
        // we can use millis() and a counter to determine the time
        // measured ~250-270µs @ 80MHz
        if (_adcIntegral > currentLimit && millis() % 2 == _currentLimitCounter % 2) {
            if (++_currentLimitCounter > _channel.current_limit_time) {
                __LDBG_printf("current limit time=%u", _currentLimitCounter);
                finished = true;
            }
        }
        else if (_currentLimitCounter && _adcIntegral < currentLimit * 0.8) {   // reset current limit counter if current drops below 80%
            __LDBG_printf("current limit timer reset=%ums", _currentLimitCounter);
            _currentLimitCounter = 0;
        }

#if IOT_BLINDS_CTRL_RPM_PIN
        if (_hasStalled()) {
            __LDBG_printf("stalled");
            finished = true;
        }
#endif

        if (_motorTimeout.reached()) {
            __LDBG_printf("timeout");
            finished = true;
        }

        if (finished) {
            action.next();
            _stop();
            _publishState();
        }
    }
}

void BlindsControl::_loopMethod()
{
    if (!_queue.empty()) {
        ChannelType forChannel = ChannelType::NONE;
        ChannelType openChannel = ChannelType::NONE;
        ChannelType closeChannel = ChannelType::NONE;
        auto &action = _queue.getAction();
        auto channel = action.getChannel();
        switch(action.getState()) {
            case ActionStateType::NONE:
                switch(action.getAction()) { // translate action into which channel to check and which to open/close
                    case ActionType::OPEN_CHANNEL0:
                        forChannel = ChannelType::CHANNEL0;
                        openChannel = ChannelType::CHANNEL0;
                        break;
                    case ActionType::OPEN_CHANNEL1:
                        forChannel = ChannelType::CHANNEL1;
                        openChannel = ChannelType::CHANNEL1;
                        break;
                    case ActionType::OPEN_CHANNEL0_FOR_CHANNEL1:
                        forChannel = ChannelType::CHANNEL1;
                        openChannel = ChannelType::CHANNEL0;
                        break;
                    case ActionType::OPEN_CHANNEL1_FOR_CHANNEL0:
                        forChannel = ChannelType::CHANNEL0;
                        openChannel = ChannelType::CHANNEL1;
                        break;
                    case ActionType::CLOSE_CHANNEL0:
                        forChannel = ChannelType::CHANNEL0;
                        closeChannel = ChannelType::CHANNEL0;
                        break;
                    case ActionType::CLOSE_CHANNEL1:
                        forChannel = ChannelType::CHANNEL1;
                        closeChannel = ChannelType::CHANNEL1;
                        break;
                    case ActionType::CLOSE_CHANNEL0_FOR_CHANNEL1:
                        forChannel = ChannelType::CHANNEL1;
                        closeChannel = ChannelType::CHANNEL0;
                        break;
                    case ActionType::CLOSE_CHANNEL1_FOR_CHANNEL0:
                        forChannel = ChannelType::CHANNEL0;
                        closeChannel = ChannelType::CHANNEL1;
                        break;
                    default:
                        break;
                }
                __LDBG_printf("action=%d for=%d open=%d close=%d", action.getAction(), forChannel, openChannel, closeChannel);
                if (channel == forChannel || channel == ChannelType::ALL) {
                    if (openChannel != ChannelType::NONE && !_states[openChannel].isOpen()) {
                        action.begin();
                        _startMotor(openChannel, true);
                    }
                    else if (closeChannel != ChannelType::NONE && !_states[closeChannel].isClosed()) {
                        action.begin();
                        _startMotor(closeChannel, false);
                    }
                    else {
                        action.end();
                    }
                }
                else {
                    action.end();
                }
                break;
            case ActionStateType::WAIT_FOR_MOTOR:
                _monitorMotor(action);
                break;
            case ActionStateType::DELAY:
                action.monitorDelay();
                break;
            case ActionStateType::REMOVE:
                _queue.removeAction(action);
                if (_queue.empty()) {
                    _publishState();
                    _saveState();
                }
                break;
            default:
                __LDBG_panic("state=%u", action.getState());
                break;
        }
    }
}

BlindsControl::NameType BlindsControl::_getStateStr(ChannelType channel) const
{
    if (_queue.empty()) {
        return _states[channel]._getFPStr();
    }
    else if (_activeChannel == channel) {
        return FSPGM(Running, "Running");
    }
    else if (!_queue.empty() && _queue.front().getState() == ActionStateType::DELAY) {
        return F("Waiting");
    }
    return FSPGM(Busy);
}

void BlindsControl::_clearAdc()
{
    auto channel = *_activeChannel;
    __LDBG_printf("rssel=%u", channel == 0 && Plugins::Blinds::ConfigStructType::get_enum_multiplexer(_config) == Plugins::Blinds::MultiplexerType::HIGH_FOR_CHANNEL0 ? HIGH : LOW);

    // select shunt
    digitalWrite(_config.pins[4], channel == 0 && Plugins::Blinds::ConfigStructType::get_enum_multiplexer(_config) == Plugins::Blinds::MultiplexerType::HIGH_FOR_CHANNEL0 ? HIGH : LOW);
    delay(IOT_BLINDS_CTRL_RSSEL_WAIT);

    _adcIntegral = 0;
    _currentLimitCounter = 0;
    _currentTimer.start();

#if IOT_BLINDS_CTRL_RPM_PIN
    _rpmReset();
#endif
}

void BlindsControl::_updateAdc()
{
    uint16_t count = (_currentTimer.getTime() / IOT_BLINDS_CTRL_INT_TDIV);
    _currentTimer.start();
    _adcIntegral = ((_adcIntegral * count) + analogRead(A0)) / (count + 1);
}

void BlindsControl::_setMotorSpeed(ChannelType channelType, uint16_t speed, bool open)
{
    auto channel = *channelType;
    uint8_t pin1 = _config.pins[(channel * kChannelCount) + !open]; // first pin is open pin
    uint8_t pin2 = _config.pins[(channel * kChannelCount) + open]; // second is close pin

    __LDBG_printf("channel=%u speed=%u open=%u pins=%u,%u", channel, speed, open, pin1, pin2);

    analogWrite(pin1, speed);
    analogWrite(pin2, 0);
}

void BlindsControl::_stop()
{
    __LDBG_println();

    _motorTimeout.disable();
    _activeChannel = ChannelType::NONE;
    for(auto pin : _config.pins) {
        digitalWrite(pin, LOW);
    }
}

String BlindsControl::_getTopic(ChannelType channel, TopicType type)
{
    if (type == TopicType::METRICS) {
        return MQTTClient::formatTopic(FSPGM(state));
    }
    else if (type == TopicType::CHANNELS) {
        return MQTTClient::formatTopic(FSPGM(channels));
    }
    const __FlashStringHelper *str;
    switch(type) {
        case TopicType::SET:
            str = FSPGM(_set);
            break;
        case TopicType::STATE:
        default:
            str = FSPGM(_state);
            break;
    }
    if (channel == ChannelType::ALL) {
        return MQTTClient::formatTopic(str);
    }
    return MQTTClient::formatTopic(PrintString(FSPGM(channel__u), channel), str);
}

void BlindsControl::_publishState(MQTTClient *client)
{
    if (!client) {
        client = MQTTClient::getClient();
    }
    __LDBG_printf("state %s/%s, client %p", _getStateStr(ChannelType::CHANNEL0), _getStateStr(ChannelType::CHANNEL1), client);

    if (client) {
        JsonUnnamedObject metrics(2);
        JsonUnnamedObject channels(kChannelCount);
        String binaryState = String('b');

        for(const auto channel: _states.channels()) {
            auto &state = _states[channel];
            auto isOpen = state.getCharState();
            binaryState += isOpen;
            client->publish(_getTopic(channel, TopicType::STATE), true, String(isOpen));
            channels.add(PrintString(FSPGM(channel__u), channel), _getStateStr(channel));
        }
        metrics.add(FSPGM(binary), binaryState);
        metrics.add(FSPGM(busy), !_queue.empty());

        PrintString buffer;
        buffer.reserve(metrics.length());
        metrics.printTo(buffer);

        client->publish(_getTopic(ChannelType::NONE, TopicType::METRICS), true, buffer);

        buffer = PrintString();
        buffer.reserve(channels.length());
        channels.printTo(buffer);
        client->publish(_getTopic(ChannelType::NONE, TopicType::CHANNELS), true, buffer);
    }

    JsonUnnamedObject webUI(2);
    webUI.add(JJ(type), JJ(ue));
    getValues(webUI.addArray(JJ(events), kChannelCount * 2));
    WsWebUISocket::broadcast(WsWebUISocket::getSender(), webUI);
}

void BlindsControl::_loadState()
{
#if IOT_BLINDS_CTRL_SAVE_STATE
    auto file = SPIFFS.open(FSPGM(iot_blinds_control_state_file), fs::FileOpenMode::read);
    if (file) {
        file.read(reinterpret_cast<uint8_t *>(_states.data()), _states.size() * sizeof(*_states.data()));
    }
    __LDBG_printf("file=%u state=%u,%u", (bool)file, _states[0], _states[1]);
#endif
}

void BlindsControl::_saveState()
{
#if IOT_BLINDS_CTRL_SAVE_STATE
    auto file = SPIFFS.open(FSPGM(iot_blinds_control_state_file), fs::FileOpenMode::write);
    if (file) {
        file.write(reinterpret_cast<const uint8_t *>(_states.data()), _states.size() * sizeof(*_states.data()));
    }
    __LDBG_printf("file=%u state=%u,%u", (bool)file, _states[0], _states[1]);
#endif
}

void BlindsControl::_readConfig()
{
    _config = Plugins::Blinds::getConfig();
    _loadState();
}

#if IOT_BLINDS_CTRL_RPM_PIN

void BlindsControl::_rpmReset()
{
    _rpmTimer.start();
    _rpmLastInterrupt.start();
    _rpmCounter = 0;
}

void BlindsControl::_rpmIntCallback(const InterruptInfo &info)
{
    const uint8_t multiplier = IOT_BLINDS_CTRL_RPM_PULSES * 10;
    auto time = _rpmTimer.getTime(info.micro);
    _rpmLastInterrupt.start();
    _rpmTimer.start(info.micro);
    _rpmTimeIntegral = ((_rpmTimeIntegral * multiplier) + time) / (multiplier + 1);
    _rpmCounter++;
}

uint16_t BlindsControl::_getRpm()
{
    return (uint16_t)((1000000UL * 60UL / IOT_BLINDS_CTRL_RPM_PULSES) / _rpmTimeIntegral);
}

bool BlindsControl::_hasStalled()
{
    // interval for 500 rpm = ~40ms with 3 pulses
    return (_rpmLastInterrupt.getTime() > (1000000UL / 500UL/*rpm*/ * 60UL / IOT_BLINDS_CTRL_RPM_PULSES));
}

#endif
