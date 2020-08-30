/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <LoopFunctions.h>
#include <MicrosTimer.h>
#include <KFCForms.h>
#include <kfc_fw_config.h>
#include <PluginComponent.h>
#include <PrintHtmlEntitiesString.h>
#include "blinds_plugin.h"
#include "blinds_defines.h"
#include "BlindsControl.h"
#include "plugins_menu.h"
#include "../src/plugins/mqtt/mqtt_client.h"

#if DEBUG_IOT_BLINDS_CTRL
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

// Plugin

extern int8_t operator *(const BlindsControl::ChannelType type);

static BlindsControlPlugin plugin;

PROGMEM_DEFINE_PLUGIN_OPTIONS(
    BlindsControlPlugin,
    "blinds",           // name
    "Blinds Controller",// friendly name
    "",                 // web_templates
    // config_forms
    "channels,controller",
    "mqtt",             // reconfigure_dependencies
    PluginComponent::PriorityType::BLINDS,
    PluginComponent::RTCMemoryId::NONE,
    static_cast<uint8_t>(PluginComponent::MenuType::CUSTOM),
    false,              // allow_safe_mode
    false,              // setup_after_deep_sleep
    true,               // has_get_status
    true,               // has_config_forms
    true,               // has_web_ui
    false,              // has_web_templates
    true,               // has_at_mode
    0                   // __reserved
);

BlindsControlPlugin::BlindsControlPlugin() : PluginComponent(PROGMEM_GET_PLUGIN_OPTIONS(BlindsControlPlugin)), BlindsControl()
{
    REGISTER_PLUGIN(this, "BlindsControlPlugin");
}

void BlindsControlPlugin::setup(SetupModeType mode)
{
    _setup();
    MQTTClient::safeRegisterComponent(this);
    LoopFunctions::add(loopMethod);
}

void BlindsControlPlugin::reconfigure(const String &source)
{
    if (String_equals(source, FSPGM(mqtt))) {
        MQTTClient::safeReRegisterComponent(this);
    }
    else {
        _stop();
        _readConfig();
        for(auto pin : _config.pins) {
            digitalWrite(pin, LOW);
            pinMode(pin, OUTPUT);
        }
    }
}

void BlindsControlPlugin::shutdown()
{
    _stop();
    LoopFunctions::add(loopMethod);
    MQTTClient::safeUnregisterComponent(this);
}

void BlindsControlPlugin::getStatus(Print &output)
{
    output.printf_P(PSTR("PWM %.2fkHz" HTML_S(br)), _config.pwm_frequency / 1000.0);
#if IOT_BLINDS_CTRL_RPM_PIN
    output.print(F("Position sensing and stall protection" HTML_S(br)));
#endif

    for(const auto channel: _states.channels()) {
        auto &channelConfig = _config.channels[*channel];
        output.printf_P(PSTR("Channel %s, state %s, open/close time limit  %ums / %ums, current limit %umA/%ums" HTML_S(br)),
            Plugins::Blinds::getChannelName(*channel),
            _states[channel]._getFPStr(),
            channelConfig.open_time,
            channelConfig.close_time,
            channelConfig.current_limit,
            channelConfig.current_limit_time
        );
    }
}

void BlindsControlPlugin::createMenu()
{
    bootstrapMenu.addSubMenu(F("Blinds Channels"), F("blinds/channels.html"), navMenu.config);
    bootstrapMenu.addSubMenu(getFriendlyName(), F("blinds/controller.html"), navMenu.config);
}

void BlindsControlPlugin::createWebUI(WebUI &webUI) {

    auto row = &webUI.addRow();
    row->setExtraClass(JJ(title));
    row->addGroup(F("Blinds"), false);

    row = &webUI.addRow();
    row->addSwitch(FSPGM(set_all), String(F("Both Channels")));

    for(const auto channel: _states.channels()) {

        String prefix = PrintString(FSPGM(channel__u), channel);

        row = &webUI.addRow();
        row->addBadgeSensor(prefix + F("_state"), Plugins::Blinds::getChannelName(*channel), JsonString()).add(JJ(head), F("h5"));

        row = &webUI.addRow();
        row->addSwitch(prefix + F("_set"), String(F("Channel ")) + String(*channel));
    }
}

void BlindsControlPlugin::getValues(JsonArray &array)
{
    BlindsControl::getValues(array);
}

void BlindsControlPlugin::setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState)
{
    BlindsControl::setValue(id, value, hasValue, state, hasState);
}


#if IOT_BLINDS_CTRL_RPM_PIN

void BlindsControl::rpmIntCallback(InterruptInfo info) {
    _rpmIntCallback(info);
}

#endif

void BlindsControlPlugin::loopMethod()
{
    plugin._loopMethod();
}


#if AT_MODE_SUPPORTED && IOT_BLINDS_CTRL_TESTMODE

#include "at_mode.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(BCME, "BCME", "<adc|div|open|close>[,<value>]", "Set or display test mode value\n"
        "\n"
        "adc,<client_id>            send ADC readings to web socket client\n"
);

// void BlindsControlPlugin::_printTestInfo() {

// #if IOT_BLINDS_CTRL_RPM_PIN
//     Serial.printf_P(PSTR("%umA %u peak %u rpm %u position %u\n"), (unsigned)(_adcIntegral * BlindsControllerConversion::kConvertADCValueToCurrentMulitplier), _adcIntegral, _peakCurrent, _getRpm(), _rpmCounter);
// #else
//     Serial.printf_P(PSTR("%umA %u peak %u\n"), (unsigned)(_adcIntegral * BlindsControllerConversion::kConvertADCValueToCurrentMulitplier), _adcIntegral, _peakCurrent);
// #endif
// }

// void BlindsControlPlugin::_resetTestMode()
// {
//     _stop();
//     LoopFunctions::add(loopMethod);
//     LoopFunctions::remove(testLoopMethod);
// }

// void BlindsControlPlugin::_testLoopMethod()
// {
//     if (_motorTimeout.isActive()) {
//         _updateAdc();

//         if (_adcIntegral > _currentLimit) {
//             if (_currentLimitTimer.reached()) {
//                 _resetTestMode();
//                 _printTestInfo();
//                 Serial.printf_P(PSTR("+BCME: Current limit time=%dms\n"), _currentLimitTimer.getDelay());
//                 return;
//             }
//             else if (!_currentLimitTimer.isActive()) {
//                 _currentLimitTimer.restart();
//             }
//         }
//         else if (_currentLimitTimer.isActive() && _adcIntegral < _currentLimit * 0.8) {   // reset current limit counter if current drops below 80%
//             Serial.printf_P(PSTR("+BCME: Current limit <80%, reset time=%dms\n"), _currentLimitTimer.get());
//             _currentLimitTimer.disable();
//         }

// #if IOT_BLINDS_CTRL_RPM_PIN
//         if (_hasStalled()) {
//             _resetTestMode();
//             _printTestInfo();
//             Serial.println(F("+BCME: Stalled"));
//             return;
//         }
// #endif
//         if (_motorTimeout.reached()) {
//             _resetTestMode();
//             _printTestInfo();
//             Serial.println(F("+BCME: Timeout"));
//         }
//         else if (_printCurrentTimeout.reached(true)) {
//             _printTestInfo();
//             _peakCurrent = 0;
//         }
//     }
//     else {
//         _resetTestMode();
//         Serial.println(F("+BCME: Motor inactive"));
//     }
// }

ATModeCommandHelpArrayPtr BlindsControlPlugin::atModeCommandHelp(size_t &size) const
{
    static ATModeCommandHelpArray tmp PROGMEM = {
        PROGMEM_AT_MODE_HELP_COMMAND(BCME),
    };
    size = sizeof(tmp) / sizeof(tmp[0]);
    return tmp;
}

bool BlindsControlPlugin::atModeHandler(AtModeArgs &args)
{
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(BCME))) {
        if (args.requireArgs(1, 4)) {
            auto cmds = PSTR("adc|div|open|close");
            int cmd = stringlist_find_P_P(cmds, args.get(0), '|');
            switch(cmd) {
                case 0:
                    if (args.size() == 2) {
                        _storeValues = args.isTrue(1);
                    }
                    args.printf_P(PSTR("store=%u"), _storeValues);
                    break;
                case 1:
                    if (args.size() == 2) {
                        _adcIntegralMultiplier = 1.0 / (1000.0 / args.toFloatMinMax(1, 0.1, 10000.0, 40.0));
                    }
                    args.printf_P(PSTR("ADC multiplier=%.4f/us %.1f/ms"), _adcIntegralMultiplier, _adcIntegralMultiplier * 1000.0);
                    break;
                case 2:
                case 3: {
                        auto channel = ((uint8_t)args.toInt(1)) % kChannelCount;
                        if (args.size() >= 3) {
                            _adcIntegralMultiplier = 1.0 / (1000.0 / args.toFloatMinMax(2, 0.1, 10000.0, 40.0));
                        }
                        if (args.size() >= 4) {
                            _config.channels[0].pwm_value =
                            _config.channels[1].pwm_value =
                                args.toIntMinMax(3, 0, PWMRANGE, 100);
                        }
                        if (_queue.empty()) {
                            _storeValues = true;
                            _activeChannel = (ChannelType)channel;
                            _states[_activeChannel] = StateType::UNKNOWN;
                            args.printf_P(PSTR("channel=%u action=%s multiplier=%.4f pwm=%u"), channel, cmd == 2 ? PSTR("open") : PSTR("close"), _adcIntegralMultiplier, _config.channels[0].pwm_value);
                            ActionType action;
                            if (channel == 0) {
                                if (cmd == 2) {
                                    action = ActionType::OPEN_CHANNEL0;
                                }
                                else {
                                    action = ActionType::CLOSE_CHANNEL0;
                                }
                            }
                            else {
                                if (cmd == 2) {
                                    action = ActionType::OPEN_CHANNEL1;
                                }
                                else {
                                    action = ActionType::CLOSE_CHANNEL1;
                                }
                            }
                            _queue.emplace_back(action, _activeChannel, 0);
                        }
                        else {
                            args.print(F("queue not empty"));
                        }
                    } break;
                    break;
                default:
                    args.printf_P(PSTR("expected <%s>"), cmds);
                    break;
            }
        }
        return true;
    }
    return false;
}

#endif
