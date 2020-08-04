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
    "blinds",           // config_forms
    "mqtt",             // reconfigure_dependencies
    PluginComponent::PriorityType::BLINDS,
    PluginComponent::RTCMemoryId::NONE,
    static_cast<uint8_t>(PluginComponent::MenuType::AUTO),
    false,              // allow_safe_mode
    false,              // setup_after_deep_sleep
    true,               // has_get_status
    true,               // has_config_forms
    true,               // has_web_ui
    false,              // has_web_templates
    true,               // has_at_mode
    0                   // __reserved
);

BlindsControlPlugin::BlindsControlPlugin() : PluginComponent(PROGMEM_GET_PLUGIN_OPTIONS(BlindsControlPlugin)), BlindsControl(), _isTestMode(false)
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
        _readConfig();
    }
}

void BlindsControlPlugin::shutdown()
{
    LoopFunctions::add(loopMethod);
    MQTTClient::safeUnregisterComponent(this);
}

void BlindsControlPlugin::getStatus(Print &output)
{
    output.printf_P(PSTR("PWM %.2fkHz" HTML_S(br)), IOT_BLINDS_CTRL_PWM_FREQ / 1000.0);
#if IOT_BLINDS_CTRL_RPM_PIN
    output.print(F("Position sensing and stall protection" HTML_S(br)));
#endif

    for(const auto channel: _states.channels()) {
        auto &channelConfig = BlindsControl::_config.channels[*channel];
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

void BlindsControlPlugin::createWebUI(WebUI &webUI) {

    auto row = &webUI.addRow();
    row->setExtraClass(JJ(title));
    row->addGroup(F("Blinds"), false);

    for(const auto channel: _states.channels()) {
        String prefix = PrintString(FSPGM(channel__u), channel);

        row = &webUI.addRow();
        row->addBadgeSensor(prefix + F("_state"), Plugins::Blinds::getChannelName(*channel), JsonString());

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

#if IOT_BLINDS_CTRL_TESTMODE

#if !AT_MODE_SUPPORTED
#error Test mode requires AT_MODE_SUPPORTED=1
#endif

void BlindsControlPlugin::loopMethod()
{
    if (plugin._isTestMode) {
        plugin._testLoopMethod();
    }
    else {
        plugin._loopMethod();
    }
}

#else

void BlindsControlPlugin::loopMethod()
{
    plugin._loopMethod();
}

#endif

#if AT_MODE_SUPPORTED && IOT_BLINDS_CTRL_TESTMODE

#include "at_mode.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(BCME, "BCME", "<channel=0/1>,<direction=0/1>,<max.time/ms>,<level=0-1023>,<limit/mA>,<max.time over limit/ms>", "Enable motor for a specific time");

void BlindsControlPlugin::_printTestInfo() {

#if IOT_BLINDS_CTRL_RPM_PIN
    Serial.printf_P(PSTR("%umA %u peak %u rpm %u position %u\n"), (unsigned)(_adcIntegral * BlindsControllerConversion::kConvertADCValueToCurrentMulitplier), _adcIntegral, _peakCurrent, _getRpm(), _rpmCounter);
#else
    Serial.printf_P(PSTR("%umA %u peak %u\n"), (unsigned)(_adcIntegral * BlindsControllerConversion::kConvertADCValueToCurrentMulitplier), _adcIntegral, _peakCurrent);
#endif
}

void BlindsControlPlugin::_testLoopMethod()
{
    if (_motorTimeout.isActive()) {
        _updateAdc();

        _peakCurrent = std::max((uint16_t)_adcIntegral, _peakCurrent);
        if (_adcIntegral > _currentLimit && millis() % 2 == _currentLimitCounter % 2) {
            if (++_currentLimitCounter > _currentLimitMinCount) {
                _isTestMode = false;
                _printTestInfo();
                _stop();
                Serial.println(F("+BCME: Current limit"));
                return;
            }
        }
        else if (_adcIntegral < _currentLimit * 0.8) {
            _currentLimitCounter = 0;
        }
#if IOT_BLINDS_CTRL_RPM_PIN
        if (_hasStalled()) {
            _isTestMode = false;
            _printTestInfo();
            _stop();
            Serial.println(F("+BCME: Stalled"));
            return;
        }
#endif
        if (_motorTimeout.reached()) {
            _isTestMode = false;
            _printTestInfo();
            _stop();
            Serial.println(F("+BCME: Timeout"));
        }
        else if (_printCurrentTimeout.reached(true)) {
            _printTestInfo();
            _peakCurrent = 0;
        }
    }
}

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
        if (args.requireArgs(6, 6)) {
            uint8_t channel = args.toInt(0);
            bool direction = args.toInt(1);
            uint32_t time = args.toInt(2);
            uint16_t pwmLevel = args.toInt(3);

            channel %= kChannelCount;

            _currentLimit = args.toInt(4) * BlindsControllerConversion::kConvertCurrentToADCValueMulitplier;
            _currentLimitMinCount = args.toInt(5);
            _activeChannel = static_cast<ChannelType>(channel);

            analogWriteFreq(IOT_BLINDS_CTRL_PWM_FREQ);
            for(uint i = 0; i < 4; i++) {
                analogWrite(BlindsControl::_config.pins[i], LOW);
            }
            analogWrite(BlindsControl::_config.pins[(channel * kChannelCount) + direction], pwmLevel);
            args.printf_P(PSTR("level %u current limit/%u %u frequency %.2fkHz"), pwmLevel, _currentLimit, _currentLimitMinCount, IOT_BLINDS_CTRL_PWM_FREQ / 1000.0);
            args.printf_P(PSTR("channel %u direction %u time %u"), channel, direction, time);

            _peakCurrent = 0;
            _printCurrentTimeout.set(500);
            _motorTimeout.set(time);
            _isTestMode = true;
            _clearAdc();
        }
        return true;
    }
    return false;
}

#endif
