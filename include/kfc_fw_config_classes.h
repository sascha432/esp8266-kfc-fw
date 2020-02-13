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

    struct MainConfig {

        System system;
        Network network;

    };

};

#include "pop_pack.h"
