/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "serial2tcp.h"
#include "Serial2TcpBase.h"
#include <LoopFunctions.h>
#include <Configuration.h>
#include <kfc_fw_config.h>
#include "at_mode.h"

#if DEBUG_SERIAL2TCP
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

static Serial2TcpPlugin plugin;

Serial2TcpPlugin::Serial2TcpPlugin()
{
    REGISTER_PLUGIN(this);
}

void Serial2TcpPlugin::setup(SetupModeType mode)
{
    auto instance = Serial2TcpBase::createInstance(Serial2TCP::getConfig());
    if (instance) {
        instance->begin();
    }
}

void Serial2TcpPlugin::reconfigure(PGM_P source)
{
    setup(SetupModeType::DEFAULT);
}

void Serial2TcpPlugin::shutdown()
{
    Serial2TcpBase::destroyInstance();
}

void Serial2TcpPlugin::getStatus(Print &output)
{
    using KFCConfigurationClasses::System;

    auto cfg = Serial2TCP::getConfig();
    if (Serial2TCP::isEnabled()) {
        if (cfg.mode == Serial2TCP::ModeType::SERVER) {
            output.printf_P(PSTR("Server listening on port %u"), cfg.port);
        }
        else {
            output.printf_P(PSTR("Client connecting to %s:%u"), Serial2TCP::getHostname(), cfg.port);
        }
        auto instance = Serial2TcpBase::getInstance();
        if (instance) {
            instance->getStatus(output);
        }
    }
    else {
        output.print(FSPGM(Disabled));
    }
}


void Serial2TcpPlugin::createConfigureForm(AsyncWebServerRequest *request, Form &form) {
    //TODO
}

#if AT_MODE_SUPPORTED

#include "at_mode.h"

#if DEBUG
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(S2TCPD, "S2TCPD", "<0/1>", "Enable or disable Serial2Tcp debug output");
#endif
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(S2TCP, "S2TCP", "<0=disable/1=server/2=client>", "Enable or disable Serial2Tcp");

void Serial2TcpPlugin::atModeHelpGenerator()
{
#if DEBUG
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(S2TCPD), getName());
#endif
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(S2TCP), getName());
}

bool Serial2TcpPlugin::atModeHandler(AtModeArgs &args)
{
#if DEBUG
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(S2TCPD))) {
        if (args.requireArgs(1, 1)) {
            Serial2TcpBase::_debugOutput = args.isTrue(0);
            args.printf_P(PSTR("debug output %s"), Serial2TcpBase::_debugOutput ? SPGM(enabled) : SPGM(disabled));
        }
        return true;
    }
    else
#endif
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(S2TCP))) {
        if (args.requireArgs(1, 1)) {
            using KFCConfigurationClasses::System;

            auto &cfg = Serial2TCP::getWriteableConfig();
            auto &flags = System::Flags::getWriteable();
            auto enable = args.toInt(0);
            if (enable) {
                if (enable == 2) {
                    args.print(F("Serial2TCP client with auto connect enabled"));
                    flags.serial2TCPEnabled = true;
                    cfg.auto_connect = true;
                    cfg.mode = Serial2TCP::ModeType::CLIENT;
                }
                else {
                    args.print(F("Serial2TCP server enabled"));
                    flags.serial2TCPEnabled = true;
                    cfg.mode = Serial2TCP::ModeType::SERVER;
                }
                auto instance = Serial2TcpBase::createInstance(cfg);
                if (instance) {
                    instance->begin();
                }
            }
            else {
                flags.serial2TCPEnabled = false;
                Serial2TcpBase::destroyInstance();
                args.print(F("Serial2TCP disabled"));
            }
        }
        return true;
    }
    return false;
}

#endif
