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
    "channels,controller,automation",
    "",                 // reconfigure_dependencies
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
    MQTT::Client::registerComponent(this);
    LoopFunctions::add(loopMethod);
}

void BlindsControlPlugin::reconfigure(const String &source)
{
    _stop();
    _readConfig();
    for(auto pin : _config.pins) {
        digitalWrite(pin, LOW);
        pinMode(pin, OUTPUT);
    }
}

void BlindsControlPlugin::shutdown()
{
    _stop();
    LoopFunctions::add(loopMethod);
    MQTT::Client::unregisterComponent(this);
}

void BlindsControlPlugin::getStatus(Print &output)
{
    output.printf_P(PSTR("PWM %.2fkHz" HTML_S(br)), _config.pwm_frequency / 1000.0);
#if IOT_BLINDS_CTRL_RPM_PIN
    output.print(F("Position sensing and stall protection" HTML_S(br)));
#endif

    for(const auto channel: _states.channels()) {
        auto &channelConfig = _config.channels[*channel];
        output.printf_P(PSTR("Channel %s, state %s, open/close time limit  %ums / %ums, current limit %umA/%.3fms" HTML_S(br)),
            Plugins::Blinds::getChannelName(*channel),
            _states[channel]._getFPStr(),
            channelConfig.open_time_ms,
            channelConfig.close_time_ms,
            channelConfig.current_limit_mA,
            channelConfig.current_avg_period_us / 1000.0
        );
    }
}

void BlindsControlPlugin::createMenu()
{
    auto configMenu = bootstrapMenu.getMenuItem(navMenu.config);
    configMenu.addMenuItem(F("Blinds Channels"), F("blinds/channels.html"));
    configMenu.addMenuItem(F("Blinds Automation"), F("blinds/automation.html"));
    configMenu.addMenuItem(getFriendlyName(), F("blinds/controller.html"));
}

void BlindsControlPlugin::createWebUI(WebUINS::Root &webUI) {

    auto row = &webUI.addRow();
    row->addGroup(F("Blinds"), false);

    row = &webUI.addRow();
    row->addSwitch(FSPGM(set_all), String(F("Both Channels")), true, WebUINS::NamePositionType::TOP).setColumns(4);

    row = &webUI.addRow();
    for(const auto channel: _states.channels()) {

        String prefix = PrintString(FSPGM(channel__u), channel);

        // row = &webUI.addRow();
        row->addBadgeSensor(prefix + F("_state"), Plugins::Blinds::getChannelName(*channel), JsonString()).add(JJ(head), F("h5"));

        // row = &webUI.addRow();
        // row->addSwitch(prefix + F("_set"), String(F("Channel ")) + String(*channel));
    }
    row = &webUI.addRow();
    for(const auto channel: _states.channels()) {

        String prefix = PrintString(FSPGM(channel__u), channel);

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

BlindsControlPlugin &BlindsControlPlugin::getInstance()
{
    return plugin;
}

void BlindsControlPlugin::loopMethod()
{
    plugin._loopMethod();
}


#if AT_MODE_SUPPORTED

#include "at_mode.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(BCME, "BCME", "<open|close|stop|tone>[,<channel>][,<tone_frequency>,<tone_pwm_value>]", "Open, close a channel, stop motor or run tone test");

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
        if (args.requireArgs(1, 8)) {
            auto cmds = PSTR("open|close|stop|tone|imperial");
            int cmd = stringlist_find_P_P(cmds, args.get(0), '|');
            int channel = args.toIntMinMax(1, 0, 1, 0);
            switch(cmd) {
                case 0:
                case 1:
                    _startMotor(static_cast<ChannelType>(channel), cmd == 0);
                    args.printf_P(PSTR("motor channel=%u direction=%s start"), channel, cmd == 0 ? PSTR("open") : PSTR("close"));
                    args.printf_P(PSTR("pin=%u pwm=%u soft-start=%u"), _motorPin, _motorPWMValue, _motorStartTime != 0);
                    break;
                case 3:
                case 4: {
                        _queue.clear();
                        _stop();
                        auto play_tone_channel = _config.play_tone_channel;
                        auto tone_frequency = _config.tone_frequency;
                        auto tone_pwm_value = _config.tone_pwm_value;
                        auto resetConfig = [&]() {
                                _config.play_tone_channel = play_tone_channel;
                                _config.tone_frequency = tone_frequency;
                                _config.tone_pwm_value = tone_pwm_value;
                        };
                        if (args.size() >= 2) {
                            _config.play_tone_channel = 1 + channel;
                        }
                        _config.tone_frequency = args.toIntMinMax(2, _config.kMinValueFor_tone_frequency, _config.kMaxValueFor_tone_frequency, _config.tone_frequency);;
                        _config.tone_pwm_value = args.toIntMinMax(3, _config.kMinValueFor_tone_pwm_value, _config.kTypeMaxValueFor_tone_pwm_value, _config.tone_pwm_value);;
                        args.printf_P(PSTR("testing tone timer for 15 seconds: channel=%u frequency=%u pwm_value=%u"), _config.play_tone_channel - 1, _config.tone_frequency, _config.tone_pwm_value);
#if HAVE_IMPERIAL_MARCH
                        if (cmd == 4) {
                            uint16_t speed = args.toIntMinMax<uint16_t>(4, 50, 1000, 80);
                            int8_t zweiklang = args.toIntMinMax<int8_t>(5, -15, 15, 0);
                            uint8_t repeat = args.toIntMinMax<uint8_t>(6, 0, 255, 1);
                            uint8_t delaySync =  args.toIntMinMax<uint8_t>(7, 30, 180, 0);
                            if (delaySync) {
                                uint32_t now = ((time(nullptr) / delaySync) * delaySync) + delaySync + 5;

                                PrintString str;
                                str.strftime_P(PSTR("%H:%M:%S"), now);
                                str.strftime_P(PSTR(" now=%H:%M:%S"), time(nullptr));
                                args.printf_P(PSTR("Imperial march @ %s in=%u seconds)"), str.c_str(), now - time(nullptr));

                                _Scheduler.add(10, true, [this, now, speed, zweiklang, repeat, resetConfig](Event::CallbackTimerPtr timer) {
                                    if ((uint32_t)time(nullptr) <= now) {
                                        return;
                                    }
                                    timer->disarm();
                                    _playImerialMarch(speed, zweiklang, repeat);
                                    resetConfig();
                                }, Event::PriorityType::HIGHEST);
                            }
                            else {
                                args.print(F("Imperial march"));
                                _playImerialMarch(speed, zweiklang, repeat);
                                resetConfig();
                            }

                        } else {
                            _startToneTimer(15000);
                            resetConfig();
                        }
#else
                        _startToneTimer(15000);
                        resetConfig();
#endif
                    } break;
                default:
                    _stop();
                    args.printf_P(PSTR("motor stopped"));
                    break;
            }
        }
        return true;
    }
    return false;
}

#endif
