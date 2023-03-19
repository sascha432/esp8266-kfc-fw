/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include "../src/plugins/mqtt/mqtt_client.h"

class MqttRemote : public MQTTComponent {
public:
    MqttRemote() :
        MQTTComponent(ComponentType::DEVICE_AUTOMATION),
        _sendBatteryStateAndGotoSleep(false)
    {
    }

    virtual AutoDiscovery::EntityPtr getAutoDiscovery(FormatType format, uint8_t num) override;
    virtual uint8_t getAutoDiscoveryCount() const override;

    virtual bool _sendBatteryStateAndSleep() = 0;

    virtual void onConnect() {
        if (_autoDiscoveryPending) {
            publishAutoDiscovery();
        }
        else if (_sendBatteryStateAndGotoSleep) {
            _sendBatteryStateAndGotoSleep = false;
            if (!_sendBatteryStateAndSleep()) {
                _Scheduler.add(Event::milliseconds(250), true, [this](Event::CallbackTimerPtr timer) {
                    if (_sendBatteryStateAndSleep()) {
                        timer->disarm();
                    }
                });
            }
        }
    }

    void publishAutoDiscovery() {
        if (isConnected()) {
            __LDBG_printf("auto discovery running=%u registered=%u", client().isAutoDiscoveryRunning(), MQTT::Client::isComponentRegistered(this));
            if (client().publishAutoDiscovery()) {
                _autoDiscoveryPending = false;
            }
        }
        else {
            _autoDiscoveryPending = true;
        }
    }

    void _setup() {
        _updateAutoDiscoveryCount();
        MQTT::Client::registerComponent(this);
    }

    void _reconfigure() {
        _updateAutoDiscoveryCount();
    }

    void _shutdown() {
        MQTT::Client::unregisterComponent(this);
    }

protected:
    bool _sendBatteryStateAndGotoSleep;

private:
    void _updateAutoDiscoveryCount();

private:
    bool _autoDiscoveryPending;
    uint8_t _autoDiscoveryCount;
};
