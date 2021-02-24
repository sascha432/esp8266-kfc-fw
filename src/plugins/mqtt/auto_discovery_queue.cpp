/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include <MicrosTimer.h>
#include <stl_ext/utility.h>
#include <stl_ext/memory.h>
#include "mqtt_client.h"
#include "component_proxy.h"
#include "auto_discovery_list.h"

#if DEBUG_MQTT_AUTO_DISCOVERY_QUEUE
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using KFCConfigurationClasses::Plugins;
using KFCConfigurationClasses::System;

using namespace MQTT::AutoDiscovery;

Queue::Queue(Client &client) :
    Component(ComponentType::AUTO_DISCOVERY),
    _client(client),
    _packetId(0)
{
    _client.registerComponent(this);
}

Queue::~Queue()
{
    clear();
    _client.unregisterComponent(this);
}

EntityPtr Queue::nextAutoDiscovery(FormatType format, uint8_t num)
{
    return nullptr;
}

uint8_t Queue::getAutoDiscoveryCount() const
{
    return 0;
}

void Queue::onDisconnect(Client *client, AsyncMqttClientDisconnectReason reason)
{
    _client._startAutoDiscovery = true;
}

void Queue::onPacketAck(uint16_t packetId, PacketAckType type)
{
    __LDBG_printf("packet=%u<->%u type=%u", _packetId, packetId, type);
    if (_packetId == packetId) {
        __LDBG_printf("packet=%u type=%u", packetId, type);
        _packetId = 0;
        if (type == PacketAckType::TIMEOUT) {
            _publishDone(false);
            return;
        }
        else {
            _discoveryCount++;
            _size += _iterator->getMessageSize();
            _crcs.update(_iterator->getTopic(), _iterator->getPayload());
            ++_iterator;
            _publishNextMessage();
        }
    }
}


bool Queue::isEnabled()
{
#if MQTT_AUTO_DISCOVERY
    return
#if ENABLE_DEEP_SLEEP
        !resetDetector.hasWakeUpDetected() &&
#endif
        System::Flags::getConfig().is_mqtt_enabled;
#else
    return false;
#endif
}

void Queue::clear()
{
   __LDBG_printf("clear size=%u done=%u left=%u", _entities.size(), _iterator == _entities.end(), std::distance(_entities.begin(), _iterator));
   if (_iterator != _entities.end()) {
       // clear last state on error
       _setState(StateFileType());
   }
    _timer.remove();
    _crcs.clear();
}

bool Queue::isUpdateScheduled()
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

void Queue::publish(Event::milliseconds delay)
{
    __LDBG_printf("components=%u delay=%u", _client._components.size(), delay);
    if (!_client._components.empty()) {
        uint32_t initialDelay;

        // remove state
        if (delay.count() == 0) {
            _setState(StateFileType());
            initialDelay = 0; // use min. value
        }
        else {
            // add +-10%
            srand(micros());
            initialDelay = stdex::randint(delay.count() * 0.9, delay.count() * 1.1);
        }

        _size = 0;
        _discoveryCount = 0;
        _crcs.clear();
        _entities = List(_client._components, FormatType::JSON);

        _Timer(_timer).add(Event::milliseconds(std::max<uint32_t>(1000, initialDelay)), false, [this](Event::CallbackTimerPtr timer) {

            // get all topics that belong to this device
            auto collect = std::make_shared<CollectTopicsComponent>(&_client, std::move(_client._createAutoDiscoveryTopics()));
            collect->begin([this, collect](ComponentProxy::ErrorType error, AutoDiscovery::CrcVector &crcs) mutable {
                __LDBG_printf("collect callback error=%u", error);
                if (error == ComponentProxy::ErrorType::SUCCESS) {
                    // compare with current auto discovery
                    auto currentCrcs = _entities.crc();
                    __LDBG_printf("crc32 %08x==%08x", crcs.crc32b(), currentCrcs.crc32b());
                    if (currentCrcs == crcs) {
                        __LDBG_printf("auto discovery matches current version");
                        _publishDone(true);
                        return;
                    }
                    else {
                        __LDBG_printf("collect=%p", collect.get());
                        // discovery has changed, remove all topics that are not updated/old and start broadcasting
                        // recycle _wildcards and currentCrcs
                        auto remove = std::make_shared<RemoveTopicsComponent>(&_client, std::move(collect->_wildcards), std::move(currentCrcs));
                        // destroy object. the callback was moved onto the stack and _wildcards to RemoveTopicsComponent
                        collect.reset();

                        remove->begin([this, remove](ComponentProxy::ErrorType error) mutable {
                            __LDBG_printf("collect callback error=%u remove=%p", error, remove.get());
                            remove->_wildcards.clear(); // remove wildcards to avoid unsubscribing twice
                            remove.reset();

                            if (error == ComponentProxy::ErrorType::SUCCESS) {
                                __LDBG_printf("starting to broadcast auto discovery");
                                _packetId = 0;
                                _iterator = _entities.begin();
                                _publishNextMessage();
                            }
                            else {
                                __LDBG_printf("error %u occurred during remove topics", error);
                                _publishDone(true);
                                return;
                            }
                        });
                    }
                }
                else {
                    __LDBG_printf("error %u occurred during collect topics", error);
                    _publishDone(true);
                    return;
                }
            });
        });
    }
}

void Queue::_publishNextMessage()
{
    do {
        if (_iterator == _entities.end()) {
            __LDBG_printf("auto discovery published");
            _publishDone(true);
            return;
        }
        if (!_client.isConnected()) {
            __LDBG_printf("lost connection: auto discovery aborted");
            // _client._startAutoDiscovery = true;
            _publishDone(false);
            return;
        }
        auto entitiy = *_iterator;
        if (!entitiy) {
            __LDBG_printf_E("entity nullptr");
            ++_iterator;
            continue;
        }
        auto topic = entitiy->getTopic().c_str();
        auto size = entitiy->getMessageSize();
        if (Client::_isMessageSizeExceeded(size, topic)) {
            // skip message that does not fit into the tcp buffer
            ++_iterator;
            __LDBG_printf_E("max size exceeded topic=%s payload=%u size=%u", topic, entitiy->getPayload().length(), size);
            __LDBG_printf("%s", printable_string(entitiy->getPayload().c_str(), entitiy->getPayload().length(), DEBUG_MQTT_CLIENT_PAYLOAD_LEN).c_str());
            continue;
        }
        if (size >= _client.getClientSpace()) {
            __LDBG_printf_W("topic=%s size=%u/%u delay=%u", topic, size, _client.getClientSpace(), kAutoDiscoveryErrorDelay);
            _Timer(_timer).add(Event::milliseconds(kAutoDiscoveryErrorDelay), false, [this](Event::CallbackTimerPtr) {
                _publishNextMessage();
            });
            break;
        }
        __LDBG_printf("topic=%s size=%u/%u", topic, size, _client.getClientSpace());
        __LDBG_printf("%s", printable_string(entitiy->getPayload().c_str(), entitiy->getPayload().length(), DEBUG_MQTT_CLIENT_PAYLOAD_LEN).c_str());

        // we can assign a packet id before in case the acknowledgment arrives before setting _packetId after the call
        // pretty unlikely with WiFI a a few ms lag, but with enough debug code in between it is possible ;)
        _packetId = _client.getNextInternalPacketId();
        __LDBG_printf("auto discovery packet id=%u", _packetId);
        auto queue = _client.publishWithId(nullptr, entitiy->getTopic(), true, entitiy->getPayload(), QosType::AUTO_DISCOVERY, _packetId);
        if (!queue) {
            __LDBG_printf("failed to publish topic=%s", topic);
            _publishDone(false);
            return;
        }
        // _packetId = queue.getInternalId();
        // __LDBG_printf("auto discovery packet id=%u", _packetId);
    }
    while(false);
}

// make sure to exit any method after calling done
// the method deletes the object and this becomes invalid
void Queue::_publishDone(bool success, uint16_t onErrorDelay)
{
    __LDBG_printf("success=%u error_delay=%u entities=%u published=%u size=%u crc=%08x",
        success,
        onErrorDelay,
        _entities.size(),
        _discoveryCount,
        _size,
        _crcs.crc32b()
    );

    Event::milliseconds delay;
    if (success) {
        delay = Event::minutes(Plugins::MQTTClient::getConfig().auto_discovery_rebroadcast_interval);
    }
    else if (onErrorDelay) {
        delay = Event::minutes(onErrorDelay);
    }
    else {
        delay = Event::milliseconds::zero();
    }
    auto message = PrintString(success ? F("MQTT auto discovery published [components=%u, size=") : F("MQTT auto discovery aborted [components=%u, size="), _discoveryCount);
    message.print(formatBytes(_size));
    if (delay.count()) {
        message.printf_P(PSTR(", rebroadcast in %s"), formatTime(delay.count() / 1000U).c_str());
        _Timer(_client._autoDiscoveryRebroadcast).add(delay, false, Client::publishAutoDiscoveryCallback);
    }
    message.print(']');

    Logger_notice(message);
    if (success) {
        _setState(StateFileType(_crcs, time(nullptr)));
    }
    else {
        _setState(StateFileType());
    }

    _client._autoDiscoveryQueue.reset(); // deletes itself and the timer
}

void Queue::_setState(const StateFileType &state)
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

StateFileType Queue::_getState()
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
