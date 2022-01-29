/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <EEPROM.h>

#ifndef DEBUG_CONFIGURATION
#    define DEBUG_CONFIGURATION 1
#endif

#ifndef DEBUG_CONFIGURATION_GETHANDLE
#    define DEBUG_CONFIGURATION_GETHANDLE 0
#endif

/*
http://192.168.0.10/file_manager/view?filename=%2F%2Epvt%2Fcfg%5Fhandles

<EEPROM>
<INVALID>
MainConfig().system.device.cfg
MainConfig().system.webserver.cfg
MainConfig().network.settings.cfg
MainConfig().plugins.alarm.cfg
MainConfig().plugins.serial2tcp.cfg
MainConfig().plugins.mqtt.cfg
MainConfig().plugins.syslog.cfg
MainConfig().plugins.ntpclient.cfg
MainConfig().plugins.sensor.cfg
MainConfig().system.flags.cfg
MainConfig().network.softap.cfg
MainConfig().plugins.blinds.cfg
MainConfig().plugins.dimmer.cfg
MainConfig().plugins.clock.cfg
MainConfig().plugins.ping.cfg
MainConfig().plugins.weatherstation.cfg
MainConfig().plugins.remote.cfg
MainConfig().system.firmware.MD5
MainConfig().system.firmware.PluginBlacklist
MainConfig().network.wifi.SoftApSSID
MainConfig().network.wifi.SSID0
MainConfig().system.device.Name
MainConfig().network.wifi.SSID1
MainConfig().network.wifi.SSID2
MainConfig().network.wifi.SSID3
MainConfig().network.wifi.SSID4
MainConfig().network.wifi.Password0
MainConfig().network.wifi.SoftApPassword
MainConfig().system.device.Title
MainConfig().plugins.ntpclient.Server1
MainConfig().plugins.ntpclient.Server2
MainConfig().plugins.ntpclient.Server3
MainConfig().plugins.ntpclient.PosixTimezone
MainConfig().plugins.mqtt.Hostname
MainConfig().plugins.mqtt.Username
MainConfig().plugins.mqtt.Password
MainConfig().plugins.mqtt.BaseTopic
MainConfig().system.device.Password
MainConfig().plugins.mqtt.AutoDiscoveryPrefix
MainConfig().plugins.syslog.Hostname
MainConfig().plugins.ntpclient.TimezoneName
MainConfig().network.wifi.Password1
MainConfig().network.wifi.Password2
MainConfig().network.wifi.Password3
MainConfig().network.wifi.Password4
MainConfig().system.device.Token
*/

#if DEBUG_CONFIGURATION
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

#ifndef CONFIG_MAGIC_DWORD
#    define CONFIG_MAGIC_DWORD 0xfef312f7
#endif

#define CONFIG_GET_HANDLE(name)     __get_constexpr_getHandle(_STRINGIFY(name))
#define CONFIG_GET_HANDLE_STR(name) __get_constexpr_getHandle(name)
#define _H(name)                    CONFIG_GET_HANDLE(name)
#define _HS(name)                   __get_constexpr_getHandle(name)

#if DEBUG_CONFIGURATION_GETHANDLE

#    define _H_GET(name)            get<decltype(name)>(__DBG__registerHandleName(PSTR(_STRINGIFY(name)), __DBG__TYPE_GET))
#    define _H_W_GET(name)          getWriteable<decltype(name)>(__DBG__registerHandleName(PSTR(_STRINGIFY(name)), __DBG__TYPE_W_GET))
#    define _H_SET(name, value)     set<decltype(name)>(__DBG__registerHandleName(PSTR(_STRINGIFY(name)), __DBG__TYPE_SET), value)
#    define _H_STR(name)            getString(__DBG__registerHandleName(PSTR(_STRINGIFY(name)), __DBG__TYPE_GET))
#    define _H_W_STR(name, max_len) getWriteableString(__DBG__registerHandleName(PSTR(_STRINGIFY(name)), __DBG__TYPE_W_GET), max_len)
#    define _H_SET_STR(name, value) setString(__DBG__registerHandleName(PSTR(_STRINGIFY(name)), __DBG__TYPE_SET), value)
#    define _H_GET_IP(name)         get<uint32_t>(__DBG__registerHandleName(PSTR(_STRINGIFY(name)), __DBG__TYPE_GET))
#    define _H_W_GET_IP(name)       getWriteable<uint32_t>(__DBG__registerHandleName(PSTR(_STRINGIFY(name)), __DBG__TYPE_W_GET))
#    define _H_SET_IP(name, value)  set<uint32_t>(__DBG__registerHandleName(PSTR(_STRINGIFY(name)), __DBG__TYPE_SET), (uint32_t)value)

#else

#    define _H_GET(name)            get<decltype(name)>(_H(name))
#    define _H_W_GET(name)          getWriteable<decltype(name)>(_H(name))
#    define _H_SET(name, value)     set<decltype(name)>(_H(name), value)
#    define _H_STR(name)            getString(_H(name))
#    define _H_W_STR(name, max_len) getWriteableString(_H(name), max_len)
#    define _H_SET_STR(name, value) setString(_H(name), value)
#    define _H_GET_IP(name)         get<uint32_t>(_H(name))
#    define _H_W_GET_IP(name)       getWriteable<uint32_t>(_H(name))
#    define _H_SET_IP(name, value)  set<uint32_t>(_H(name), (uint32_t)value)

#endif

#if DEBUG_CONFIGURATION_GETHANDLE

// log usage to file, 0 = disable
#    ifndef DEBUG_CONFIGURATION_GETHANDLE_LOG_INTERVAL
#        define DEBUG_CONFIGURATION_GETHANDLE_LOG_INTERVAL 5 // minutes
#    endif

// store all configuration handle names for debugging. needs a lot RAM
#    define __DBG__registerHandleName(name, type) ConfigurationHelper::registerHandleName(name, type)
#    define __DBG__checkIfHandleExists(type, handle)                                     \
        if (!ConfigurationHelper::registerHandleExists(handle)) {                        \
            __DBG_printf("handle=%04x no name registered, type=%s", handle, PSTR(type)); \
        }

#else

#    define __DBG__registerHandleName(...)
#    define __DBG__checkIfHandleExists(...)

#endif

#define __get_constexpr_getHandle(name) constexpr_crc16_update(name, constexpr_strlen(name))

class Configuration;
class ConfigurationParameter;

namespace ConfigurationHelper {

    using HandleType = uint16_t;
    using Handle_t = HandleType;
    using size_type = uint16_t;
    using ParameterHeaderType = uint32_t;

    enum class ParameterType : uint8_t { // 4 bit, 0-13 available as type
        _INVALID = 0,
        STRING,
        BINARY,
        BYTE,
        WORD,
        DWORD,
        QWORD,
        FLOAT,
        DOUBLE,
        MAX,
        _ANY = 0b1111,
    };
    using TypeEnum_t = ParameterType;

    static_assert((int)ParameterType::MAX < 15, "size exceeded");

    class ParameterInfo;
    class WriteableData;

    uint8_t *allocate(size_t size, size_t *realSize);
    void allocate(size_t size, ConfigurationParameter &parameter);
    void deallocate(ConfigurationParameter &parameter);
    size_type getParameterLength(ParameterType type, size_t length);

    #if DEBUG_CONFIGURATION_GETHANDLE

        const char *getHandleName(HandleType crc);

        const HandleType registerHandleName(const char *name, uint8_t type);
        const HandleType registerHandleName(const __FlashStringHelper *name, uint8_t type);
        bool registerHandleExists(HandleType handle);

        void setPanicMode(bool value);
        void addFlashUsage(HandleType handle, size_t readSize, size_t writeSize);

        void readHandles();
        void writeHandles(bool clear = false);
        void dumpHandles(Print &output, bool log);

    #else

        inline const char *getHandleName(HandleType crc)
        {
            return emptyString.c_str();
        }

    #endif

}

#if DEBUG_CONFIGURATION_GETHANDLE
#    include "DebugHandle.h"
#endif

#if DEBUG_CONFIGURATION
#    include <debug_helper_disable.h>
#endif
