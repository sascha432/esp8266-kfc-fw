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
        // SoftAP

        class SoftAPConfig {
        public:
            using EncryptionType = WiFiEncryptionType;

            struct __attribute__packed__ SoftAPSettings {
                using Type = SoftAPSettings;
                CREATE_IPV4_ADDRESS(address, kCreateIPv4Address(192, 168, 4, 1));
                CREATE_IPV4_ADDRESS(subnet, kCreateIPv4Address(255, 255, 255, 0));
                CREATE_IPV4_ADDRESS(gateway, kCreateIPv4Address(192, 168, 4, 1));
                CREATE_IPV4_ADDRESS(dhcp_start, kCreateIPv4Address(192, 168, 4, 2));
                CREATE_IPV4_ADDRESS(dhcp_end, kCreateIPv4Address(192, 168, 4, 100));
                CREATE_UINT8_BITFIELD_MIN_MAX(channel, 8, 0, 255, 1, 7);
                CREATE_ENUM_BITFIELD_SIZE_DEFAULT(encryption, 8, EncryptionType, std::underlying_type<EncryptionType>::type, uint8, kWiFiEncryptionTypeDefault);

                SoftAPSettings();

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
            };

            static constexpr size_t kSoftAPSettingsSize = sizeof(SoftAPSettings);
        };


        class SoftAP : public SoftAPConfig, public ConfigGetterSetter<SoftAPConfig::SoftAPSettings, _H(MainConfig().network.softap.cfg) CIF_DEBUG(, &handleNameSoftAPConfig)> {
        public:

            static void defaults();

        };

        // --------------------------------------------------------------------
        // WiFi

        #ifndef WIFI_STATION_MAX_NUM
            #define WIFI_STATION_MAX_NUM 5
        #endif

        #if WIFI_STATION_MAX_NUM < 1 || WIFI_STATION_MAX_NUM > 8
            #error WIFI_STATION_MAX_NUM: supported WiFi stations are 1 to 8
        #endif

        class SettingsConfig {
        public:
            struct __attribute__packed__ StationModeSettings {
                static constexpr uint32_t kGlobalDNS = ~0;

                using Type = StationModeSettings;
                CREATE_IPV4_ADDRESS(local_ip, 0);
                CREATE_IPV4_ADDRESS(subnet, kCreateIPv4Address(255, 255, 255, 0));
                CREATE_IPV4_ADDRESS(gateway, kCreateIPv4Address(192, 168, 4, 1));
                CREATE_IPV4_ADDRESS(dns1, kGlobalDNS);
                CREATE_IPV4_ADDRESS(dns2, kGlobalDNS);
                CREATE_UINT8_BITFIELD_MIN_MAX(priority, 6, 0, 63, 1, 1);
                CREATE_UINT8_BITFIELD_MIN_MAX(enabled, 1, false, true, 1, false);
                CREATE_UINT8_BITFIELD_MIN_MAX(dhcp, 1, false, true, 1, false);

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
                    return (dns1 == kGlobalDNS) ? Network::Settings::getConfig().global_dns1 : dns1;
                }
                IPAddress getDns2() const {
                    return (dns2 == kGlobalDNS) ? Network::Settings::getConfig().global_dns2 : dns2;
                }
                uint8_t getPriority() const {
                    return priority;
                }
                String getPriorityStr() const {
                    if (getPriority() == 0) {
                        return F("AUTO");
                    }
                    return String(getPriority());
                }
                bool isEnabled() const {
                    return enabled;
                }
                bool isEnabled(int num) const {
                    return enabled && *Network::WiFi::getSSID(num);
                }
                bool isDHCPEnabled() const {
                    return dhcp;
                }
                StationModeSettings();
            };

            struct __attribute__packed__ StationsConfig {
                using Type = StationsConfig;

                static constexpr int kNumStations = WIFI_STATION_MAX_NUM;

                CREATE_IPV4_ADDRESS(global_dns1, kCreateIPv4Address(8, 8, 8, 8));
                CREATE_IPV4_ADDRESS(global_dns2, kCreateIPv4Address(8, 8, 4, 4));
                StationModeSettings stations[kNumStations];
                uint8_t activeNetwork;

                StationsConfig() :
                    global_dns1(kDefaultValueFor_global_dns1),
                    global_dns2(kDefaultValueFor_global_dns2),
                    stations{},
                    activeNetwork(0)
                {
                    stations[0].enabled = true;
                    for(int i = 0; i < kNumStations; i++) {
                        stations[i].local_ip = createIPv4Address(192, 168, 4, (i * 10) + 10);
                        stations[i].priority = (i * 7) + 1;
                    }
                }
            };

            static constexpr size_t kWiFiConfigSize = sizeof(StationsConfig);
        };


        class Settings : public SettingsConfig, public ConfigGetterSetter<SettingsConfig::StationsConfig, _H(MainConfig().network.settings.cfg) CIF_DEBUG(, &handleStationsConfig)> {
        public:
            static constexpr uint32_t kGlobalDNS = SettingsConfig::StationModeSettings::kGlobalDNS;
        public:
            static void defaults();
        };

        class WiFi {
        public:
            using StationModeSettings = SettingsConfig::StationModeSettings;

            enum StationConfigType : uint8_t {
                CFG_DEFAULT = 0,
                CFG_0 = CFG_DEFAULT,
                #if WIFI_STATION_MAX_NUM > 1
                    CFG_1,
                #endif
                #if WIFI_STATION_MAX_NUM > 2
                    CFG_2,
                #endif
                #if WIFI_STATION_MAX_NUM > 3
                    CFG_3,
                #endif
                #if WIFI_STATION_MAX_NUM > 4
                    CFG_4,
                #endif
                #if WIFI_STATION_MAX_NUM > 5
                    CFG_5,
                #endif
                #if WIFI_STATION_MAX_NUM > 6
                    CFG_6,
                #endif
                #if WIFI_STATION_MAX_NUM > 7
                    CFG_7,
                #endif
                // max number
                MAX,
                // last configuration
                CFG_LAST = MAX - 1,
                // invalid id & keep current configuration
                CFG_INVALID = 255,
                CFG_KEEP = CFG_INVALID
            };

            static constexpr auto kNumStationsMax = StationConfigType::MAX;
            static constexpr auto kNumStations = static_cast<int>(kNumStationsMax);
            static_assert(kNumStations == WIFI_STATION_MAX_NUM, "kNumStations does not match WIFI_STATION_MAX_NUM");

        public:
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().network.wifi, SSID0, 1, 32);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().network.wifi, Password0, 8, 64);
            #if WIFI_STATION_MAX_NUM > 1
                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().network.wifi, SSID1, 1, 32);
                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().network.wifi, Password1, 8, 64);
            #endif
            #if WIFI_STATION_MAX_NUM > 2
                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().network.wifi, SSID2, 1, 32);
                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().network.wifi, Password2, 8, 64);
            #endif
            #if WIFI_STATION_MAX_NUM > 3
                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().network.wifi, SSID3, 1, 32);
                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().network.wifi, Password3, 8, 64);
            #endif
            #if WIFI_STATION_MAX_NUM > 4
                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().network.wifi, SSID4, 1, 32);
                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().network.wifi, Password4, 8, 64);
            #endif
            #if WIFI_STATION_MAX_NUM > 5
                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().network.wifi, SSID5, 1, 32);
                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().network.wifi, Password5, 8, 64);
            #endif
            #if WIFI_STATION_MAX_NUM > 6
                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().network.wifi, SSID6, 1, 32);
                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().network.wifi, Password6, 8, 64);
            #endif
            #if WIFI_STATION_MAX_NUM > 7
                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().network.wifi, SSID7, 1, 32);
                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().network.wifi, Password7, 8, 64);
            #endif

            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().network.wifi, SoftApSSID, 1, 32);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().network.wifi, SoftApPassword, 8, 32);

            struct StationConfig {
                StationConfigType _id;
                String _SSID;
                uint32_t _priority;
                uint8_t _bssid[6];

                StationConfig(StationConfigType id, const char *ssid, uint32_t priority) : StationConfig(id, FPSTR(ssid), priority)
                {
                }

                StationConfig(StationConfigType id, const __FlashStringHelper *ssid, uint32_t priority) :
                    _id(id),
                    _SSID(ssid),
                    _priority(priority),
                    _bssid{}
                {
                }

                bool operator < (const StationConfig &station) const {
                    return _priority < station._priority;
                }
            };
            using StationVector = std::vector<StationConfig>;
            using ScanCallback = std::function<void (StationVector &list)>;

            static StationVector getStations(const ScanCallback &scanCallback)
            {
                StationVector list;
                bool scan = false;
                auto cfg = Network::Settings::getConfig();
                for(int i = 0; i < kNumStations; i++) {
                    auto &config = cfg.stations[i];
                    if (config.isEnabled()) {
                        list.emplace_back(static_cast<StationConfigType>(i), getFPStrSSID(i), config._get_priority());
                        if (config._get_priority() == 0) {
                            scan = true;
                        }
                    }
                }
                if (scan && scanCallback) {
                    scanCallback(list);
                }
                srand(micros());
                std::sort(list.begin(), list.end(), [](const StationConfig &a, const StationConfig &b) -> bool {
                    if (a._priority == b._priority) {
                        return rand() % 2;
                    }
                    return a._priority < b._priority;
                });
                #if DEBUG_CONFIG_CLASS
                    int n = 0;
                    for(const auto &station: list) {
                        __DBG_printf("num=%u id=%u ssid=%s prio=%u bssid=%s", n++, station._id, station._SSID.c_str(), station._priority, mac2String(station._bssid).c_str());
                    }
                #endif
                return list;
            }

            static StationModeSettings getNetworkConfigOrdered(StationConfigType num)
            {
                auto list = getStations(nullptr); // TODO implement scanning for strongest signal
                auto count = static_cast<int>(num);
                for(const auto &station: list) {
                    if (count-- == 0) {
                        #if DEBUG_CONFIG_CLASS
                            if (static_cast<int>(station._id) != (static_cast<int>(station._id) % Network::Settings::StationsConfig::kNumStations)) {
                                __DBG_printf("invalid station num=%u id=%u ssid=%s new_id=%u", static_cast<int>(num), station._id, __S(station._SSID), static_cast<int>(station._id) % Network::Settings::StationsConfig::kNumStations);
                                for(int i = 0; i < Network::Settings::StationsConfig::kNumStations; i++) {
                                    const auto &station = Network::Settings::getConfig().stations[i];
                                    __DBG_printf("n=%u ip=%s", i, IPAddress(station.local_ip).toString().c_str());
                                }
                            }
                        #endif
                        return Network::Settings::getConfig().stations[static_cast<int>(station._id) % Network::Settings::StationsConfig::kNumStations];
                    }
                }
                __LDBG_printf("network out of range=%u", num);
                return Network::Settings::getConfig().stations[0];
            }

            static StationModeSettings getNetworkConfig(StationConfigType num)
            {
                return Network::Settings::getConfig().stations[static_cast<uint8_t>(num)];
            }

            static StationModeSettings &getWriteableNetworkConfig(StationConfigType num)
            {
                return Network::Settings::getWriteableConfig().stations[static_cast<uint8_t>(num)];
            }

            static const char *getSSID(uint8_t num)
            {
                return getSSID(static_cast<StationConfigType>(num));
            }

            static const __FlashStringHelper *getFPStrSSID(uint8_t num)
            {
                auto ssid = getSSID(static_cast<StationConfigType>(num));
                if (ssid && *ssid) {
                    return FPSTR(ssid);
                }
                if (Network::Settings::getConfig().stations[num].isEnabled()) {
                    return F("<No SSID>");
                }
                return F("<Disabled>");
            }

            static const char *getSSID(StationConfigType num)
            {
                switch(num) {
                    case StationConfigType::CFG_0:
                        return getSSID0();
                    #if WIFI_STATION_MAX_NUM > 1
                        case StationConfigType::CFG_1:
                            return getSSID1();
                    #endif
                    #if WIFI_STATION_MAX_NUM > 2
                        case StationConfigType::CFG_2:
                            return getSSID2();
                    #endif
                    #if WIFI_STATION_MAX_NUM > 3
                        case StationConfigType::CFG_3:
                            return getSSID3();
                    #endif
                    #if WIFI_STATION_MAX_NUM > 4
                        case StationConfigType::CFG_4:
                            return getSSID4();
                    #endif
                    #if WIFI_STATION_MAX_NUM > 5
                        case StationConfigType::CFG_5:
                            return getSSID5();
                    #endif
                    #if WIFI_STATION_MAX_NUM > 6
                        case StationConfigType::CFG_6:
                            return getSSID6();
                    #endif
                    #if WIFI_STATION_MAX_NUM > 7
                        case StationConfigType::CFG_7:
                            return getSSID7();
                    #endif
                    default:
                        break;
                }
                return nullptr;
            }

            static void setSSID(uint8_t num, const String &ssid)
            {
                setSSID(static_cast<StationConfigType>(num), ssid);
            }

            static void setSSID(StationConfigType num, const String &ssid)
            {
                switch(num) {
                    case StationConfigType::CFG_0:
                        setSSID0(ssid);
                        break;
                    #if WIFI_STATION_MAX_NUM > 1
                        case StationConfigType::CFG_1:
                            setSSID1(ssid);
                            break;
                    #endif
                    #if WIFI_STATION_MAX_NUM > 2
                        case StationConfigType::CFG_2:
                            setSSID2(ssid);
                            break;
                    #endif
                    #if WIFI_STATION_MAX_NUM > 3
                        case StationConfigType::CFG_3:
                            setSSID3(ssid);
                            break;
                    #endif
                    #if WIFI_STATION_MAX_NUM > 4
                        case StationConfigType::CFG_4:
                            setSSID4(ssid);
                            break;
                    #endif
                    #if WIFI_STATION_MAX_NUM > 5
                        case StationConfigType::CFG_5:
                            setSSID5(ssid);
                            break;
                    #endif
                    #if WIFI_STATION_MAX_NUM > 6
                        case StationConfigType::CFG_6:
                            setSSID6(ssid);
                            break;
                    #endif
                    #if WIFI_STATION_MAX_NUM > 7
                        case StationConfigType::CFG_7:
                            setSSID7(ssid);
                            break;
                    #endif
                    default:
                        break;
                }
            }

            static const char *getPassword(uint8_t num)
            {
                return getPassword(static_cast<StationConfigType>(num));
            }

            static const char *getPassword(StationConfigType num)
            {
                switch(num) {
                    case StationConfigType::CFG_0:
                        return getPassword0();
                    #if WIFI_STATION_MAX_NUM > 1
                        case StationConfigType::CFG_1:
                            return getPassword1();
                    #endif
                    #if WIFI_STATION_MAX_NUM > 2
                        case StationConfigType::CFG_2:
                            return getPassword2();
                    #endif
                    #if WIFI_STATION_MAX_NUM > 3
                        case StationConfigType::CFG_3:
                            return getPassword3();
                    #endif
                    #if WIFI_STATION_MAX_NUM > 4
                        case StationConfigType::CFG_4:
                            return getPassword4();
                    #endif
                    #if WIFI_STATION_MAX_NUM > 5
                        case StationConfigType::CFG_5:
                            return getPassword5();
                    #endif
                    #if WIFI_STATION_MAX_NUM > 6
                        case StationConfigType::CFG_6:
                            return getPassword6();
                    #endif
                    #if WIFI_STATION_MAX_NUM > 7
                        case StationConfigType::CFG_7:
                            return getPassword7();
                    #endif
                    default:
                        break;
                }
                return nullptr;
            }

            static void setPassword(uint8_t num, const String &password)
            {
                setPassword(static_cast<StationConfigType>(num), password);
            }

            static void setPassword(StationConfigType num, const String &password)
            {
                switch(num) {
                    case StationConfigType::CFG_0:
                        setPassword0(password);
                        break;
                    #if WIFI_STATION_MAX_NUM > 1
                        case StationConfigType::CFG_1:
                            setPassword1(password);
                            break;
                    #endif
                    #if WIFI_STATION_MAX_NUM > 2
                        case StationConfigType::CFG_2:
                            setPassword2(password);
                            break;
                    #endif
                    #if WIFI_STATION_MAX_NUM > 3
                        case StationConfigType::CFG_3:
                            setPassword3(password);
                            break;
                    #endif
                    #if WIFI_STATION_MAX_NUM > 4
                        case StationConfigType::CFG_4:
                            setPassword4(password);
                            break;
                    #endif
                    #if WIFI_STATION_MAX_NUM > 5
                        case StationConfigType::CFG_5:
                            setPassword5(password);
                            break;
                    #endif
                    #if WIFI_STATION_MAX_NUM > 6
                        case StationConfigType::CFG_6:
                            setPassword6(password);
                            break;
                    #endif
                    #if WIFI_STATION_MAX_NUM > 7
                        case StationConfigType::CFG_7:
                            setPassword7(password);
                            break;
                    #endif
                    default:
                        break;
                }
            }
        };

        Settings settings;
        SoftAP softap;
        WiFi wifi;
    };

}
