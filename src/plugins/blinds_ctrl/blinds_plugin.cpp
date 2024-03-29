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
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

// Plugin

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

void BlindsControlPlugin::setup(SetupModeType mode, const PluginComponents::DependenciesPtr &dependencies)
{
    _setup();
    MQTT::Client::registerComponent(this);
    LOOP_FUNCTION_ADD(loopMethod);

    // update states when wifi has been connected
    WiFiCallbacks::add(WiFiCallbacks::EventType::CONNECTED, [this](WiFiCallbacks::EventType event, void *payload) {
        _publishState();
    }, this);

    // update states when wifi is already connected
    if (WiFi.isConnected()) {
        _publishState();
    }

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
    WiFiCallbacks::remove(WiFiCallbacks::EventType::CONNECTED, this);
    LOOP_FUNCTION_ADD(loopMethod);
    MQTT::Client::unregisterComponent(this);
}

void BlindsControlPlugin::getStatus(Print &output)
{
    output.printf_P(PSTR("PWM %.2fkHz" HTML_S(br)), _config.pwm_frequency / 1000.0);

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
    {
        auto configMenu = bootstrapMenu.getMenuItem(navMenu.config);
        auto subMenu = configMenu.addSubMenu(getFriendlyName());
        subMenu.addMenuItem(F("Channels"), F("blinds/channels.html"));
        subMenu.addMenuItem(F("Automation"), F("blinds/automation.html"));
        subMenu.addMenuItem(F("Controller"), F("blinds/controller.html"));
    }
    // {
    //     auto homeMenu = bootstrapMenu.getMenuItem(navMenu.home);
    //     auto subMenu = homeMenu.addSubMenu(F("Blinds Configuration"));
    //     subMenu.addMenuItem(F("Channels"), F("blinds/channels.html"));
    //     subMenu.addMenuItem(F("Automation"), F("blinds/automation.html"));
    //     subMenu.addMenuItem(F("Controller"), F("blinds/controller.html"));
    // }
}

void BlindsControlPlugin::createWebUI(WebUINS::Root &webUI)
{
    webUI.addRow(WebUINS::Row(WebUINS::Group(F("Blinds"), false)));

    WebUINS::Row row(
        WebUINS::Switch(FSPGM(set_all), F("Both Channels"), true, WebUINS::NamePositionType::TOP, 4)
    );
    webUI.addRow(row);

    WebUINS::Row row2;
    WebUINS::Row row3;
    for(const auto channel: _states.channels()) {
        String prefix = PrintString(FSPGM(channel__u), channel);
        WebUINS::BadgeSensor sensor(prefix + F("_state"), reinterpret_cast<WebUINS::FStr>(Plugins::Blinds::getChannelName(*channel)), F(""));
        sensor.append(WebUINS::NamedString(J(head), F("h5")));
        row2.append(sensor);
        row3.append(WebUINS::Switch(prefix + F("_set"), String(F("Channel ")) + String(*channel)));
    }
    webUI.addRow(row2);
    webUI.addRow(row3);
}

BlindsControlPlugin &BlindsControlPlugin::getInstance()
{
    return plugin;
}

#if AT_MODE_SUPPORTED

#include "at_mode.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(BCME, "BCME", "<open|close|stop|tone|imperial|init>[,<channel>][,<tone_frequency>,<tone_pwm_value>]", "Open, close a channel, stop motor or run tone test, play imperial march, init. state");

#if AT_MODE_HELP_SUPPORTED

ATModeCommandHelpArrayPtr BlindsControlPlugin::atModeCommandHelp(size_t &size) const
{
    static ATModeCommandHelpArray tmp PROGMEM = {
        PROGMEM_AT_MODE_HELP_COMMAND(BCME),
    };
    size = sizeof(tmp) / sizeof(tmp[0]);
    return tmp;
}

#endif

bool BlindsControlPlugin::atModeHandler(AtModeArgs &args)
{
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(BCME))) {
        if (args.requireArgs(1, 8)) {
            auto cmds = PSTR("open|close|stop|tone|imperial|init");
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
                                    args.printf_P(PSTR("Imperial march @ %s in=%u seconds)"), str.c_str(), (uint32_t)(now - time(nullptr)));

                                    _Scheduler.add(10, true, [this, now, speed, zweiklang, repeat, resetConfig](Event::CallbackTimerPtr timer) {
                                        if ((uint32_t)time(nullptr) <= now) {
                                            return;
                                        }
                                        timer->disarm();
                                        _playImperialMarch(speed, zweiklang, repeat);
                                        resetConfig();
                                    }, Event::PriorityType::HIGHEST);
                                }
                                else {
                                    args.print(F("Imperial march"));
                                    _playImperialMarch(speed, zweiklang, repeat);
                                    resetConfig();
                                }

                            }
                            else {
                                _startToneTimer(_config.tone_pwm_value * 1000U);
                                resetConfig();
                            }
                        #else
                            _startToneTimer(_config.tone_pwm_value * 1000U);
                            resetConfig();
                        #endif
                    } break;
                case 5:
                    if (args.size() < 3) {
                        args.print(F("init,<channel>,<open|closed>"));
                    }
                    else {
                        if (args.isTrue(2, _states[channel].isOpen())) {
                            _states[channel] = StateType::OPEN;
                        }
                        else if (args.isFalse(2, _states[channel].isClosed())) {
                            _states[channel] = StateType::CLOSED;
                        }
                        args.printf_P(PSTR("set channel #%u state to %s"), channel, _states[channel]._getFPStr());
                        _saveState();
                    }
                    break;
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
