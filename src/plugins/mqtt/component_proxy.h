/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "mqtt_client.h"

namespace MQTT {

    class ComponentProxy : public Component {
    public:
        ComponentProxy(Client *client, ComponentPtr component, const String &wildcard, ResultCallback callback) :
            Component(ComponentType::DEVICE_AUTOMATION),
            _client(client),
            _component(component),
            _wildcard(wildcard),
            _callback(callback)
        {
        }
        ~ComponentProxy() {
            if (_callback) {
                _callback(_component, Client::getClient(), false);
            }
        }

        Component *get() const {
            return _component;
        }

        virtual AutoDiscoveryPtr nextAutoDiscovery(FormatType format, uint8_t num) { return nullptr; }
        virtual uint8_t getAutoDiscoveryCount() const { return 0; }

        // this method will destroy the object, do not access this afterwards
        void invokeEnd(MQTTClient *client, bool result) {
            _timeout.remove();
            if (_callback) {
                _callback(_component, client, true);
                _callback = nullptr;
            }
            client->remove(this);
            client->removeTopicsEndRequest(_component);
        }

        virtual void onConnect(MQTTClient *client) {
            begin();
        }

        virtual void onDisconnect(MQTTClient *client, AsyncMqttClientDisconnectReason reason) {
            end();
        }

        virtual void onMessage(MQTTClient *client, char *topic, char *payload, size_t len) {
            __LDBG_printf("component=%p topic=%s payload=%s len=%u", _component, topic, payload, len);
            if (len != 0) {
                if (_testPayload.equals(payload)) {
                    _testPayload = String();
                    invokeEnd(client, true);
                    return;
                }
                __DBG_printf("remove topic=%s", topic);
                _client->publish(topic, false, String(), QosType::AT_MODE_ONCE);
            }
        }

        void begin(uint16_t timeout_seconds = 30) {
            __LDBG_printf("begin topic=%s", _wildcard.c_str());
            _Timer(_timeout).add(Event::seconds(timeout_seconds), false, [this](Event::CallbackTimerPtr timer) {
                __LDBG_printf("timeout=%s", _wildcard.c_str());
                invokeEnd(_client, false);
            });
            _client->subscribe(this, _wildcard, QosType::EXACTLY_ONCE);
            auto tmp = String(_wildcard);
            tmp.replace(F("+"), PrintString(F("test_%08x%06x"), micros(), this));

            srand(micros());
            char buf[32];
            char *ptr = buf;
            uint8_t count = sizeof(buf);
            while(count--) {
                *ptr++ = rand();
                delayMicroseconds(rand() % 10 + 1);
            }
            _testPayload = String();
            bin2hex_append(_testPayload, buf, sizeof(buf));

            _client->publish(tmp, false, _testPayload, QosType::AT_MODE_ONCE);

        }
        void end() {
            __LDBG_printf("end topic=%s", _wildcard.c_str());
            _client->unsubscribe(this, _wildcard);
            _timeout.remove();
        }


        Client *_client;
        Component *_component;
        String _wildcard;
        String _testPayload;
        ResultCallback _callback;
        Event::Timer _timeout;
    };

}
