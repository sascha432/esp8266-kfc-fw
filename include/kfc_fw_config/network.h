/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "kfc_fw_config/base.h"

namespace KFCConfigurationClasses {

    // --------------------------------------------------------------------
    // Network

    struct Network {

        // --------------------------------------------------------------------
        // Settings
        class SettingsConfig {
        public:
            typedef struct __attribute__packed__ SettingsConfig_t {
                using Type = SettingsConfig_t;
                CREATE_IPV4_ADDRESS(local_ip, 0);
                CREATE_IPV4_ADDRESS(subnet, kCreateIPv4Address(255, 255, 255, 0));
                CREATE_IPV4_ADDRESS(gateway, kCreateIPv4Address(192, 168, 4, 1));
                CREATE_IPV4_ADDRESS(dns1, kCreateIPv4Address(8, 8, 8, 8));
                CREATE_IPV4_ADDRESS(dns2, kCreateIPv4Address(8, 8, 4, 4));

                IPAddress getLocalIp() const {
                    return local_ip;
                }
                IPAddress getSubnet() const {
                    return subnet;
                }
                IPAddress getGateway() const {
                    return gateway;
                }
                IPAddress getDns1() const {
                    return dns1;
                }
                IPAddress getDns2() const {
                    return dns2;
                }
                SettingsConfig_t();
            } SettingsConfig_t;
        };

        class Settings : public SettingsConfig, public ConfigGetterSetter<SettingsConfig::SettingsConfig_t, _H(MainConfig().network.settings.cfg) CIF_DEBUG(, &handleNameSettingsConfig_t)> {
        public:

            static void defaults();

        };

        // --------------------------------------------------------------------
        // SoftAP

        class SoftAPConfig {
        public:
            using EncryptionType = WiFiEncryptionType;

            typedef struct __attribute__packed__ SoftAPConfig_t {
                using Type = SoftAPConfig_t;
                CREATE_IPV4_ADDRESS(address, kCreateIPv4Address(192, 168, 4, 1));
                CREATE_IPV4_ADDRESS(subnet, kCreateIPv4Address(255, 255, 255, 0));
                CREATE_IPV4_ADDRESS(gateway, kCreateIPv4Address(192, 168, 4, 1));
                CREATE_IPV4_ADDRESS(dhcp_start, kCreateIPv4Address(192, 168, 4, 2));
                CREATE_IPV4_ADDRESS(dhcp_end, kCreateIPv4Address(192, 168, 4, 100));
                CREATE_UINT8_BITFIELD_MIN_MAX(channel, 8, 0, 255, 1, 7);
                CREATE_ENUM_BITFIELD_SIZE_DEFAULT(encryption, 8, EncryptionType, std::underlying_type<EncryptionType>::type, uint8, kWiFiEncryptionTypeDefault);

                SoftAPConfig_t();

                IPAddress getAddress() const {
                    return address;
                }
                IPAddress getSubnet() const {
                    return subnet;
                }
                IPAddress getGateway() const {
                    return gateway;
                }
                IPAddress getDhcpStart() const {
                    return dhcp_start;
                }
                IPAddress getDhcpEnd() const {
                    return dhcp_end;
                }
                uint8_t getChannel() const {
                    return channel;
                }
                EncryptionType getEncryption() const {
                    return static_cast<EncryptionType>(encryption);
                }
            } SoftAPConfig_t;
        };

        class SoftAP : public SoftAPConfig, public ConfigGetterSetter<SoftAPConfig::SoftAPConfig_t, _H(MainConfig().network.softap.cfg) CIF_DEBUG(, &handleNameSoftAPConfig_t)> {
        public:

            static void defaults();

        };

        // --------------------------------------------------------------------
        // WiFi

        class WiFi {
        public:
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().network.wifi, SSID, 1, 32);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().network.wifi, Password, 8, 32);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().network.wifi, SoftApSSID, 1, 32);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().network.wifi, SoftApPassword, 8, 32);
        };

        Settings settings;
        SoftAP softap;
        WiFi wifi;
    };

}
