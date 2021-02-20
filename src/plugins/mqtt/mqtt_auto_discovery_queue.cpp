/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include <MicrosTimer.h>
#include "mqtt_client.h"

#if DEBUG_MQTT_AUTO_DISCOVERY_QUEUE
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using KFCConfigurationClasses::Plugins;

MQTTAutoDiscoveryQueue::MQTTAutoDiscoveryQueue(MQTTClient &client) : _client(client), _crc(~0)
{
}

MQTTAutoDiscoveryQueue::~MQTTAutoDiscoveryQueue()
{
    clear();
}

void MQTTAutoDiscoveryQueue::clear()
{
   __LDBG_printf("clear size=%u done=%u left=%u", _client._components.size(), _next == _client._components.end(), std::distance(_next, _client._components.end()));
   if (_next != _client._components.end()) {
       // clear last state on error
       _setState(StateFileType(0));
   }
    _timer.remove();
}

bool MQTTAutoDiscoveryQueue::isUpdateScheduled()
{
    uint32_t now  = time(nullptr);
    if (!IS_TIME_VALID(now)) {
        return false;
    }
    auto state = _getState();
    if (!state._valid) {
        return true;
    }
    auto delay = Plugins::MQTTClient::getConfig().auto_discovery_rebroadcast_interval;
    return (delay && now > state._lastAutoDiscoveryTimestamp + delay);
}

void MQTTAutoDiscoveryQueue::publish(bool force)
{
    __LDBG_printf("components=%u delay=%u force=%u", _client._components.size(), MQTT_AUTO_DISCOVERY_QUEUE_INITIAL_DELAY, force);
    if (!_client._components.empty()) {

        // remove state
        if (force) {
            _setState(StateFileType(0));
        }

#if DEBUG_MQTT_AUTO_DISCOVERY_QUEUE
        _maxQueue = 0;
        _maxQueueSkipCounter = 0;
        _start = millis();
#endif
        _crc = static_cast<uint16_t>(~0U);
        _lastFailed = false;
        _size = 0;
        _counter = 0;
        _discoveryCount = 0;
        _next = _client._components.begin();
        (*_next)->rewindAutoDiscovery();

        // no delay if forced
        auto intialDelay = force ?
            (0) :
            (MQTT_AUTO_DISCOVERY_QUEUE_INITIAL_DELAY + (rand() %
                    ((MQTT_AUTO_DISCOVERY_QUEUE_INITIAL_DELAY / 10) > 5 ? (MQTT_AUTO_DISCOVERY_QUEUE_INITIAL_DELAY / 10) : 5)
                ));
        _Timer(_timer).add(Event::milliseconds(std::max(1000, intialDelay)), true, [this](Event::CallbackTimerPtr timer) {
            _timerCallback(timer);
        });
    }
}

void MQTTAutoDiscoveryQueue::_timerCallback(Event::CallbackTimerPtr timer)
{
    __DBG_assert_panic(MQTTClient::getClient() == &_client, "_client address has changed");

    if (_counter++ == 0 && _next == _client._components.begin()) {
        uint32_t now = time(nullptr);
        uint32_t delay = 0;
        auto state = StateFileType();
        if (IS_TIME_VALID(now)) {
            state = _getState();
            if (state._valid) {
                delay = Plugins::MQTTClient::getConfig().auto_discovery_rebroadcast_interval;
                if (delay && now < state._lastAutoDiscoveryTimestamp + delay) {
                    __LDBG_printf("state=%u now=%u last_update=%u delay=%u", state._valid, now, state._lastAutoDiscoveryTimestamp, delay);
                    _publishDone(false, delay);
                    return;
                }
            }
        }

        __LDBG_printf("state=%u now=%u last_update=%u crc=%04x delay=%u update_interval=%u", state._valid, now, state._lastAutoDiscoveryTimestamp, state._crc, delay, MQTT_AUTO_DISCOVERY_QUEUE_DELAY);

        // rearm timer after the first call
        timer->updateInterval(Event::milliseconds(MQTT_AUTO_DISCOVERY_QUEUE_DELAY));
    }

    if (!_client.isConnected()) {
        __LDBG_print("not connected to MQTT server, defering auto discovery");
        return;
    }

#if DEBUG_MQTT_AUTO_DISCOVERY_QUEUE
    _maxQueue = std::max(_maxQueue, _client._queue.size());
#endif

    // check client's queue
    if (!_client._queue.empty()) {
        __LDBG_printf("client queue %u, skipping max=%u", _client._queue.size(), _maxQueueSkipCounter++);
        return;
    }

    __LDBG_printf("component #%u count=%u queue=%u counter=%u", std::distance(_client._components.begin(), _next), (*_next)->getAutoDiscoveryCount() + 1, _client._queue.size(), _counter);
    __DBG_assert_panic((_next != _client._components.end()), "_next must not be equal _client._components.end()");

    // skip components without auto discovery
    if ((*_next)->getAutoDiscoveryCount() == 0) {
        do {
            if (++_next == _client._components.end()) {
                _publishDone(true);
                return;
            }
            __LDBG_printf("component #%u count=%u", std::distance(_client._components.begin(), _next), (*_next)->getAutoDiscoveryCount());
        }
        while((*_next)->rewindAutoDiscovery() == false);
    }

    auto component = *_next;
    auto discovery = component->nextAutoDiscovery(MQTTAutoDiscovery::FormatType::JSON, component->getAutoDiscoveryNumber());
    component->nextAutoDiscovery();
    if (discovery) {
        // do we have enough space to send?
        auto msgSize = discovery->getMessageSize();
        if (msgSize < _client.getClientSpace()) {
            __LDBG_printf("topic=%s size=%u/%u", discovery->getTopic().c_str(), msgSize, _client.getClientSpace());
            __LDBG_printf("%s", printable_string(discovery->getPayload().c_str(), discovery->getPayload().length(), DEBUG_MQTT_CLIENT_PAYLOAD_LEN).c_str());

            _discoveryCount++;
            _size += msgSize;

            _client.publish(discovery->getTopic(), true, discovery->getPayload(), MQTTClient::QosType::AUTO_DISCOVERY);

            if (_lastFailed) {
                _lastFailed = false;
            }
            else {
                _crc = crc16_update(_crc, discovery->getPayload().c_str(), discovery->getPayload().length());
                __LDBG_printf("crc=%04x topic=%s", _crc, discovery->getTopic().c_str());
            }

        }
        else {
            _lastFailed = true;
            __LDBG_printf("tcp buffer full: %u > %u, queue counter=%u", msgSize, _client.getClientSpace(), _maxQueueSkipCounter++);
            if (!MQTTClient::_isMessageSizeExceeded(msgSize, discovery->getTopic().c_str())) {
                // retry if size not exceeded
                component->retryAutoDiscovery();
            }
            else {
                _crc = 0;
            }
        }
        __LDBG_delete(discovery);
        return;
    }

    if (++_next == _client._components.end()) {
        _publishDone(true);
        return;
    }
    else {
        // move to next component
        (*_next)->rewindAutoDiscovery();
    }
}

void MQTTAutoDiscoveryQueue::_publishDone(bool success, uint32_t delay)
{
    __LDBG_printf("done=%u delay_next=%u components=%u discovery=%u size=%u time=%.4fs max_queue=%u queue_skip=%u iterations=%u crc=%04x old_crc=%04x",
        success,
        delay,
        _client._components.size(),
        _discoveryCount,
        _size,
        get_time_diff(_start, millis()) / 1000.0,
        _maxQueue,
        _maxQueueSkipCounter,
        _counter,
        _crc,
        _getState()._crc
    );


    if ((delay) || ((delay = Plugins::MQTTClient::getConfig().auto_discovery_rebroadcast_interval) != 0) || (!success && (delay = 15) != 0)) {
        _Timer(_client._autoDiscoveryRebroadcast).add(Event::minutes(delay), false, MQTTClient::publishAutoDiscoveryCallback);
    }
    auto message = PrintString(success ? F("MQTT auto discovery published [components=%u, size=") : F("MQTT auto discovery aborted [components=%u, size="), _discoveryCount);
    message.print(formatBytes(_size));
    if (delay) {
        message.printf_P(PSTR(", rebroadcast in %s"), formatTime(delay * 60U).c_str());
    }
    message.print(']');

    Logger_notice(message);
    if (success) {
        _setState(StateFileType(_crc, time(nullptr)));
    }
    else {
        _setState(StateFileType());
    }

    _client._autoDiscoveryQueue.reset(); // deletes itself and the timer
}

void MQTTAutoDiscoveryQueue::_setState(const StateFileType &state)
{
    if (!state._valid) {
        KFCFS.remove(FSPGM(mqtt_state_file));
    }
    else {
        auto file = KFCFS.open(FSPGM(mqtt_state_file), fs::FileOpenMode::write);
        if (file) {
            file.write(reinterpret_cast<const uint8_t *>(&state), sizeof(state));
            file.close();
        }
    }
}

MQTTAutoDiscoveryQueue::StateFileType MQTTAutoDiscoveryQueue::_getState()
{
    auto state = StateFileType();
    auto file = KFCFS.open(FSPGM(mqtt_state_file), fs::FileOpenMode::read);
    if (file) {
        if (file.read(reinterpret_cast<uint8_t *>(&state), sizeof(state)) != sizeof(state)) {
            state = StateFileType();
            _setState(state);
        }
        file.close();
    }
    return state;
}
