    // --------------------------------------------------------------------
    // Network

    struct Network {

        // --------------------------------------------------------------------
        // Settings
        class SettingsConfig {
        public:
            typedef struct __attribute__packed__ SettingsConfig_t {
                using Type = SettingsConfig_t;
                CREATE_IPV4_ADDRESS(local_ip);
                CREATE_IPV4_ADDRESS(subnet);
                CREATE_IPV4_ADDRESS(gateway);
                CREATE_IPV4_ADDRESS(dns1);
                CREATE_IPV4_ADDRESS(dns2);

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
                CREATE_IPV4_ADDRESS(address);
                CREATE_IPV4_ADDRESS(subnet);
                CREATE_IPV4_ADDRESS(gateway);
                CREATE_IPV4_ADDRESS(dhcp_start);
                CREATE_IPV4_ADDRESS(dhcp_end);
                uint8_t channel;
                union __attribute__packed__ {
                    EncryptionType encryption_enum;
                    uint8_t encryption;
                };
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
                    return encryption_enum;
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
