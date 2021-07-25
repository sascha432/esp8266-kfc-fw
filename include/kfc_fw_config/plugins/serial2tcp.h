/**
 * Author: sascha_lammers@gmx.de
 */

#include "kfc_fw_config/base.h"
#include "ConfigurationHelper.h"

namespace KFCConfigurationClasses {

    namespace Plugins {

        // --------------------------------------------------------------------
        // Serial2TCP

        namespace Serial2TCPConfigNS {

            class Serial2TCPConfig {
            public:
                enum class ModeType : uint8_t {
                    NONE,
                    SERVER,
                    CLIENT,
                    MAX
                };
                enum class SerialPortType : uint8_t {
                    SERIAL0,
                    SERIAL1,
                    SOFTWARE,
                    MAX
                };

                typedef struct __attribute__packed__ Serial2Tcp_t {
                    using Type = Serial2Tcp_t;
                    uint16_t port;
                    uint32_t baudrate;
                    CREATE_ENUM_BITFIELD(mode, ModeType);
                    CREATE_ENUM_BITFIELD(serial_port, SerialPortType);
                    CREATE_UINT8_BITFIELD(authentication, 1);
                    CREATE_UINT8_BITFIELD(auto_connect, 1);
                    uint8_t rx_pin;
                    uint8_t tx_pin;
                    uint8_t keep_alive;
                    uint8_t auto_reconnect;
                    uint16_t idle_timeout;

                    Serial2Tcp_t() : port(2323), baudrate(KFC_SERIAL_RATE), mode(cast_int_mode(ModeType::SERVER)), serial_port(cast_int_serial_port(SerialPortType::SERIAL0)), authentication(false), auto_connect(false), rx_pin(0), tx_pin(0), keep_alive(30), auto_reconnect(5), idle_timeout(300) {}

                } Serial2Tcp_t;
            };

            class Serial2TCP : public Serial2TCPConfig, KFCConfigurationClasses::ConfigGetterSetter<Serial2TCPConfig::Serial2Tcp_t, _H(MainConfig().plugins.serial2tcp.cfg) CIF_DEBUG(, &handleNameSerial2TCPConfig_t)> {
            public:

                static void defaults();
                static bool isEnabled();

                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.serial2tcp, Hostname, 1, 64);
                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.serial2tcp, Username, 3, 32);
                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.serial2tcp, Password, 6, 32);
            };

        }
    }
}
