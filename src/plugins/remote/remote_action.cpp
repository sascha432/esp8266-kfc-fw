/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include <kfc_fw_config.h>
#include <WiFiUdp.h>
#include "remote_action.h"
#include "remote.h"

#if DEBUG_IOT_REMOTE_CONTROL
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using namespace RemoteControl;

void ActionUDP::execute(Callback callback)
{
    WiFiUDP udp;
    auto address = getResolved();
    if (
        udp.beginPacket(address, _port) &&
        (udp.write(_payload.data(), _payload.size()) == _payload.size()) &&
        udp.endPacket()
    ) {
        // __LDBG_printf("execute succeeded udp=%s:%u payload=%s", address.toString().c_str(), _port, printable_string(_payload.data(), _payload.size(), 128, nullptr, true).c_str());
        callback(true);
        return;
    }
    __LDBG_printf("execute failed udp=%s:%u payload=%s", address.toString().c_str(), _port, printable_string(_payload.data(), _payload.size(), 128, nullptr, true).c_str());
    callback(false);
}

void ActionMQTT::execute(Callback callback)
{
    auto client = MQTTClient::getClient();
    if (client && client->isConnected() && client->publish(MQTT::AutoDiscovery::Entity::getTriggersTopic(), false, _payload, _qos)) {
        // __LDBG_printf("execute succeeded client=%p connected=%u payload=%s", client, client && client->isConnected(), _payload.c_str());
        callback(true);
        return;
    }
    __LDBG_printf("execute failed client=%p connected=%u payload=%s", client, client && client->isConnected(), _payload.c_str());
    callback(false);
}
