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
    serialPorts.emplace_back(String(static_cast<int>(Serial2TcpPlugin::Serial2TCP::SerialPortType::SOFTWARE)), String(F("Software Serial")));

    modes.emplace_back(String(static_cast<int>(Serial2TcpPlugin::Serial2TCP::ModeType::NONE)), String(FSPGM(Disabled)));
    modes.emplace_back(String(static_cast<int>(Serial2TcpPlugin::Serial2TCP::ModeType::SERVER)), String(F("Server")));
    modes.emplace_back(String(static_cast<int>(Serial2TcpPlugin::Serial2TCP::ModeType::CLIENT)), String(F("Client")));

    #ifndef NUM_DIGITAL_PINS
    #define NUM_DIGITAL_PINS 256
    #endif

    form.setFormUI(F("Serial2TCP Settings"));

    form.add<uint8_t>(F("mode"), _H_W_STRUCT_VALUE(cfg, mode_byte))->setFormUI((new FormUI(FormUI::TypeEnum_t::SELECT, F("Mode:")))->addItems(modes));

    auto &allGroup = form.addDivGroup(F("allset"), F("{'i':'#mode','e':'var A=$(\\'#authdiv,#idle_group\\');var C=$(\\'#conn_group\\')','s':{'0':'$T.hide()','1':'C.hide();A.show();$T.show()','2':'C.show();A.hide();$T.show()'}}"));
    form.add<uint8_t>(F("s_port"), _H_W_STRUCT_VALUE(cfg, serial_port_byte))->setFormUI((new FormUI(FormUI::TypeEnum_t::SELECT, F("Serial Port:")))->addItems(serialPorts));
    form.add<uint32_t>(F("baud"), _H_W_STRUCT_VALUE(cfg, baudrate))->setFormUI((new FormUI(FormUI::TypeEnum_t::TEXT, F("Baud:")))->setPlaceholder(F("Default")));

    auto &customGroup = form.addDivGroup(F("scustom"), F("{'i':'#s_port','m':'$T.hide()','s':{'2':'$T.show()'}}"));
    form.add<uint8_t>(F("rxpin"), _H_W_STRUCT_VALUE(cfg, rx_pin))->setFormUI((new FormUI(FormUI::TypeEnum_t::TEXT, F("Rx Pin:"))));
    form.addValidator(new FormRangeValidator(0, NUM_DIGITAL_PINS - 1));
    form.add<uint8_t>(F("txpin"), _H_W_STRUCT_VALUE(cfg, tx_pin))->setFormUI((new FormUI(FormUI::TypeEnum_t::TEXT, F("Tx Pin:"))));
    form.addValidator(new FormRangeValidator(0, NUM_DIGITAL_PINS - 1));
    customGroup.endDiv();

    auto &connGroup = form.addDivGroup(F("conn_group"));
    form.add(F("host"), _H_STR_VALUE(MainConfig().plugins.serial2tcp.hostname))->setFormUI((new FormUI(FormUI::TypeEnum_t::TEXT, F("Host:"))));
    form.add<uint16_t>(F("tcp_port"), _H_W_STRUCT_VALUE(cfg, port))->setFormUI((new FormUI(FormUI::TypeEnum_t::TEXT, F("TCP Port:"))));
    form.addValidator(new FormRangeValidator(FSPGM(Invalid_port), 1, 65535));
    form.add<bool>(F("autocnn"), _H_W_STRUCT_VALUE(cfg, auto_connect))->setFormUI((new FormUI(FormUI::TypeEnum_t::SELECT, F("Auto Connect:")))->setBoolItems());
    form.add<uint8_t>(F("autoreconn"), _H_W_STRUCT_VALUE(cfg, auto_reconnect))->setFormUI((new FormUI(FormUI::TypeEnum_t::TEXT, F("Auto Reconnect Delay:")))->setSuffix(F("seconds, 0 = disable")));
    form.addValidator(new FormRangeValidator(0, 255));
    connGroup.endDiv();

    auto &idleGroup = form.addDivGroup(F("idle_group"));
    form.add<uint16_t>(F("idltimeout"), _H_W_STRUCT_VALUE(cfg, idle_timeout))->setFormUI((new FormUI(FormUI::TypeEnum_t::TEXT, F("Idle Timeout:")))->setSuffix(F("seconds, 0 = disable")));
    form.addValidator(new FormRangeValidator(0, 65535));
    idleGroup.endDiv();

    auto &authDivGroup = form.addDivGroup(F("authdiv"));
    form.add<bool>(F("auth"), _H_W_STRUCT_VALUE(cfg, authentication))->setFormUI((new FormUI(FormUI::TypeEnum_t::SELECT, F("Authentication:")))->setBoolItems());
    auto &authGroup = form.addDivGroup(F("auth_group"), F("{'i':'#auth','s':{'0':'$T.hide()','1':'$T.show()'}}"));
    form.add(F("s2t_user"), _H_STR_VALUE(MainConfig().plugins.serial2tcp.username))->setFormUI((new FormUI(FormUI::TypeEnum_t::TEXT, F("Username:"))));
    auto password = Serial2TcpPlugin::Serial2TCP::getPassword();
    form.add(F("s2t_pass"), _H_STR_VALUE(MainConfig().plugins.serial2tcp.password))->setFormUI((new FormUI(FormUI::TypeEnum_t::PASSWORD, F("Password:")))->addAttribute(F("value"), password));
    authGroup.endDiv();
    authDivGroup.endDiv();

    allGroup.endDiv();

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
