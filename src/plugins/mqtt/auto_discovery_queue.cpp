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
    _runFlags(RunFlags::DEFAULTS)
{
    MQTTClient::registerComponent(this);
}

Queue::~Queue()
{
    __LDBG_printf("dtor collect=%p remove=%p timer=%u", _collect.get(), _remove.get(), (bool)_timer);
    clear();

    // if the component is still registered, publish done has not been called
    if (MQTTClient::unregisterComponent(this)) {
        _publishDone(false);
    }

#if DEBUG_MQTT_AUTO_DISCOVERY_QUEUE
    __LDBG_printf("--- callbacks");
    for(const auto &topic: _client.getTopics()) {
        __LDBG_printf("topic=%s component=%p name=%s", topic.getTopic().c_str(), topic.getComponent(), topic.getComponent()->getName());
    }
    __LDBG_printf("--- end callbacks");
#endif
}

// EntityPtr Queue::getAutoDiscovery(FormatType format, uint8_t num)
// {
//     return nullptr;
// }

// uint8_t Queue::getAutoDiscoveryCount() const
// {
//     return 0;
// }

void Queue::onDisconnect(AsyncMqttClientDisconnectReason reason)
{
    __LDBG_printf("auto discovery aborted, marked for restart");
    client()._startAutoDiscovery = true;
}

void Queue::onPacketAck(uint16_t packetId, PacketAckType type)
{
    __LDBG_printf("packet=%u<->%u type=%u", _packetId, packetId, type);
    if (_packetId == packetId) {
        __LDBG_printf("packet=%u type=%u", packetId, type);
        _packetId = 0;
        if (type == PacketAckType::TIMEOUT) {
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


bool Queue::isEnabled(bool force)
{
#if MQTT_AUTO_DISCOVERY
    auto cfg = Plugins::MQTTClient::getConfig();
    return
        System::Flags::getConfig().is_mqtt_enabled && cfg.auto_discovery && (force || (
#if ENABLE_DEEP_SLEEP
            !resetDetector.hasWakeUpDetected() &&
#endif
            (cfg.auto_discovery_delay != 0)
        ));
#else
    return false;
#endif
}

void Queue::clear()
{
   __LDBG_printf("clear entities=%u done=%u", _entities.size(), _iterator == _entities.end());
    _timer.remove();
    _crcs.clear();

    noInterrupts();
    _iterator = _entities.end();
    interrupts();

    // stop proxies if still running
    _remove->stop(_remove);
    _collect->stop(_collect);
}

bool Queue::isUpdateScheduled()
{
    auto client = MQTTClient::getClient();
    if (!client) {
        return false;
    }
    return client->_autoDiscoveryRebroadcast;
}

void Queue::runPublish(uint32_t delayMillis)
{
    __LDBG_printf("components=%u delay=%u", _client._components.size(), delayMillis);
    if (!_client._components.empty() && delayMillis) {
        uint32_t initialDelay;

        if (_runFlags & RunFlags::FORCE_NOW) {
            delayMillis = 1;
        }

        // add +-10%
        srand(micros());
        initialDelay = stdex::randint((delayMillis * 90) / 100, (delayMillis * 110) / 100);

        clear();
        _diff = {};
        _entities = List(_client._components, FormatType::JSON);

        __LDBG_printf("starting broadcast in %u ms", std::max<uint32_t>(1000, initialDelay));

        _Timer(_timer).add(Event::milliseconds(std::max<uint32_t>(1000, initialDelay)), false, [this](Event::CallbackTimerPtr timer) {

            // check if we have real time
            if (!(_runFlags & RunFlags::FORCE_NOW)) {
                auto now = time(nullptr);
                if (!IS_TIME_VALID(now)) {
                    __DBG_printf("auto discovery, time() is invalid, retrying in 30 seconds");
                    timer->rearm(Event::seconds(30), false);
                    return;
                }
                if (!_client.isAutoDiscoveryLastTimeValid()) {
                    __DBG_printf("auto discovery, last upate timestamp invalid, retrying in 30 seconds");
                    timer->rearm(Event::seconds(30), false);
                    return;
                }
                // check when the next auto discovery is supposed to run
                __DBG_printf("last sucess d% last failure %d run=%d", _client._autoDiscoveryLastSuccess, _client._autoDiscoveryLastFailure, _client._autoDiscoveryLastSuccess > _client._autoDiscoveryLastFailure);
                if (_client._autoDiscoveryLastSuccess > _client._autoDiscoveryLastFailure) {
                    auto cfg = Plugins::MQTTClient::getConfig();
                    uint32_t next = _client._autoDiscoveryLastSuccess + (cfg.getAutoDiscoveryRebroadcastInterval() * 60);
                    int32_t diff = next - time(nullptr);
                    __DBG_printf_E("last published=%u wait_time=%d minutes", (diff / 60) + 1);
                    // not within 2 minutes... report error and delay next run by the time that has been left
                    if (diff > 120) {
                        _publishDone(false, (next / 60) + 1);
                        return;
                    }
                }
            }

            // get all topics that belong to this device
            __LDBG_printf("starting CollectTopicsComponent");
            _collect.reset(new CollectTopicsComponent(&_client, std::move(_client._createAutoDiscoveryTopics())));

            _collect->begin([this](ComponentProxy::ErrorType error, AutoDiscovery::CrcVector &crcs) {
                __LDBG_printf("collect callback error=%u", error);
                if (error == ComponentProxy::ErrorType::SUCCESS) {
                    // compare with current auto discovery
                    auto currentCrcs = _entities.crc();
                    if (_runFlags & RunFlags::FORCE_UPDATE) {
                        crcs.invalidate();
                    }
                    _diff = currentCrcs.difference(crcs);
                    if (_runFlags & RunFlags::FORCE_UPDATE) {
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
                        __LDBG_printf("add=%u modified=%u removed=%u unchanged=%u force_update=%u", _diff.add, _diff.modify, _diff.remove, _diff.unchanged, _forceUpdate);
                        // discovery has changed, remove all topics that are not updated/old and start broadcasting
                        // recycle _wildcards and currentCrcs
                        _remove.reset(new RemoveTopicsComponent(&_client, std::move(_collect->_wildcards), std::move(currentCrcs)));
                        // destroy object. the callback was moved onto the stack and _wildcards to RemoveTopicsComponent
                        __LDBG_printf("destroy CollectTopicsComponent");
                        _collect.reset();

                        _remove->begin([this](ComponentProxy::ErrorType error) {
                            __LDBG_printf("collect callback error=%u remove=%p", error, _remove.get());
                            _remove->_wildcards.clear(); // remove wildcards to avoid unsubscribing twice
                            _remove.reset();

                            if (error == ComponentProxy::ErrorType::SUCCESS) {
                                __LDBG_printf("starting to broadcast auto discovery");
                                _packetId = 0;
                                _iterator = _entities.begin();
                                _publishNextMessage();
                            }
                            else {
                                __LDBG_printf("error %u occurred during remove topics", error);
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
    else {
        __LDBG_printf("auto discovery not executed");
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
        const auto &entitiy = *_iterator;
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
        // __LDBG_printf("%s", printable_string(entitiy->getPayload().c_str(), entitiy->getPayload().length(), DEBUG_MQTT_CLIENT_PAYLOAD_LEN).c_str());
        __LDBG_printf("%s", entitiy->getPayload().c_str());

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
    __DBG_printf("success=%u error_delay=%u entities=%u size=%u crc=%08x add=%u modified=%u removed=%u unchanged=%u",
        success,
        onErrorDelay,
        _entities.size(),
        _entities.payloadSize(),
        _crcs.crc32b(),
        _diff.add, _diff.modify, _diff.remove, _diff.unchanged
    );
    MQTTClient::unregisterComponent(this);

    Event::milliseconds delay;
    if (success) {
        delay = Event::minutes(Plugins::MQTTClient::getConfig().getAutoDiscoveryRebroadcastInterval());
    }
    else if (onErrorDelay) {
        delay = Event::minutes(onErrorDelay);
    }
    else {
        delay = Event::milliseconds::zero();
    }

    _client.updateAutoDiscoveryTimestamps(success);

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

    // cleanup before deleteing
    clear();
    _client._autoDiscoveryQueue.reset(); // deletes itself and the timer
    return;
}
