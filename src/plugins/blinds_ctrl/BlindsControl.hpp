/**
 * Author: sascha_lammers@gmx.de
 */

#include "BlindsControl.h"
#include "WebUISocket.h"
#include "../src/plugins/mqtt/mqtt_client.h"

// ------------------------------------------------------------------------
// BlindsControl::ChannelState
// ------------------------------------------------------------------------

inline BlindsControl::ChannelState::ChannelState() :
    _state(StateType::UNKNOWN)
{
}

inline BlindsControl::ChannelState &BlindsControl::ChannelState::operator=(StateType state)
{
    _state = state;
    return *this;
}

// inline void BlindsControl::ChannelState::setToggleOpenClosed(StateType state)
// {
//     _state = (state == StateType::OPEN ? StateType::CLOSED : StateType::OPEN);
// }

inline BlindsControl::StateType BlindsControl::ChannelState::getState() const
{
    return _state;
}
// return binary state as char
inline char BlindsControl::ChannelState::getCharState() const
{
    return isOpen() ? '1' : '0';
}

inline bool BlindsControl::ChannelState::isOpen() const
{
    return _state == StateType::OPEN;
}

inline uint8_t BlindsControl::ChannelState::isOpenInt() const
{
    return _state == StateType::OPEN ? 1 : 0;
}

inline bool BlindsControl::ChannelState::isClosed() const
{
    return _state == StateType::CLOSED;
}

inline BlindsControl::NameType BlindsControl::ChannelState::_getFPStr() const
{
    return __getFPStr(_state);
}

inline BlindsControl::NameType BlindsControl::ChannelState::__getFPStr(StateType state)
{
    switch(state) {
        case StateType::OPEN:
            return FSPGM(Open);
        case StateType::CLOSED:
            return FSPGM(Closed);
        case StateType::STOPPED:
            return FSPGM(Stopped);
        default:
            break;
    }
    return FSPGM(n_a);
}

// ------------------------------------------------------------------------
// BlindsControl::ChannelAction
// ------------------------------------------------------------------------

inline BlindsControl::ChannelAction::ChannelAction(uint32_t startTime, uint16_t delay, ActionType state, ChannelType channel, PlayToneType playTone, bool relativeDelay) :
    _startTime(startTime),
    _delay(delay),
    _state(ActionStateType::NONE),
    _action(state),
    _channel(channel),
    _playTone(playTone),
    _relativeDelay(relativeDelay),
    _open(false)
{
}

inline BlindsControl::ChannelType BlindsControl::ChannelAction::getChannel() const
{
    return _channel;
}

inline BlindsControl::ActionType BlindsControl::ChannelAction::getAction() const
{
    return _action;
}

inline BlindsControl::ActionStateType BlindsControl::ChannelAction::getState() const
{
    return _state;
}

inline BlindsControl::PlayToneType BlindsControl::ChannelAction::getPlayTone() const
{
    return _playTone;
}

inline uint32_t BlindsControl::ChannelAction::getDelay() const
{
    return _delay * 1000U;
}

inline uint32_t BlindsControl::ChannelAction::getTimeout() const
{
    if (_state != ActionStateType::DELAY) {
        return 0;
    }
    return _startTime + getDelay();
}

inline bool BlindsControl::ChannelAction::isRelativeDelay() const
{
    return _relativeDelay;
}

inline bool BlindsControl::ChannelAction::getOpen() const
{
    return _open;
}

inline void BlindsControl::ChannelAction::monitorDelay()
{
    if (_state == ActionStateType::DELAY && get_time_diff(_startTime, millis()) >= getDelay()) {
        __LDBG_printf("start=%u end=%u delay=%u diff=%u relative=%u finished", _startTime, getTimeout(), getDelay(), get_time_diff(_startTime, millis()), isRelativeDelay());
        next();
    }
}

inline void BlindsControl::ChannelAction::beginDoNothing(ChannelType channel)
{
    __LDBG_printf("begin=%u channel=%u do_nothing", _state, channel);
    _state = ActionStateType::START_DELAY;
}

inline void BlindsControl::ChannelAction::begin(ChannelType channel, bool open)
{
    __LDBG_printf("begin=%u channel=%u open=%u", _state, channel, open);
    // update channel to change and the state
    _open = open;
    _channel = channel;
    next();
}

inline void BlindsControl::ChannelAction::next()
{
    BlindsControl::stopToneTimer();
    __LDBG_printf("next=%u", _state);
    switch (_state) {
        case ActionStateType::NONE:
            _state = ActionStateType::START_MOTOR;
            break;
        case ActionStateType::START_MOTOR:
            _state = ActionStateType::WAIT_FOR_MOTOR;
            break;
        case ActionStateType::START_DELAY:
        case ActionStateType::WAIT_FOR_MOTOR:
            if (_delay) {
                _state = ActionStateType::DELAY;
                if (isRelativeDelay()) {
                    _startTime = millis();
                }
                __LDBG_printf("start=%u end=%u delay=%u relative=%u play_tone=%u", _startTime, getTimeout(), _delay, _relativeDelay, _playTone);
                break;
            }
            // fallthrough
        default:
            end();
            break;
    }
}

inline void BlindsControl::ChannelAction::end()
{
    BlindsControl::stopToneTimer(_state);
    __LDBG_printf("end=%u", _state);
    _delay = 0;
    _state = ActionStateType::REMOVE;
}


// ------------------------------------------------------------------------
// BlindsControl::ChannelQueue
// ------------------------------------------------------------------------

inline BlindsControl::ChannelQueue::ChannelQueue(BlindsControl &control) :
    Type(),
    _control(control),
    _startTime(millis())
{
}

inline void BlindsControl::ChannelQueue::push_back(ChannelAction &&action)
{
    if (empty()) {
        resetStartTime();
    }
    emplace_back(std::move(action));
}

inline BlindsControl::ChannelAction &BlindsControl::ChannelQueue::getAction()
{
    return front();
}

inline const BlindsControl::ChannelAction &BlindsControl::ChannelQueue::getAction() const
{
    return front();
}

inline void BlindsControl::ChannelQueue::removeAction(const ChannelAction &action)
{
    __LDBG_printf("remove action=%p", &action);
    erase(std::remove_if(begin(), end(), [&action](const ChannelAction &_action) {
        return &action == &_action;
    }), end());
}

inline void BlindsControl::ChannelQueue::resetStartTime(uint32_t startTime)
{
    _startTime = startTime;
}

inline uint32_t BlindsControl::ChannelQueue::getStartTime() const
{
    return _startTime;
}

// ------------------------------------------------------------------------
// BlindsControl::ActionToChannel
// ------------------------------------------------------------------------

inline BlindsControl::ActionToChannel::ActionToChannel() :
    _for(ChannelType::NONE),
    _open(ChannelType::NONE),
    _close(ChannelType::NONE)
{
}

inline BlindsControl::ActionToChannel::ActionToChannel(ActionType action, ChannelType channel) : ActionToChannel()
{
    switch(action) { // translate action into which channel to check and which to open/close
        case ActionType::DO_NOTHING:
            _for = ChannelType::ALL;
            _open = ChannelType::NONE;
            _close = ChannelType::NONE;
            break;
        case ActionType::DO_NOTHING_CHANNEL0:
            _for = ChannelType::CHANNEL0;
            _open = ChannelType::NONE;
            _close = ChannelType::NONE;
            break;
        case ActionType::DO_NOTHING_CHANNEL1:
            _for = ChannelType::CHANNEL1;
            _open = ChannelType::NONE;
            _close = ChannelType::NONE;
            break;
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

inline bool BlindsControl::ActionToChannel::isOpenValid() const
{
    return _open != ChannelType::NONE;
}

inline bool BlindsControl::ActionToChannel::isCloseValid() const
{
    return _close != ChannelType::NONE;
}

inline bool BlindsControl::ActionToChannel::isDoNothing() const
{
    return _close == ChannelType::NONE && _open == ChannelType::NONE;
}

// ------------------------------------------------------------------------
// BlindsControl
// ------------------------------------------------------------------------

inline BlindsControl::BlindsControl() :
    MQTTComponent(ComponentType::SWITCH), _queue(*this),
    _activeChannel(ChannelType::NONE),
    _adcIntegralMultiplier(2500 / kAdcIntegralMultiplierDivider),
    _adc(ADCManager::getInstance())
{
}

inline void BlindsControl::_setup()
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

inline void BlindsControl::_startTone()
{
    __LDBG_printf("start=%u tone=%d", !_queue.empty() && _queue.getAction().getState() == ActionStateType::DELAY, _queue.empty() ? -1 : (int)_queue.getAction().getPlayTone());
    if (!_queue.empty() && _queue.getAction().getState() == ActionStateType::DELAY) {
        switch(_queue.getAction().getPlayTone()) {
            case PlayToneType::INTERVAL:
                BlindsControl::startToneTimer();
                break;
            case PlayToneType::INTERVAL_SPEED_UP:
                BlindsControl::startToneTimer(_queue.getAction().getTimeout());
                break;
#if HAVE_IMPERIAL_MARCH
            case PlayToneType::IMPERIAL_MARCH:
                BlindsControl::playImperialMarch(80, 0, 1);
                break;
#endif
            default:
                break;
        }
    }
    else if (_toneTimer) {
        __LDBG_printf("tone timer running");
        _stopToneTimer();
    }
}

inline void BlindsControl::_publishState()
{
    __LDBG_printf("state %s/%s", _getStateStr(ChannelType::CHANNEL0), _getStateStr(ChannelType::CHANNEL1));

    _startTone();

    if (isConnected()) {

        using namespace MQTT::Json;
        UnnamedObject channels;
        String binaryState = String('b');

        // __LDBG_printf("topic=%s payload=%s", _getTopic(ChannelType::ALL, TopicType::STATE).c_str(), MQTT::Client::toBoolOnOff(_states[0].isOpen() || _states[1].isOpen()));
        publish(_getTopic(ChannelType::ALL, TopicType::STATE), true, MQTT::Client::toBoolOnOff(_states[0].isOpen() || _states[1].isOpen()));

        for(const auto channel: _states.channels()) {
            auto &state = _states[channel];
            binaryState += state.getCharState();
            // __LDBG_printf("topic=%s payload=%s", _getTopic(channel, TopicType::STATE).c_str(), MQTT::Client::toBoolOnOff(state.isOpen()));
            publish(_getTopic(channel, TopicType::STATE), true, MQTT::Client::toBoolOnOff(state.isOpen()));
            channels.append(NamedStoredString(PrintString(FSPGM(channel__u), channel), _getStateStr(channel)));
        }

        // __LDBG_printf("topic=%s payload=%s", _getTopic(ChannelType::NONE, TopicType::METRICS).c_str(), UnnamedObject(NamedString(FSPGM(binary), binaryState), NamedBool(FSPGM(busy), !_queue.empty())).toString().c_str());
        publish(_getTopic(ChannelType::NONE, TopicType::METRICS), true, UnnamedObject(
            NamedString(FSPGM(binary), binaryState),
            NamedBool(FSPGM(busy), !_queue.empty())
        ).toString());

        // __LDBG_printf("topic=%s payload=%s", _getTopic(ChannelType::NONE, TopicType::CHANNELS).c_str(), channels.toString().c_str());
        publish(_getTopic(ChannelType::NONE, TopicType::CHANNELS), true, channels.toString());
    }


    if (WebUISocket::hasAuthenticatedClients()) {
        WebUINS::Events events;
        getValues(events);
        WebUISocket::broadcast(WebUISocket::getSender(), WebUINS::UpdateEvents(events));
    }
}


inline MQTT::AutoDiscovery::EntityPtr BlindsControl::getAutoDiscovery(FormatType format, uint8_t num)
{
    auto discovery = new MQTT::AutoDiscovery::Entity();
    if (num < kChannelCount) {
        ChannelType channel = (ChannelType)num;
        if (discovery->create(this, PrintString(FSPGM(channel__u, "channel_%u"), channel), format)) {
            discovery->addStateTopic(_getTopic(channel, TopicType::STATE));
            discovery->addCommandTopic(_getTopic(channel, TopicType::SET));
        }
    }
    else if (num == kChannelCount) {
        if (discovery->create(this, FSPGM(channels), format)) {
            discovery->addStateTopic(_getTopic(ChannelType::ALL, TopicType::STATE));
            discovery->addCommandTopic(_getTopic(ChannelType::ALL, TopicType::SET));
        }
    }
    else if (num == kChannelCount + 1) {
        if (discovery->create(MQTTComponent::ComponentType::SENSOR, FSPGM(binary), format)) {
            discovery->addStateTopic(_getTopic(ChannelType::NONE, TopicType::METRICS));
            discovery->addValueTemplate(FSPGM(binary));
        }
    }
    else if (num == kChannelCount + 2) {
        if (discovery->create(MQTTComponent::ComponentType::SENSOR, FSPGM(busy, "busy"), format)) {
            discovery->addStateTopic(_getTopic(ChannelType::NONE, TopicType::METRICS));
            discovery->addValueTemplate(FSPGM(busy));
        }
    }
    else if (num >= kChannelCount + 3) {
        ChannelType channel = (ChannelType)(num - (kChannelCount + 3));
        if (discovery->create(MQTTComponent::ComponentType::SENSOR, PrintString(FSPGM(channel__u), channel), format)) {
            discovery->addStateTopic(_getTopic(ChannelType::NONE, TopicType::CHANNELS));
            discovery->addValueTemplate(PrintString(FSPGM(channel__u), channel));
        }
    }
    return discovery;
}

inline void BlindsControl::onConnect()
{
    subscribe(_getTopic(ChannelType::ALL, TopicType::SET));
    for(const auto channel: _states.channels()) {
        subscribe(_getTopic(channel, TopicType::SET));
    }
    _publishState();
}

inline void BlindsControl::onMessage(const char *topic, const char *payload, size_t len)
{
    ChannelType channel;

    if (String(F("/channel_0/")).endEquals(topic)) {
        channel = ChannelType::CHANNEL0;
    }
    else if (String(F("/channel_1/")).endEquals(topic)) {
        channel = ChannelType::CHANNEL1;
    }
    else {
        channel = ChannelType::ALL;
    }
    auto state = MQTT::Client::toBool(payload, false);
    __LDBG_printf("topic=%s state=%u payload=%s", topic, state, payload);
    _executeAction(channel, state);
}


inline BlindsControl::NameType BlindsControl::_getStateStr(ChannelType channel) const
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

inline uint8_t BlindsControl::getAutoDiscoveryCount() const
{
    return kChannelCount * 2 + 3;
}

inline void BlindsControl::getValues(WebUINS::Events &array)
{
    array.append(WebUINS::Values(FSPGM(set_all, "set_all"), (_states[0].isOpen() || _states[1].isOpen()) ? 1 : 0, true));
    for(const auto channel: _states.channels()) {
        String prefix = PrintString(FSPGM(channel__u), channel);
        array.append(
            WebUINS::Values(prefix + F("_state"), _getStateStr(channel), true),
            WebUINS::Values(prefix + F("_set"), _states[channel].isOpenInt(), true)
        );
    }
}

inline String BlindsControl::_getTopic(ChannelType channel, TopicType type)
{
    if (type == TopicType::METRICS) {
        return MQTT::Client::formatTopic(FSPGM(metrics));
    }
    else if (type == TopicType::CHANNELS) {
        return MQTT::Client::formatTopic(FSPGM(channels));
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
        return MQTT::Client::formatTopic(str);
    }
    return MQTT::Client::formatTopic(PrintString(FSPGM(channel__u), channel), str);
}

inline void BlindsControl::setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState)
{
    __LDBG_printf("id=%s has_value=%u value=%s has_state=%u state=%u", id.c_str(), hasValue, value.c_str(), hasState, state);
    if (hasValue) {
        auto open = static_cast<bool>(value.toInt());
        if (id.endsWith(F("_set"))) {
            auto channel = static_cast<uint8_t>(atoi(id.c_str() + id.indexOf('_') + 1));
            if (channel < _states.size()) {
                _executeAction(static_cast<ChannelType>(channel), open);
            }
        }
        else if (id.endsWith(FSPGM(set_all))) {
            _executeAction(ChannelType::ALL, open);
        }
    }
}

inline void BlindsControl::_playTone(uint8_t *pins, uint16_t pwm, uint32_t frequency)
{
    if (*pins == kInvalidPin) {
        return;
    }
    __LDBG_printf("tone pins=%u,%u pwm=%u freq=%u", pins[0], pins[1], pwm, frequency);
    analogWriteFreq(frequency);
    analogWrite(pins[0], pwm);
#if !defined(ESP8266)
    if (pins[1] != kInvalidPin)
#endif
    {
        analogWrite(pins[1], pwm);
    }
}

inline void BlindsControl::_setDac(uint16_t pwm)
{
    analogWriteRange(PWMRANGE);
    analogWriteFreq(kPwmMaxFrequency);
    analogWrite(_config.pins[kDACPin], pwm);
    __LDBG_printf("dac pin=%u pwm=%u frequency=%u", _config.pins[kDACPin], pwm, kPwmMaxFrequency);
}

inline void BlindsControl::_playNote(uint8_t pin, uint16_t pwm, uint8_t note)
{
    uint8_t tmp = note + MPERIAL_MARCH_NOTE_OFFSET;
    if (tmp < NOTE_TO_FREQUENCY_COUNT) {
        uint8_t pins[2] = { pin, kInvalidPin };
        _playTone(pins, pwm, NOTE_FP_TO_INT(pgm_read_word(note_to_frequency + tmp)));
    }
}

inline void BlindsControl::_stopToneTimer()
{
    __LDBG_printf("stopping tone timer and disabling motors");
    _toneTimer.remove();
    _disableMotors();
}

inline void BlindsControl::_disableMotors()
{
    // do not allow interrupts when changing a pin pair from high/high to low/low and vice versa
    noInterrupts();

// #if defined(ESP8266)
//     using BlindsConfig = Plugins::BlindsConfig;
//     // clear motors pins. gpio16 is not supported and stopWaveForm(...) must be called to disable PWM
//     GPOC |= (
//         _BV(_config.pins[BlindsConfig::kChannel0_OpenPinArrayIndex]) |
//         _BV(_config.pins[BlindsConfig::kChannel0_ClosePinArrayIndex]) |
//         _BV(_config.pins[BlindsConfig::kChannel1_OpenPinArrayIndex]) |
//         _BV(_config.pins[BlindsConfig::kChannel1_ClosePinArrayIndex])
//     ) & 0x7fffU;
// #endif

    for(uint8_t i = 0; i < 2 * kChannelCount; i++) {
        digitalWrite(_config.pins[i], LOW);
    }
    interrupts();
}

inline void BlindsControl::_stop()
{
    _stopToneTimer();

    __LDBG_printf("pwm=%u pin=%u adc=%.2f peak=%.2f stopping motor", _motorPWMValue, _motorPin, _adcIntegral, _adcIntegralPeak);

    _motorStartTime = 0;
    _motorPWMValue = 0;
    _motorTimeout.disable();

    _activeChannel = ChannelType::NONE;
    _disableMotors();
    digitalWrite(_config.pins[kMultiplexerPin], LOW);
    digitalWrite(_config.pins[kDACPin], LOW);

    _adcIntegral = 0;
}

inline void BlindsControl::_loadState()
{
#if IOT_BLINDS_CTRL_SAVE_STATE
    auto file = KFCFS.open(FSPGM(iot_blinds_control_state_file), fs::FileOpenMode::read);
    if (file) {
        // file.read(reinterpret_cast<uint8_t *>(_states.data()), _states.size() * sizeof(*_states.data()));
        file.read(_states, _states.sizeInBytes());
    }
    __LDBG_printf("file=%u state=%u,%u", (bool)file, _states[0], _states[1]);
#endif
}

inline void BlindsControl::_saveState()
{
#if IOT_BLINDS_CTRL_SAVE_STATE
    auto file = KFCFS.open(FSPGM(iot_blinds_control_state_file), fs::FileOpenMode::read);
    if (file) {
        decltype(_states) states;
        if (file.read(states, states.sizeInBytes()) == states.sizeInBytes()) {
            if (memcmp(&states, &_states, sizeof(states)) == 0) {
                __LDBG_printf("file=%u state=%u,%u skipping save, no changes", (bool)file, _states[0], _states[1]);
                return;
            }
        }
    }
    file = KFCFS.open(FSPGM(iot_blinds_control_state_file), fs::FileOpenMode::write);
    if (file) {
        //file.write(reinterpret_cast<const uint8_t *>(_states.data()), _states.size() * sizeof(*_states.data()));
        file.write(_states, _states.sizeInBytes());
    }
    __LDBG_printf("file=%u state=%u,%u", (bool)file, _states[0], _states[1]);
#endif
}

inline void BlindsControl::_saveState(StateType *channels, uint8_t numChannels)
{
#if IOT_BLINDS_CTRL_SAVE_STATE
    ChannelStateArray<kChannelCount> _states;

    for(uint8_t i = 0; i < numChannels; i++) {
        _states[i] = std::clamp(channels[i], StateType::UNKNOWN, StateType::CLOSED);
    }

    auto file = KFCFS.open(FSPGM(iot_blinds_control_state_file), fs::FileOpenMode::write);
    if (file) {
        // file.write(reinterpret_cast<const uint8_t *>(_states.data()), _states.size() * sizeof(*_states.data()));
        file.write(_states, _states.sizeInBytes());
    }
#endif
}

inline void BlindsControl::_readConfig()
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
