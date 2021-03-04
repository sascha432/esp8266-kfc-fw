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
    _packetId(0),
    _forceUpdate(false)
{
    _client.registerComponent(this);
}

Queue::~Queue()
{
#if MQTT_AUTO_DISCOVERY_LOG2FILE
    if (_log) {
        _log.flush();
    }
#endif

    __DBG_printf("dtor collect=%p remove=%p timer=%u", _collect.get(), _remove.get(), (bool)_timer);
    clear();
    noInterrupts();
    _iterator = _entities.end();
    interrupts();

    // if the component is still registered, publish done has not been called
    if (_client.isComponentRegistered(this)) {
        _publishDone(false);
    }

#if MQTT_AUTO_DISCOVERY_LOG2FILE
    if (_log) {
        _log.close();
    }
#endif

    __DBG_printf("--- callbacks");
    for(const auto &topic: _client.getTopics()) {
        __DBG_printf("topic=%s component=%p name=%s", topic.getTopic().c_str(), topic.getComponent(), topic.getComponent()->getName());
    }
    __DBG_printf("--- end callbacks");

}

EntityPtr Queue::getAutoDiscovery(FormatType format, uint8_t num)
{
    return nullptr;
}

uint8_t Queue::getAutoDiscoveryCount() const
{
    return 0;
}

void Queue::onDisconnect(AsyncMqttClientDisconnectReason reason)
{
    client()._startAutoDiscovery = true;
}

void Queue::onPacketAck(uint16_t packetId, PacketAckType type)
{
    __LDBG_printf("packet=%u<->%u type=%u", _packetId, packetId, type);
    if (_packetId == packetId) {
        __LDBG_printf("packet=%u type=%u", packetId, type);
        _packetId = 0;
        if (type == PacketAckType::TIMEOUT) {
#if MQTT_AUTO_DISCOVERY_LOG2FILE
            if (_log) {
                _log.printf_P(PSTR("timeout [%u]\n"), packetId);
            }
#endif
            LoopFunctions::callOnce([this]() {
                _publishDone(false);
            });
            return;
        }
        else {
            _crcs.update(_iterator->getTopic(), _iterator->getPayload());
            ++_iterator;
            LoopFunctions::callOnce([this]() {
                _publishNextMessage();
            });
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
    _timer.remove();
    _crcs.clear();
    _collect.reset();
    _remove.reset();
}

bool Queue::isUpdateScheduled()
{
    auto client = MQTTClient::getClient();
    if (!client) {
        return false;
    }
    return static_cast<bool>(client->_autoDiscoveryRebroadcast);
}

void Queue::publish(Event::milliseconds delay)
{
    __LDBG_printf("components=%u delay=%u", _client._components.size(), delay);
    if (!_client._components.empty()) {
        uint32_t initialDelay;

        // remove state
        if (delay.count() == 0) {
            initialDelay = 0; // use min. value
        }
        else {
            // add +-10%
            srand(micros());
            initialDelay = stdex::randint(delay.count() * 0.9, delay.count() * 1.1);
        }

        clear();
        _diff = {};
        _entities = List(_client._components, FormatType::JSON);
#if MQTT_AUTO_DISCOVERY_LOG2FILE
        String filename = PrintString(F("/.pvt/mqtt-ad.%u.log"), time(nullptr));
        _log = KFCFS.open(filename, fs::FileOpenMode::write);
#endif

        _Timer(_timer).add(Event::milliseconds(std::max<uint32_t>(1000, initialDelay)), false, [this](Event::CallbackTimerPtr timer) {

            // get all topics that belong to this device
            _collect.reset(new CollectTopicsComponent(&_client, IF_MQTT_AUTO_DISCOVERY_LOG2FILE(_log, )std::move(_client._createAutoDiscoveryTopics())));
            _collect->begin([this](ComponentProxy::ErrorType error, AutoDiscovery::CrcVector &crcs) {
                __DBG_printf("collect callback error=%u", error);
#if MQTT_AUTO_DISCOVERY_LOG2FILE
                if (_log) {
                    _log.printf_P(PSTR("---\n"));
                    _log.flush();
                }
#endif
                if (error == ComponentProxy::ErrorType::SUCCESS) {
                    // compare with current auto discovery
                    auto currentCrcs = _entities.crc();
                    __LDBG_printf("crc32 %08x==%08x", crcs.crc32b(), currentCrcs.crc32b());
                    if (_forceUpdate) {
                        std::fill(crcs.begin(), crcs.end(), ~0U);
                    }
                    _diff = currentCrcs.difference(crcs);
                    if (_forceUpdate) {
                        currentCrcs = {};
                        crcs.clear();
                    }
                    //if (currentCrcs == crcs) {
                    if (_diff.equal) {
                        __LDBG_printf("auto discovery matches current version");
                        _publishDone(true);
                        return;
                    }
                    else {
                        __LDBG_printf("add=%u modified=%u removed=%u unchanged=%u", _diff.add, _diff.modify, _diff.remove, _diff.unchanged);
                        // discovery has changed, remove all topics that are not updated/old and start broadcasting
                        // recycle _wildcards and currentCrcs
                        _remove.reset(new RemoveTopicsComponent(&_client, std::move(_collect->_wildcards), IF_MQTT_AUTO_DISCOVERY_LOG2FILE(_log, )std::move(currentCrcs)));
                        // destroy object. the callback was moved onto the stack and _wildcards to RemoveTopicsComponent
                        _collect.reset();

                        _remove->begin([this](ComponentProxy::ErrorType error) {
                            __DBG_printf("collect callback error=%u remove=%p", error, _remove.get());
#if MQTT_AUTO_DISCOVERY_LOG2FILE
                            if (_log) {
                                _log.printf_P(PSTR("---\n"));
                                _log.flush();
                            }
#endif

                            _remove->_wildcards.clear(); // remove wildcards to avoid unsubscribing twice
                            _remove.reset();

                            if (error == ComponentProxy::ErrorType::SUCCESS) {
                                __DBG_printf("starting to broadcast auto discovery");
                                _packetId = 0;
                                _iterator = _entities.begin();
                                _publishNextMessage();
                            }
                            else {
                                __DBG_printf("error %u occurred during remove topics", error);
                                _publishDone(false);
                                return;
                            }
                        });
                    }
                }
                else {
                    __LDBG_printf("error %u occurred during collect topics", error);
                    _publishDone(false);
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
#if MQTT_AUTO_DISCOVERY_LOG2FILE
            if (_log) {
                _log.printf_P(PSTR("publish [error: size %u limit %u exceeded] topic="), size, MQTT_MAX_MESSAGE_SIZE);
                _log.print(entitiy->getTopic());
                _log.print(F(" payload="));
                _log.print(entitiy->getPayload());
                _log.print('\n');
            }
#endif
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
        // __LDBG_printf("%s", printable_string(entitiy->getPayload().c_str(), entitiy->getPayload().length(), DEBUG_MQTT_CLIENT_PAYLOAD_LEN).c_str());
        __LDBG_printf("%s", entitiy->getPayload().c_str());

        // we can assign a packet id before in case the acknowledgment arrives before setting _packetId after the call
        // pretty unlikely with WiFI a a few ms lag, but with enough debug code in between it is possible ;)
        _packetId = _client.getNextInternalPacketId();
        __LDBG_printf("auto discovery packet id=%u", _packetId);
#if MQTT_AUTO_DISCOVERY_LOG2FILE
        if (_log) {
            _log.printf_P(PSTR("publish [%u] topic="), _packetId);
            _log.print(entitiy->getTopic());
            _log.print(F(" payload="));
            _log.print(entitiy->getPayload());
            _log.print('\n');
        }
#endif
        auto queue = _client.publishWithId(nullptr, entitiy->getTopic(), true, entitiy->getPayload(), QosType::AUTO_DISCOVERY, _packetId);
        if (!queue) {
            __LDBG_printf("failed to publish topic=%s", topic);
#if MQTT_AUTO_DISCOVERY_LOG2FILE
            if (_log) {
                _log.printf_P(PSTR("error [%u]\n"), _packetId);
            }
#endif
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
    __DBG_printf("success=%u error_delay=%u entities=%u size=%u crc=%08x add=%u modified=%u removed=%u unchanged=%u",
        success,
        onErrorDelay,
        _entities.size(),
        _entities.payloadSize(),
        _crcs.crc32b(),
        _diff.add, _diff.modify, _diff.remove, _diff.unchanged
    );
    _client.unregisterComponent(this);

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

    auto message = PrintString(success ? F("MQTT auto discovery published [entities=%u") : F("MQTT auto discovery aborted [entities=%u"), _entities.size());
    if (_entities.payloadSize()) {
        message.print(F(", size="));
        message.print(formatBytes(_entities.payloadSize()));
    }
    if (_diff.add) {
        message.printf_P(PSTR(", added %u"), _diff.add);
    }
    if (_diff.remove) {
        message.printf_P(PSTR(", removed %u"), _diff.remove);
    }
    if (_diff.modify) {
        message.printf_P(PSTR(", modified %u"), _diff.modify);
    }
    if (_diff.unchanged) {
        message.printf_P(PSTR(", unchanged %u"), _diff.unchanged);
    }
    if (delay.count()) {
        message.printf_P(PSTR(", rebroadcast in %s"), formatTime(delay.count() / 1000U).c_str());
        _Timer(_client._autoDiscoveryRebroadcast).add(delay, false, Client::publishAutoDiscoveryCallback);
    }
    message.print(']');

    Logger_notice(message);

    _client._autoDiscoveryQueue.reset(); // deletes itself and the timer

    // auto client = &_client;
    // LoopFunctions::callOnce([client]() {
    //     client->_autoDiscoveryQueue.reset(); // deletes itself and the timer
    // });
    //+mqtt=auto
}
