/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_BLINDS_CTRL

#include <Arduino_compat.h>
#include <LoopFunctions.h>
#include <MicrosTimer.h>
#include <PrintHtmlEntitiesString.h>
#include "plugins.h"
#include "blinds_ctrl.h"
#include "BlindsControl.h"
#include "BlindsChannel.h"

#if DEBUG_IOT_BLINDS_CTRL
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

// Plugin

class BlindsControlPlugin : public PluginComponent, public BlindsControl {
public:
    BlindsControlPlugin();

    virtual PGM_P getName() const;
    virtual void setup(PluginSetupMode_t mode) override;

    virtual bool hasStatus() const override;
    virtual const String getStatus() override;

    virtual bool hasWebUI() const override;
    virtual void createWebUI(WebUI &webUI) override;
    virtual WebUIInterface *getWebUIInterface() override;

    static void loopMethod();


#if IOT_BLINDS_CTRL_RPM_PIN
    static void rpmIntCallback(InterruptInfo info);
#endif

#if AT_MODE_SUPPORTED
    virtual bool hasAtMode() const override;
    virtual void atModeHelpGenerator() override;
    virtual bool atModeHandler(Stream &serial, const String &command, int8_t argc, char **argv) override;

#if IOT_BLINDS_CTRL_TESTMODE
private:
    void _printTestInfo();
    void _testLoopMethod();

    MillisTimer _printCurrentTimeout;
    uint16_t _currentLimit;
    uint16_t _currentLimitMinCount;
    uint16_t _peakCurrent;
    bool _isTestMode;
#endif

#endif
};


static BlindsControlPlugin plugin;

BlindsControlPlugin::BlindsControlPlugin() : BlindsControl(), _isTestMode(false) {
    register_plugin(this);
}

PGM_P BlindsControlPlugin::getName() const {
    return PSTR("blindsctrl");
}

void BlindsControlPlugin::setup(PluginSetupMode_t mode) {

    _setup();
    LoopFunctions::add(loopMethod);
}

bool BlindsControlPlugin::hasStatus() const {
    return true;
}

const String BlindsControlPlugin::getStatus() {
    PrintHtmlEntitiesString str;
    str.printf_P(PSTR("PWM %.2fkHz" HTML_S(br)), IOT_BLINDS_CTRL_PWM_FREQ / 1000.0);

    _readConfig();

    for(uint8_t i = 0; i < 2; i++) {
        auto &_channel = _channels[i].getChannel();
        str.printf_P(PSTR("Channel %u, state %s, open %ums, close %ums, current limit %umA/%ums" HTML_S(br)),
            (i + 1),
            BlindsChannel::_stateStr(_channels[i].getState()),
            _channel.openTime,
            _channel.closeTime,
            (unsigned)ADC_TO_CURRENT(_channel.currentLimit),
            _channel.currentLimitTime
        );
    }
    return str;
}

bool BlindsControlPlugin::hasWebUI() const {
    return true;
}

WebUIInterface *BlindsControlPlugin::getWebUIInterface() {
    return this;
}

void BlindsControlPlugin::createWebUI(WebUI &webUI) {

    auto row = &webUI.addRow();
    row->setExtraClass(JJ(title));
    row->addGroup(F("Blinds"), false);

    row = &webUI.addRow();
    row->addBadgeSensor(FSPGM(blinds_controller_channel1_sensor), F("Turn"), F(""));

    row = &webUI.addRow();
    row->addSwitch(FSPGM(blinds_controller_channel1), F("Channel 1"));

    row = &webUI.addRow();
    row->addBadgeSensor(FSPGM(blinds_controller_channel2_sensor), F("Move"), F(""));

    row = &webUI.addRow();
    row->addSwitch(FSPGM(blinds_controller_channel2), F("Channel 2"));
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

void BlindsControlPlugin::loopMethod() {
    if (plugin._isTestMode) {
        plugin._testLoopMethod();
    }
    else {
        plugin._loopMethod();
    }
}

#else

void BlindsControlPlugin::loopMethod() {
    plugin._loopMethod();
}

#endif

#if AT_MODE_SUPPORTED

#include "at_mode.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(BCMS, "BCMS", "<channel=0/1>,<0=closed/1=open>", "Set channel state");

#if IOT_BLINDS_CTRL_TESTMODE

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(BCME, "BCME", "<channel=0/1>,<direction=0/1>,<max-time/ms>,<level=0-1023>,<frequency/Hz>,<limit=0-1023>,<limit-time>", "Enable motor # for max-time milliseconds");

void BlindsControlPlugin::_printTestInfo() {

#if IOT_BLINDS_CTRL_RPM_PIN
    MySerial.printf_P(PSTR("%umA %u peak %u rpm %u position %u\n"), ADC_TO_CURRENT(_adcIntegral), _adcIntegral, _peakCurrent, _getRpm(), _rpmCounter);
#else
    MySerial.printf_P(PSTR("%umA %u peak %u\n"), ADC_TO_CURRENT(_adcIntegral), _adcIntegral, _peakCurrent);
#endif
}

void BlindsControlPlugin::_testLoopMethod() {
    if (_motorTimeout.isActive()) {
        _updateAdc();

        _peakCurrent = std::max((uint16_t)_adcIntegral, _peakCurrent);
        if (_adcIntegral > _currentLimit && millis() % 2 == _currentLimitCounter % 2) {
            if (++_currentLimitCounter > _currentLimitMinCount) {
                _isTestMode = false;
                _printTestInfo();
                _stop();
                MySerial.println(F("+BCME: Current limit"));
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
            MySerial.println(F("+BCME: Stalled"));
            return;
        }
#endif
        if (_motorTimeout.reached()) {
            _isTestMode = false;
            _printTestInfo();
            _stop();
            MySerial.println(F("+BCME: Timeout"));
        }
        else if (_printCurrentTimeout.reached(true)) {
            _printTestInfo();
            _peakCurrent = 0;
        }
    }
}

#endif

bool BlindsControlPlugin::hasAtMode() const {
    return true;
}

void BlindsControlPlugin::atModeHelpGenerator() {
#if IOT_BLINDS_CTRL_TESTMODE
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(BCME));
#endif
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(BCMS));
}

bool BlindsControlPlugin::atModeHandler(Stream &serial, const String &command, int8_t argc, char **argv) {

    if (constexpr_String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(BCMS))) {
        if (argc == 2) {
            uint8_t channel = atoi(argv[0]) % 2;
            _channels[channel].setState(atoi(argv[1]) == 0 ? BlindsChannel::CLOSED : BlindsChannel::OPEN);
            _saveState();
            serial.printf_P(PSTR("+BCMS: channel %u state %s\n"), channel, BlindsChannel::_stateStr(_channels[channel].getState()));
        }
        else {
            at_mode_print_invalid_arguments(serial);
        }
        return true;
    }
#if IOT_BLINDS_CTRL_TESTMODE
    else if (constexpr_String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(BCME))) {
        if (argc == 7) {
            uint8_t pins[] = { IOT_BLINDS_CTRL_M1_PIN, IOT_BLINDS_CTRL_M2_PIN, IOT_BLINDS_CTRL_M3_PIN, IOT_BLINDS_CTRL_M4_PIN };
            uint8_t channel = atoi(argv[0]) % 2;
            uint8_t direction = atoi(argv[1]) % 2;
            uint32_t time = atoi(argv[2]);
            uint16_t pwmLevel = atoi(argv[3]);
            uint32_t pwmFrequency = atoi(argv[4]);
            _currentLimit = atoi(argv[5]);
            _currentLimitMinCount = atoi(argv[6]);

            analogWriteFreq(pwmFrequency);
            for(uint i = 0; i < 4; i++) {
                analogWrite(pins[i], LOW);
            }
            analogWrite(pins[(channel << 1) | direction], pwmLevel);
            serial.printf_P(PSTR("+BCME: level %u current limit/%u %u frequency %.2fkHz\n"), pwmLevel, _currentLimit, _currentLimitMinCount, pwmFrequency / 1000.0);
            serial.printf_P(PSTR("+BCME: channel %u direction %s time %u\n"), channel, direction == 0 ? PSTR("down") : PSTR("up"), time);

            _peakCurrent = 0;
            _printCurrentTimeout.set(500);
            _motorTimeout.set(time);
            _isTestMode = true;
            _clearAdc();
        }
        else {
            at_mode_print_invalid_arguments(serial);
        }
        return true;
    }
#endif
    return false;
}

#endif

#endif
