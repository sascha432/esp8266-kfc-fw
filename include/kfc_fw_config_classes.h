/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>

#include "push_pack.h"

namespace KFCConfigurationClasses {

    struct System {

        class Flags {
        public:
            Flags();
            Flags(ConfigFlags flags) : _flags(flags) {
            }
            ConfigFlags *operator->() {
                return &_flags;
            }
            static Flags read();
            void write();

        private:
            ConfigFlags _flags;
        };

        class Device {
        public:
            Device() {
            }
            static const char *getName();
            static const char *getPassword();
            static void setName(const String &name);
            static void setPassword(const String &name);

        private:
            char _name[17];
            char _pass[33];
        };

        Flags flags;
        Device device;

    };

    struct Network {

        class Settings {
        public:
            Settings();
            static Settings read();
            static void defaults();
            void write();

            IPAddress localIp() const {
                return _localIp;
            }
            IPAddress subnet() const {
                return _subnet;
            }
            IPAddress gateway() const {
                return _gateway;
            }
            IPAddress dns1() const {
                return _dns1;
            }
            IPAddress dns2() const {
                return _dns2;
            }

            struct __attribute__packed__ {
                uint32_t _localIp;
                uint32_t _subnet;
                uint32_t _gateway;
                uint32_t _dns1;
                uint32_t _dns2;
            };
        };

        class SoftAP {
        public:
            using EncryptionType = wl_enc_type;

            SoftAP();
            static SoftAP read();
            static void defaults();
            void write();

            IPAddress address() const {
                return _address;
            }
            IPAddress subnet() const {
                return _subnet;
            }
            IPAddress gateway() const {
                return _gateway;
            }
            IPAddress dhcpStart() const {
                return _dhcpStart;
            }
            IPAddress dhcpEnd() const {
                return _dhcpEnd;
            }
            uint8_t channel() const {
                return _channel;
            }
            EncryptionType encryption() const {
                return static_cast<EncryptionType>(_encryption);
            }

            struct __attribute__packed__ {
                uint32_t _address;
                uint32_t _subnet;
                uint32_t _gateway;
                uint32_t _dhcpStart;
                uint32_t _dhcpEnd;
                uint8_t _channel;
                uint8_t _encryption;
            };
        };

        class WiFiConfig {
        public:
            static const char *getSSID();
            static const char *getPassword();
            static void setSSID(const String &ssid);
            static void setPassword(const String &password);

            static const char *getSoftApSSID();
            static const char *getSoftApPassword();
            static void setSoftApSSID(const String &ssid);
            static void setSoftApPassword(const String &password);

            char _ssid[33];
            char _password[33];
            char _softApSsid[33];
            char _softApPassword[33];
        };

        Settings settings;
        SoftAP softAp;
        WiFiConfig WiFiConfig;
    };

    struct Plugins {

        class HomeAssistant {
        public:
            typedef enum : uint8_t {
                NONE = 0,
                TURN_ON,
                TURN_OFF,
                SET_BRIGHTNESS,
                CHANGE_BRIGHTNESS,  // values: step(0), min brightness(1), max brightness(2), turn off if less than min. brightness(3)
            } ActionEnum_t;
            typedef struct __attribute__packed__ {
                uint16_t id: 16;
                ActionEnum_t action;
                uint8_t valuesLen;
                uint8_t entityLen;
            } ActionHeader_t;

            class Action {
            public:
                typedef std::vector<int32_t> ValuesVector;

                Action() = default;
                Action(uint16_t id, ActionEnum_t action, const ValuesVector &values, const String &entityId) : _id(id), _action(action), _values(values), _entityId(entityId) {
                }
                ActionEnum_t getAction() const {
                    return _action;
                }
                const __FlashStringHelper *getActionFStr() const {
                    return HomeAssistant::getActionStr(_action);
                }
                void setAction(ActionEnum_t action) {
                    _action = action;
                }
                uint16_t getId() const {
                    return _id;
                }
                void setId(uint16_t id) {
                    _id = id;
                }
                int32_t getValue(uint8_t num) const {
                    if (num < _values.size()) {
                        return _values.at(num);
                    }
                    return 0;
                }
                void setValues(const ValuesVector &values) {
                    _values = values;
                }
                ValuesVector &getValues() {
                    return _values;
                }
                const String getValuesStr() const {
                    return implode(',', _values);
                }
                uint8_t getNumValues() const {
                    return _values.size();
                }
                const String &getEntityId() const {
                    return _entityId;
                }
                void setEntityId(const String &entityId) {
                    _entityId = entityId;
                }
            private:
                uint16_t _id;
                ActionEnum_t _action;
                ValuesVector _values;
                String _entityId;
            };

            typedef std::vector<Action> ActionVector;

            HomeAssistant() {
            }

            static void setApiEndpoint(const String &endpoint);
            static void setApiToken(const String &token);
            static const char *getApiEndpoint();
            static const char *getApiToken();
            static void getActions(ActionVector &actions);
            static void setActions(ActionVector &actions);
            static Action getAction(uint16_t id);
            static const __FlashStringHelper *getActionStr(ActionEnum_t action);

            char api_endpoint[128];
            char token[250];
            uint8_t *actions;
        };

        HomeAssistant homeassistant;

    };


    struct MainConfig {

        System system;
        Network network;
        Plugins plugins;

    };

};

#include "pop_pack.h"
