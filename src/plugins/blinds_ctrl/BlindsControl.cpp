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

static constexpr float kAdcIntegralMultiplierDivider = 1000.0 * 10.0;

int8_t operator *(const BlindsControl::ChannelType type) {
    return static_cast<int8_t>(type);
}

BlindsControl::BlindsControl() :
    MQTTComponent(ComponentTypeEnum_t::SWITCH), _queue(*this),
    _activeChannel(ChannelType::NONE),
    _adcIntegralMultiplier(2500 / kAdcIntegralMultiplierDivider),
    _adc(ADCManager::getInstance())
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
    auto discovery = __LDBG_new(MQTTAutoDiscovery);
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

        client->publish(_getTopic(ChannelType::ALL, TopicType::STATE), true, String(_states[0].isOpen() || _states[1].isOpen() ? 1 : 0));

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

    if (WsWebUISocket::getWsWebUI() && WsWebUISocket::hasClients(WsWebUISocket::getWsWebUI())) {
        JsonUnnamedObject webUI(2);
        webUI.add(JJ(type), JJ(ue));
        getValues(webUI.addArray(JJ(events), kChannelCount * 2));
        WsWebUISocket::broadcast(WsWebUISocket::getSender(), webUI);
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

void BlindsControl::startToneTimer(uint32_t maxLength)
{
    BlindsControlPlugin::getInstance()._startToneTimer(maxLength);
}

void BlindsControl::_startToneTimer(uint32_t maxLength)
{
    if (_config.play_tone_channel == 0) {
        __LDBG_printf("tone timer disabled");
        return;
    }
    uint32_t counter = 0;
    static constexpr uint16_t tone_interval = 2000;
    static constexpr uint16_t tone_duration = 150;
    static constexpr uint32_t max_length = 120 * 10000; // stop after 120 seconds


    uint8_t channel = (_config.play_tone_channel - 1);
    uint8_t pin = _config.pins[(channel * kChannelCount) + _states[channel].isClosed() ? 1 : 0]; // use channel 1, and use open/close pin that is jammed
    if (maxLength == 0) {
        maxLength = max_length;
    }
    __LDBG_printf("start tone timer pin=%u channel=%u frequency=%u pwm=u interval=%u duration=%u max_duration=%.1fs", pin, channel, _config.tone_frequency, _config.tone_pwm_value, tone_interval, tone_duration, ((maxLength / tone_interval) * tone_interval) / 1000.0);

    _Timer(_toneTimer).add(tone_duration, true, [this, counter, pin, maxLength](Event::CallbackTimerPtr timer) mutable {
        if (timer == nullptr) {
            __LDBG_printf("disable tone signal pin=%u counter=%u max_duration=%u", pin, counter, maxLength);
            analogWrite(pin, 0);
            return;
        }
        uint16_t loop = counter % (tone_interval / tone_duration);
        if (loop == 0) {
            analogWriteFreq(_config.tone_frequency);
            analogWrite(pin, _config.tone_pwm_value);
        }
        else if (loop == 1) {
            analogWrite(pin, 0);
        }
        if (++counter > (maxLength / tone_duration)) {
            _stopToneTimer();
            return;
        }
    });
}

void BlindsControl::stopToneTimer()
{
    BlindsControlPlugin::getInstance()._stopToneTimer();
}

void BlindsControl::_stopToneTimer()
{
    __LDBG_printf("stopping tone timer");
    if (_toneTimer) {
        _toneTimer->_callback(nullptr); // disables the tone
        _toneTimer.remove();
        uint8_t pin = _config.pins[(1 * kChannelCount) + 1];
        analogWrite(pin, 0);
    }
}

void BlindsControl::_executeAction(ChannelType channel, bool open)
{
    __LDBG_printf("channel=%u open=%u queue=%u", channel, open, _queue.size());
    if (_queue.empty()) {
        // create queue from open or close automation
        auto &automation = open ? _config.open : _config.close;
        for(size_t i = 0; i < sizeof(automation) / sizeof(automation[0]); i++) {
            auto action = automation[i]._get_enum_action();
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
    // __LDBG_printf("channel=%u open=%u", channel, open);

    // disables all motor pins
    _stop();
    // set active channel
    _activeChannel = channel;

    auto &cfg = _config.channels[*channel];
    _currentLimit = cfg.current_limit_mA * BlindsControllerConversion::kConvertCurrentToADCValueMulitplier;

    _states[channel] = open ? StateType::OPEN : StateType::CLOSED;

    _publishState();

    _motorTimeout.set(open ? cfg.open_time_ms : cfg.close_time_ms);
    _adcIntegralMultiplier = cfg.current_avg_period_us / kAdcIntegralMultiplierDivider;
    // clear ADC last
    _clearAdc();

    if (_config.pwm_softstart_time > 0) {

        // soft start enabled
        _setMotorSpeed(channel, 0, open);

        // set timer, pwm and pin
        // the pwm value is updated in the loop function
        _motorStartTime = micros();
        if (_motorStartTime == 0) {
            _motorStartTime++;
        }
    }
    else {
        _setMotorSpeed(channel, cfg.pwm_value, open);
    }
    _motorPWMValue = cfg.pwm_value;
#if DEBUG_IOT_BLINDS_CTRL
    _softStartUpdateCount = 0;
#endif

    __LDBG_printf("pin=%u pwm=%u soft-start=%ums I-limit=%umA (%.2f) dir=%s adc-mul=%.6f timeout=%ums",
        _motorPin, _motorPWMValue, _config.pwm_softstart_time,
        cfg.current_limit_mA, _currentLimit,
        open ? PSTR("open") : PSTR("close"),
        _adcIntegralMultiplier,
        _motorTimeout.getTimeLeft()
    );
}

bool BlindsControl::_checkMotor()
{
    _updateAdc();
    if (_adcIntegral > _currentLimit) {
        __LDBG_printf("current limit time=%f @ %u ms", _adcIntegral, _motorTimeout.getDelay() - _motorTimeout.getTimeLeft());
        return true;
    }
    if (_motorTimeout.reached()) {
        __LDBG_printf("timeout");
        return true;
    }
#if IOT_BLINDS_CTRL_RPM_PIN
    if (_hasStalled()) {
        __LDBG_printf("stalled");
        return false;
    }
#endif
    return false;
}

void BlindsControl::_monitorMotor(ChannelAction &action)
{
    if (!_motorTimeout.isActive()) {
        __DBG_panic("motor timeout inactive");
    }
    if (_checkMotor()) {
        action.next();
        _stop();
        if (_cleanQueue()) {
            _saveState();
        }
        _publishState();
    }
}

BlindsControl::ActionToChannel::ActionToChannel(ActionType action, ChannelType channel) : ActionToChannel()
{
    switch(action) { // translate action into which channel to check and which to open/close
        case ActionType::OPEN_CHANNEL0:
            _for = channel == ChannelType::ALL ? ChannelType::ALL : ChannelType::CHANNEL0;
            _open = ChannelType::CHANNEL0;
            break;
        case ActionType::OPEN_CHANNEL1:
            _for = channel == ChannelType::ALL ? ChannelType::ALL : ChannelType::CHANNEL1;
            _open = ChannelType::CHANNEL1;
            break;
        case ActionType::OPEN_CHANNEL0_FOR_CHANNEL1:
            _for = ChannelType::CHANNEL1;
            _open = ChannelType::CHANNEL0;
            break;
        case ActionType::OPEN_CHANNEL1_FOR_CHANNEL0:
            _for = ChannelType::CHANNEL0;
            _open = ChannelType::CHANNEL1;
            break;
        case ActionType::CLOSE_CHANNEL0:
            _for = channel == ChannelType::ALL ? ChannelType::ALL : ChannelType::CHANNEL0;
            _close = ChannelType::CHANNEL0;
            break;
        case ActionType::CLOSE_CHANNEL1:
            _for = channel == ChannelType::ALL ? ChannelType::ALL : ChannelType::CHANNEL1;
            _close = ChannelType::CHANNEL1;
            break;
        case ActionType::CLOSE_CHANNEL0_FOR_CHANNEL1:
            _for = ChannelType::CHANNEL1;
            _close = ChannelType::CHANNEL0;
            break;
        case ActionType::CLOSE_CHANNEL1_FOR_CHANNEL0:
            _for = ChannelType::CHANNEL0;
            _close = ChannelType::CHANNEL1;
            break;
        default:
            break;
    }
}

bool BlindsControl::_cleanQueue()
{
    // check queue for any actions left otherwise clear it
    for(auto &action: _queue) {
        switch(action.getState()) {
            case ActionStateType::NONE: {
                auto channel = action.getChannel();
                ActionToChannel channels(action.getAction(), channel);
                __LDBG_printf("action=%d channel=%u for=%d open=%d close=%d channel0=%s channel1=%s", action.getAction(), channel, channels._for, channels._open, channels._close, _states[0]._getFPStr(), _states[1]._getFPStr());
                if (channel == channels._for) {
                    if (channels.isOpenValid() && !_states[channels._open].isOpen()) {
                        return false;
                    }
                    else if (channels.isCloseValid() && !_states[channels._close].isClosed()) {
                        return false;
                    }
                }
            } break;
            case ActionStateType::START_MOTOR:
            case ActionStateType::WAIT_FOR_MOTOR:
                return false;
            case ActionStateType::DELAY:
            case ActionStateType::REMOVE:
                // remove finished actions and delays
                break;
            default:
                __LDBG_panic("state=%u", action.getState());
                break;
        }
    }
    __DBG_printf("clear queue size=%u", _queue.size());
    _queue.clear();
    return true;
}

void BlindsControl::_loopMethod()
{
    // soft start is running, update PWM
    if (_motorStartTime) {
        uint32_t diff = get_time_diff(_motorStartTime, micros());
        uint32_t stime = _config.pwm_softstart_time * 1000U;
        if (diff >= stime) { // finished, set final pwm value
            analogWrite(_motorPin, _motorPWMValue);
            __LDBG_printf("soft start finished time=%u pin=%u pwm=%u updates=%u", diff, _motorPin, _motorPWMValue, _softStartUpdateCount);
            _motorStartTime = 0;
        }
        else {
            analogWrite(_motorPin, _motorPWMValue * diff / stime);
#if DEBUG_IOT_BLINDS_CTRL
            _softStartUpdateCount++;
#endif
        }
    }
    if (_queue.empty()) {
        // if the queue is empty check motor timeout only
        // this also updates the ADC readings and checks the current limit
        if (_motorTimeout.isActive()) {
            if (_checkMotor()) {
                _stop();
            }
        }
    }
    else {
        auto &action = _queue.getAction();
        auto channel = action.getChannel();
        switch(action.getState()) {
            case ActionStateType::NONE: {
                ActionToChannel channels(action.getAction(), channel);
                __LDBG_printf("action=%d channel=%u for=%d open=%d close=%d channel0=%s channel1=%s", action.getAction(), channel, channels._for, channels._open, channels._close, _states[0]._getFPStr(), _states[1]._getFPStr());
                if (channel == channels._for) {
                    if (channels.isOpenValid() && !_states[channels._open].isOpen()) {
                        action.begin(channels._open, true);
                    }
                    else if (channels.isCloseValid() && !_states[channels._close].isClosed()) {
                        action.begin(channels._close, false);
                    }
                    else {
                        action.end();
                    }
                }
                else {
                    action.end();
                }
            } break;
            case ActionStateType::START_MOTOR:
                _startMotor(channel, action.getOpen());
                action.next();
                break;
            case ActionStateType::WAIT_FOR_MOTOR:
                _monitorMotor(action);
                break;
            case ActionStateType::DELAY:
                action.monitorDelay();
                break;
            case ActionStateType::REMOVE:
                _queue.removeAction(action);
                if (_cleanQueue()) {
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
    else if (!_queue.empty() && _queue.getAction().getState() == ActionStateType::DELAY) {
        return F("Waiting");
    }
    return FSPGM(Busy);
}

void BlindsControl::_clearAdc()
{
    auto channel = *_activeChannel;

    stopToneTimer();

    static const uint32_t kMaxFrequency = 40000;
    analogWriteRange(PWMRANGE);
    analogWriteFreq(kMaxFrequency); // set to max. for the DAC
    analogWrite(_config.pins[5], _config.channels[channel].dac_pwm_value);
    __LDBG_printf("dac pin=%u pwm=%u frequency=%u", _config.pins[5], _config.channels[channel].dac_pwm_value, kMaxFrequency);

    analogWriteFreq(_config.pwm_frequency);


    uint8_t state = (channel == _config.adc_multiplexer) ? HIGH : LOW;
    digitalWrite(_config.pins[4], state);
    __LDBG_printf("multiplexer pin=%u channel=%u multiplexer=%u state=%u delay=%u", _config.pins[4], channel, _config.adc_multiplexer, state, IOT_BLINDS_CTRL_SETUP_DELAY);

    delay(IOT_BLINDS_CTRL_SETUP_DELAY);

    _adcLastUpdate = 0;
    _adcIntegral = 0;
    _currentTimer.start();

#if IOT_BLINDS_CTRL_RPM_PIN
    _rpmReset();
#endif
}

void BlindsControl::_updateAdc()
{
    auto micros = _currentTimer.getTime();
    if (micros < ADCManager::kMinDelayMicros) {
        return;
    }
    uint32_t tmp;
    auto reading = _adc.readValue(tmp);
    if (tmp == _adcLastUpdate) {
        return;
    }
    _adcLastUpdate = tmp;

    _currentTimer.start();
    float count = micros * _adcIntegralMultiplier;
    _adcIntegral = ((count * _adcIntegral) + reading) / (count + 1.0f);
}

void BlindsControl::_setMotorBrake(ChannelType channel)
{
    // shorts motor wires and brakes in slow decay mode
    // both pins low leaves motor wires floating -> _setMotorSpeed(channel, 0, ...)

    __LDBG_printf("channel=%u braking pins=%u,%u", channel, *(&_config.pins[(*channel * kChannelCount)]), *(&_config.pins[(*channel * kChannelCount)] + 1));

    _motorStartTime = 0;
    _motorPWMValue = 0;
    // make sure both pins are set to high at exactly the same time
    noInterrupts();
    auto ptr = &_config.pins[(*channel * kChannelCount)];
    digitalWrite(*ptr++, HIGH);
    digitalWrite(*ptr, HIGH);
    interrupts();
}

void BlindsControl::_setMotorSpeed(ChannelType channelType, uint16_t speed, bool open)
{
    auto channel = *channelType;
    _motorPin = _config.pins[(channel * kChannelCount) + !open]; // first pin is open pin
    uint8_t pin2 = _config.pins[(channel * kChannelCount) + open]; // second is close pin
#if DEBUG_IOT_BLINDS_CTRL
    __LDBG_printf("channel=%u speed=%u open=%u pins=%u,%u frequency=%u", channel, speed, open, _motorPin, pin2, _config.pwm_frequency);
#endif

    _motorStartTime = 0;
    analogWriteRange(PWMRANGE);
    analogWriteFreq(_config.pwm_frequency);

    noInterrupts();
    if (speed == 0) {
        digitalWrite(_motorPin, LOW);
        _motorPWMValue = 0;
    }
    else {
        analogWrite(_motorPin, speed);
    }
    digitalWrite(pin2, LOW);
    interrupts();
}

void BlindsControl::_stop()
{
    _stopToneTimer();

    __LDBG_printf("pwm=%u pin=%u adc=%u stopping motor", _motorPWMValue, _motorPin, (int)_adcIntegral);

    _motorStartTime = 0;
    _motorPWMValue = 0;
    _motorTimeout.disable();

    _activeChannel = ChannelType::NONE;
    // do not allow interrupts when changing a pin pair from high/high to low/low and vice versa
    noInterrupts();
    for(auto pin : _config.pins) {
        digitalWrite(pin, LOW);
    }
    interrupts();

    _adcIntegral = 0;
}

String BlindsControl::_getTopic(ChannelType channel, TopicType type)
{
    if (type == TopicType::METRICS) {
        return MQTTClient::formatTopic(FSPGM(metrics));
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

void BlindsControl::_loadState()
{
#if IOT_BLINDS_CTRL_SAVE_STATE
    auto file = KFCFS.open(FSPGM(iot_blinds_control_state_file), fs::FileOpenMode::read);
    if (file) {
        file.read(reinterpret_cast<uint8_t *>(_states.data()), _states.size() * sizeof(*_states.data()));
    }
    __LDBG_printf("file=%u state=%u,%u", (bool)file, _states[0], _states[1]);
#endif
}

void BlindsControl::_saveState()
{
#if IOT_BLINDS_CTRL_SAVE_STATE
    auto file = KFCFS.open(FSPGM(iot_blinds_control_state_file), fs::FileOpenMode::write);
    if (file) {
        file.write(reinterpret_cast<const uint8_t *>(_states.data()), _states.size() * sizeof(*_states.data()));
    }
    __LDBG_printf("file=%u state=%u,%u", (bool)file, _states[0], _states[1]);
#endif
}

void BlindsControl::_readConfig()
{
    _config = Plugins::Blinds::getConfig();
    _adcIntegralMultiplier = _config.channels[0].current_avg_period_us / kAdcIntegralMultiplierDivider;

    _adc.setOffset(_config.adc_offset);
    _adc.setMinDelayMicros(_config.adc_read_interval);
    _adc.setMaxDelayMicros(_config.adc_recovery_time);
    _adc.setRepeatMaxDelayPerSecond(_config.adc_recoveries_per_second);
    _adc.setMaxDelayYieldTimeMicros(std::min<uint16_t>(1000, _config.adc_recovery_time / 2));

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
