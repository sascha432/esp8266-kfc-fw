/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "BlindsControl.h"
#include "WebUISocket.h"
#include "../src/plugins/mqtt/mqtt_client.h"
#include "../src/plugins/mqtt/mqtt_json.h"

using KFCConfigurationClasses::System;
using Plugins = KFCConfigurationClasses::PluginsType;

inline BlindsControl::BlindsControl() :
    MQTTComponent(ComponentType::SWITCH),
    _queue(*this),
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
    auto baseTopic = MQTT::Client::getBaseTopicPrefix();
    switch(static_cast<AutoDiscoveryType>(num)) {
        case AutoDiscoveryType::CHANNEL_0:
        case AutoDiscoveryType::CHANNEL_1: {
                auto channel = static_cast<ChannelType>(num);
                if (discovery->create(this, baseTopic + PrintString(FSPGM(channel__u, "channel_%u"), channel), format)) {
                    discovery->addStateTopic(_getTopic(channel, TopicType::STATE));
                    discovery->addCommandTopic(_getTopic(channel, TopicType::SET));
                    auto name = Plugins::Blinds::getChannelName(static_cast<size_t>(channel));
                    discovery->addName(name);
                    discovery->addObjectId(baseTopic + PrintString(FSPGM(channel__u, "channel_%u"), channel));
                }
            } break;
        case AutoDiscoveryType::CHANNELS_ALL:
            if (discovery->create(this, baseTopic + FSPGM(channels), format)) {
                discovery->addStateTopic(_getTopic(ChannelType::ALL, TopicType::STATE));
                discovery->addCommandTopic(_getTopic(ChannelType::ALL, TopicType::SET));
                discovery->addName(F("Both Channels"));
                discovery->addObjectId(baseTopic + F("both_channels"));
                auto icon = PrintString(
                    F("{%%- if is_state('switch.%s', 'on') -%%}mdi:curtains{%%- else -%%}mdi:curtains-closed{%%- endif -%%}"),
                    String(baseTopic + F("both_channels")).c_str()
                );
                discovery->addParameter(F("icon_template"), icon);
            }
            break;
        case AutoDiscoveryType::BINARY_STATUS:
            if (discovery->create(MQTTComponent::ComponentType::SENSOR, baseTopic + FSPGM(binary), format)) {
                discovery->addStateTopic(_getTopic(ChannelType::NONE, TopicType::METRICS));
                discovery->addValueTemplate(FSPGM(binary));
                discovery->addName(F("Binary Status"));
                discovery->addObjectId(baseTopic + F("binary_status"));
            }
            break;
        case AutoDiscoveryType::BUSY_STATUS:
            if (discovery->create(MQTTComponent::ComponentType::SENSOR, baseTopic + FSPGM(busy, "busy"), format)) {
                discovery->addStateTopic(_getTopic(ChannelType::NONE, TopicType::METRICS));
                discovery->addValueTemplate(FSPGM(busy));
                discovery->addName(F("Status"));
                discovery->addObjectId(baseTopic + F("status"));
            }
            break;
        case AutoDiscoveryType::MAX:
            break;
    }
    return discovery;
}

inline uint8_t BlindsControl::getAutoDiscoveryCount() const
{
    return static_cast<uint8_t>(AutoDiscoveryType::MAX);
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
    if (strcmp_end_P(topic, PSTR("channel_0/set")) == 0) {
        channel = ChannelType::CHANNEL0;
    }
    else if (strcmp_end_P(topic, PSTR("channel_1/set")) == 0) {
        channel = ChannelType::CHANNEL1;
    }
    else {
        channel = ChannelType::ALL;
    }
    auto state = MQTT::Client::toBool(payload, false);
    __DBG_printf("topic=%s payload=%s (%d) channel=%d state=%u", topic, payload, len, channel, state);
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

inline void BlindsControl::_saveState()
{
    #if IOT_BLINDS_CTRL_SAVE_STATE
        auto file = KFCFS.open(FSPGM(iot_blinds_control_state_file), fs::FileOpenMode::read);
        if (file) {
            decltype(_states) states;
            if (states.read(file) && (memcmp(&states, &_states, sizeof(states)) == 0)) {
                __LDBG_printf("file=%u state=%u,%u skipping save, no changes", static_cast<bool>(file), static_cast<unsigned>(_states[0]), static_cast<unsigned>(_states[1]));
                return;
            }
        }
        file = KFCFS.open(FSPGM(iot_blinds_control_state_file), fs::FileOpenMode::write);
        if (file) {
            _states.write(file);
        }
        __LDBG_printf("file=%u state=%u,%u", static_cast<bool>(file), static_cast<unsigned>(_states[0]), static_cast<unsigned>(_states[1]));
    #endif
}

inline void BlindsControl::_loadState()
{
    #if IOT_BLINDS_CTRL_SAVE_STATE
        auto file = KFCFS.open(FSPGM(iot_blinds_control_state_file), fs::FileOpenMode::read);
        if (file) {
            if (!_states.read(file)) {
                _states = {};
            }
        }
        __LDBG_printf("file=%u state=%u,%u", static_cast<bool>(file), static_cast<unsigned>(_states[0]), static_cast<unsigned>(_states[1]));
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
            _states.write(file);
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
