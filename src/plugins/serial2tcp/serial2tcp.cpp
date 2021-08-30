/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "serial2tcp.h"
#include "Serial2TcpBase.h"
#include <LoopFunctions.h>
#include <Configuration.hpp>
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

void Serial2TcpPlugin::setup(SetupModeType mode, const PluginComponents::DependenciesPtr &dependencies)
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

void Serial2TcpPlugin::createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request)
{
    if (!isCreateFormCallbackType(type)) {
        return;
    }

    using KFCConfigurationClasses::MainConfig;
    auto &cfg = Serial2TcpPlugin::Serial2TCP::getWriteableConfig();
    FormUI::Container::List serialPorts;
    FormUI::Container::List modes;

    using SerialPortType = Serial2TcpPlugin::Serial2TCP::SerialPortType;
    using ModeType = Serial2TcpPlugin::Serial2TCP::ModeType;

    serialPorts.emplace_back(enumToString(SerialPortType::SERIAL0), String(F("Serial Port 0")));
    serialPorts.emplace_back(enumToString(SerialPortType::SERIAL1), String(F("Serial Port 1")));
    serialPorts.emplace_back(enumToString(SerialPortType::SOFTWARE), String(FSPGM(Software_Serial, "Software Serial")));

    modes.emplace_back(enumToString(ModeType::NONE), String(FSPGM(Disabled)));
    modes.emplace_back(enumToString(ModeType::SERVER), String(FSPGM(Server)));
    modes.emplace_back(enumToString(ModeType::CLIENT), String(FSPGM(Client)));

    #ifndef NUM_DIGITAL_PINS
    #define NUM_DIGITAL_PINS 256
    #endif

    form.setFormUI(F("Serial2TCP Settings"));

    form.add<uint8_t>(FSPGM(mode), _H_W_STRUCT_VALUE(cfg, mode_byte))->setFormUI(new FormUI::UI(FormUI::Type::SELECT, FSPGM(Mode)))->addItems(modes));

    auto &allGroup = form.addDivGroup(F("allset"), F("{'i':'#mode','e':'var A=$(\\'#authdiv,#idle_group\\');var C=$(\\'#conn_group\\')','s':{'0':'$T.hide()','1':'C.hide();A.show();$T.show()','2':'C.show();A.hide();$T.show()'}}"));
    form.add<uint8_t>(F("s_port"), _H_W_STRUCT_VALUE(cfg, serial_port_byte))->setFormUI(new FormUI::UI(FormUI::Type::SELECT, F("Serial Port:")))->addItems(serialPorts));
    form.add<uint32_t>(F("baud"), _H_W_STRUCT_VALUE(cfg, baudrate))->setFormUI(new FormUI::UI(FormUI::Type::TEXT, F("Baud:")))->setPlaceholder(FSPGM(Default)));

    auto &customGroup = form.addDivGroup(F("scustom"), F("{'i':'#s_port','m':'$T.hide()','s':{'2':'$T.show()'}}"));
    form.add<uint8_t>(F("rxpin"), _H_W_STRUCT_VALUE(cfg, rx_pin))->setFormUI(new FormUI::UI(FormUI::Type::TEXT, F("Rx Pin:"))));
    form.addValidator(FormUI::Validator::Range(0, NUM_DIGITAL_PINS - 1));
    form.add<uint8_t>(F("txpin"), _H_W_STRUCT_VALUE(cfg, tx_pin))->setFormUI(new FormUI::UI(FormUI::Type::TEXT, F("Tx Pin:"))));
    form.addValidator(FormUI::Validator::Range(0, NUM_DIGITAL_PINS - 1));
    customGroup.end();

    auto &connGroup = form.addDivGroup(F("conn_group"));
    form.add(FSPGM(host), _H_STR_VALUE(MainConfig().plugins.serial2tcp.hostname))->setFormUI(new FormUI::UI(FormUI::Type::TEXT, FSPGM(Hostname))));
    // FormUI::ZeroconfSuffix()
    form.add<uint16_t>(FSPGM(port), _H_W_STRUCT_VALUE(cfg, port))->setFormUI(new FormUI::UI(FormUI::Type::TEXT, FSPGM(Port))));
    form.addValidator(FormNetworkPortValidator());
    form.add<bool>(F("autocnn"), _H_W_STRUCT_VALUE(cfg, auto_connect))->setFormUI(new FormUI::UI(FormUI::Type::SELECT, F("Auto Connect")))->setBoolItems());
    form.add<uint8_t>(F("autoreconn"), _H_W_STRUCT_VALUE(cfg, auto_reconnect))->setFormUI(new FormUI::UI(FormUI::Type::TEXT, F("Auto Reconnect Delay")))->setSuffix(F("seconds, 0 = disable")));
    form.addValidator(FormUI::Validator::Range(0, 255));
    connGroup.end();

    auto &idleGroup = form.addDivGroup(F("idle_group"));
    form.add<uint16_t>(F("idltimeout"), _H_W_STRUCT_VALUE(cfg, idle_timeout))->setFormUI(new FormUI::UI(FormUI::Type::TEXT, F("Idle Timeout")))->setSuffix(F("seconds, 0 = disable")));
    //FormUI::IntAttribute(F("disabled-value"), 0)
    form.addValidator(FormUI::Validator::Range(0, 65535));
    idleGroup.end();

    auto &authDivGroup = form.addDivGroup(F("authdiv"));
    form.add<bool>(FSPGM(auth), _H_W_STRUCT_VALUE(cfg, authentication))->setFormUI(new FormUI::UI(FormUI::Type::SELECT, FSPGM(Authentication)))->setBoolItems());
    auto &authGroup = form.addDivGroup(F("auth_group"), F("{'i':'#auth','s':{'0':'$T.hide()','1':'$T.show()'}}"));
    form.add(F("s2t_user"), _H_STR_VALUE(MainConfig().plugins.serial2tcp.username))->setFormUI(new FormUI::UI(FormUI::Type::TEXT, FSPGM(Username))));
    auto password = Serial2TcpPlugin::Serial2TCP::getPassword();
    form.add(F("s2t_pass"), _H_STR_VALUE(MainConfig().plugins.serial2tcp.password))->setFormUI(new FormUI::UI(FormUI::Type::PASSWORD, FSPGM(Password)))->addAttribute(FSPGM(value), password));
    authGroup.end();
    authDivGroup.end();

    allGroup.end();

    form.finalize();
}

#if AT_MODE_SUPPORTED

#include "at_mode.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(S2TCP, "S2TCP", "<0=disable/1=server/2=client>", "Enable or disable Serial2Tcp");

ATModeCommandHelpArrayPtr Serial2TcpPlugin::atModeCommandHelp(size_t &size) const
{
    static ATModeCommandHelpArray tmp PROGMEM = {
        PROGMEM_AT_MODE_HELP_COMMAND(S2TCP)
    };
    size = sizeof(tmp) / sizeof(tmp[0]);
    return tmp;
}

bool Serial2TcpPlugin::atModeHandler(AtModeArgs &args)
{
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(S2TCP))) {
        if (args.requireArgs(1, 1)) {
            using KFCConfigurationClasses::System;

            auto &cfg = Serial2TCP::getWriteableConfig();
            auto &flags = System::Flags::getWriteableConfig();
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
