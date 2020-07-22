/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "serial2tcp.h"
#include "Serial2TcpBase.h"
#include <LoopFunctions.h>
#include <Configuration.h>
#include <KFCForms.h>
#include <kfc_fw_config.h>
#include "at_mode.h"

#if DEBUG_SERIAL2TCP
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

static Serial2TcpPlugin plugin;

PROGMEM_DEFINE_PLUGIN_OPTIONS(
    Serial2TcpPlugin,
    "serial2tcp",       // name
    "Serial2TCP",       // friendly name
    "",                 // web_templates
    "serial2tcp",       // config_forms
    "network",          // reconfigure_dependencies
    PluginComponent::PriorityType::SERIAL2TCP,
    PluginComponent::RTCMemoryId::NONE,
    static_cast<uint8_t>(PluginComponent::MenuType::AUTO),
    false,              // allow_safe_mode
    false,              // setup_after_deep_sleep
    true,               // has_get_status
    true,               // has_config_forms
    false,              // has_web_ui
    false,              // has_web_templates
    true,               // has_at_mode
    0                   // __reserved
);

Serial2TcpPlugin::Serial2TcpPlugin() : PluginComponent(PROGMEM_GET_PLUGIN_OPTIONS(Serial2TcpPlugin))
{
    REGISTER_PLUGIN(this, "Serial2TcpPlugin");
}

void Serial2TcpPlugin::setup(SetupModeType mode)
{
    if (Serial2TcpBase::getInstance()) {
        return;
    }
    auto instance = Serial2TcpBase::createInstance(Serial2TCP::getConfig());
    if (instance) {
        instance->begin();
    }
}

void Serial2TcpPlugin::reconfigure(const String &source)
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

void Serial2TcpPlugin::createConfigureForm(FormCallbackType type, const String &formName, Form &form, AsyncWebServerRequest *request)
{
    if (!isCreateFormCallbackType(type)) {
        return;
    }

    using KFCConfigurationClasses::MainConfig;
    auto &cfg = Serial2TcpPlugin::Serial2TCP::getWriteableConfig();
    FormUI::ItemsList serialPorts;
    FormUI::ItemsList modes;

    serialPorts.emplace_back(String(static_cast<int>(Serial2TcpPlugin::Serial2TCP::SerialPortType::SERIAL0)), String(F("Serial Port 0")));
    serialPorts.emplace_back(String(static_cast<int>(Serial2TcpPlugin::Serial2TCP::SerialPortType::SERIAL1)), String(F("Serial Port 1")));
    serialPorts.emplace_back(String(static_cast<int>(Serial2TcpPlugin::Serial2TCP::SerialPortType::SOFTWARE)), String(F("Software Sewrial")));

    modes.emplace_back(String(static_cast<int>(Serial2TcpPlugin::Serial2TCP::ModeType::SERVER)), String(F("Server")));
    modes.emplace_back(String(static_cast<int>(Serial2TcpPlugin::Serial2TCP::ModeType::CLIENT)), String(F("Client")));

    form.setFormUI(F("Serial2TCP Settings"));

    form.add<uint8_t>(F("mode"), &cfg.mode_byte)->setFormUI((new FormUI(FormUI::TypeEnum_t::SELECT, F("Mode:")))->addItems(modes));

    form.add<uint8_t>(F("s_port"), &cfg.serial_port_byte)->setFormUI((new FormUI(FormUI::TypeEnum_t::SELECT, F("Serial Port:")))->addItems(serialPorts));
    form.add<uint32_t>(F("baud"), &cfg.baudrate)->setFormUI((new FormUI(FormUI::TypeEnum_t::TEXT, F("Baud:"))));
    form.add<uint8_t>(F("rxpin"), &cfg.rx_pin)->setFormUI((new FormUI(FormUI::TypeEnum_t::TEXT, F("Rx Pin:"))));
    form.add<uint8_t>(F("txpin"), &cfg.tx_pin)->setFormUI((new FormUI(FormUI::TypeEnum_t::TEXT, F("Tx Pin:"))));

    form.add(F("host"), _H_STR_VALUE(MainConfig().plugins.serial2tcp.hostname))->setFormUI((new FormUI(FormUI::TypeEnum_t::TEXT, F("Host:"))));
    form.add<uint16_t>(F("tcp_port"), &cfg.port)->setFormUI((new FormUI(FormUI::TypeEnum_t::TEXT, F("TCP Port:"))));
    form.add<bool>(F("auth"), &cfg.authentication)->setFormUI((new FormUI(FormUI::TypeEnum_t::SELECT, F("Authenication:")))->setBoolItems(FSPGM(yes), FSPGM(no)));
    form.add(F("username"), _H_STR_VALUE(MainConfig().plugins.serial2tcp.username))->setFormUI((new FormUI(FormUI::TypeEnum_t::TEXT, F("Username:"))));
    form.add(F("password"), _H_STR_VALUE(MainConfig().plugins.serial2tcp.password))->setFormUI((new FormUI(FormUI::TypeEnum_t::PASSWORD, F("Password:"))));

    form.add<bool>(F("autocnn"), &cfg.auto_connect)->setFormUI((new FormUI(FormUI::TypeEnum_t::SELECT, F("Auto Connect:")))->setBoolItems(FSPGM(yes), FSPGM(no)));
    form.add<uint8_t>(F("autoreconn"), &cfg.auto_reconnect)->setFormUI((new FormUI(FormUI::TypeEnum_t::TEXT, F("Auto Reconnect:")))->setSuffix(F("seconds, 0 = disable")));
    form.add<uint16_t>(F("idltimeout"), &cfg.idle_timeout)->setFormUI((new FormUI(FormUI::TypeEnum_t::TEXT, F("Idle TImeout:")))->setSuffix(F("seconds, 0 = disable")));

    form.finalize();
}

#if AT_MODE_SUPPORTED

#include "at_mode.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(S2TCP, "S2TCP", "<0=disable/1=server/2=client>", "Enable or disable Serial2Tcp");

void Serial2TcpPlugin::atModeHelpGenerator()
{
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(S2TCP), getName_P());
}

bool Serial2TcpPlugin::atModeHandler(AtModeArgs &args)
{
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
