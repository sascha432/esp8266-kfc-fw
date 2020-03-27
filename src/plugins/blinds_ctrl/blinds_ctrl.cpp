/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <LoopFunctions.h>
#include <MicrosTimer.h>
#include <KFCForms.h>
#include <PrintHtmlEntitiesString.h>
#include "plugins.h"
#include "blinds_ctrl.h"
#include "BlindsControl.h"
#include "BlindsChannel.h"
#include "progmem_data.h"

#if DEBUG_IOT_BLINDS_CTRL
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

// Plugin

class BlindsControlPlugin : public PluginComponent, public BlindsControl {
public:
    BlindsControlPlugin();

    virtual PGM_P getName() const {
        return PSTR("blindsctrl");
    }
    virtual const __FlashStringHelper *getFriendlyName() const {
        return F("Blinds Controller");
    }
    virtual void setup(PluginSetupMode_t mode) override;
    virtual void reconfigure(PGM_P source) override;

    virtual bool hasStatus() const override;
    virtual void getStatus(Print &output) override;

    virtual bool hasWebUI() const override;
    virtual void createWebUI(WebUI &webUI) override;
    virtual WebUIInterface *getWebUIInterface() override;

    virtual PGM_P getConfigureForm() const override {
        return PSTR("blinds");
    }
    virtual void createConfigureForm(AsyncWebServerRequest *request, Form &form) override;

#if AT_MODE_SUPPORTED
    virtual bool hasAtMode() const override;
    virtual void atModeHelpGenerator() override;
    virtual bool atModeHandler(AtModeArgs &args) override;

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

public:
    static void loopMethod();

#if IOT_BLINDS_CTRL_RPM_PIN
    static void rpmIntCallback(InterruptInfo info);
#endif
};


static BlindsControlPlugin plugin;

BlindsControlPlugin::BlindsControlPlugin() : BlindsControl(), _isTestMode(false)
{
    REGISTER_PLUGIN(this);
}

void BlindsControlPlugin::setup(PluginSetupMode_t mode)
{
    _setup();
    LoopFunctions::add(loopMethod);
}

void BlindsControlPlugin::reconfigure(PGM_P source) {
    _readConfig();
}

bool BlindsControlPlugin::hasStatus() const {
    return true;
}

void BlindsControlPlugin::getStatus(Print &output)
{
    output.printf_P(PSTR("PWM %.2fkHz" HTML_S(br)), IOT_BLINDS_CTRL_PWM_FREQ / 1000.0);
#if IOT_BLINDS_CTRL_RPM_PIN
    output.print(F("Position sensing and stall protection" HTML_S(br)));
#endif

    for(uint8_t i = 0; i < _channels.size(); i++) {
        auto &_channel = _channels[i].getChannel();
        output.printf_P(PSTR("Channel %u, state %s, open %ums, close %ums, current limit %umA/%ums" HTML_S(br)),
            (i + 1),
            BlindsChannel::_stateStr(_channels[i].getState()),
            _channel.openTime,
            _channel.closeTime,
            (unsigned)ADC_TO_CURRENT(_channel.currentLimit),
            _channel.currentLimitTime
        );
    }
}

bool BlindsControlPlugin::hasWebUI() const {
    return true;
}

WebUIInterface *BlindsControlPlugin::getWebUIInterface() {
    return this;
}

void BlindsControlPlugin::createConfigureForm(AsyncWebServerRequest *request, Form &form) {

    auto *blinds = &config._H_W_GET(Config().blinds_controller); // must be a pointer
    auto forward = F("Forward");
    auto reverse = F("Reverse (Open/close time is reversed as well)");
    auto mA = F("mA");
    auto ms = F("ms");
    auto motorSpeed = F("0-1023");
    FormUI::ItemsList currentLimitItems;

    currentLimitItems.emplace_back(F("5"), F("Extra Fast (5ms)"));
    currentLimitItems.emplace_back(F("20"), F("Fast (20ms)"));
    currentLimitItems.emplace_back(F("50"), F("Medium (50ms)"));
    currentLimitItems.emplace_back(F("150"), F("Slow (150ms)"));
    currentLimitItems.emplace_back(F("250"), F("Extra Slow (250ms)"));

    form.setFormUI(F("Blinds Controller"));

    form.add<bool>(F("channel0_dir"), &blinds->channel0_dir)->setFormUI((new FormUI(FormUI::SELECT, F("Channel 0 Direction")))->setBoolItems(reverse, forward));
    form.add<bool>(F("channel1_dir"), &blinds->channel1_dir)->setFormUI((new FormUI(FormUI::SELECT, F("Channel 1 Direction")))->setBoolItems(reverse, forward));
    form.add<bool>(F("swap_channels"), &blinds->swap_channels)->setFormUI((new FormUI(FormUI::SELECT, F("Swap Channels")))->setBoolItems(FSPGM(Yes), FSPGM(No)));

    for (uint8_t i = 0; i < _channels.size(); i++) {
        form.add<uint16_t>(PrintString(F("channel%u_close_time"), i), &blinds->channels[i].closeTime)
            ->setFormUI((new FormUI(FormUI::TEXT, PrintString(F("Channel %u Open Time Limit"), i)))->setSuffix(ms));
        form.add<uint16_t>(PrintString(F("channel%u_open_time"), i), &blinds->channels[i].openTime)
            ->setFormUI((new FormUI(FormUI::TEXT, PrintString(F("Channel % Close Time Limit"), i)))->setSuffix(ms));
        form.add<uint16_t>(PrintString(F("channel%u_current_limit"), i), &blinds->channels[i].currentLimit, [](uint16_t &value, FormField &field, bool isSetter){
                if (isSetter) {
                    value = CURRENT_TO_ADC(value);
                }
                else {
                    value = ADC_TO_CURRENT(value);
                }
                return true;
            })
            ->setFormUI((new FormUI(FormUI::TEXT, PrintString(F("Channel %u Current Limit"), i)))->setSuffix(mA));
        form.addValidator(new FormRangeValidator(ADC_TO_CURRENT(0), ADC_TO_CURRENT(1023)));

        form.add<uint16_t>(PrintString(F("channel%u_current_limit_time"), i), &blinds->channels[i].currentLimitTime)
            ->setFormUI((new FormUI(FormUI::SELECT, PrintString(F("Channel %u Current Limit Trigger Time"), i)))->addItems(currentLimitItems));
        form.add<uint16_t>(PrintString(F("channel%u_pwm_value"), i), &blinds->channels[i].pwmValue)
            ->setFormUI((new FormUI(FormUI::TEXT, PrintString(F("Channel %u Motor PWM"), i)))->setSuffix(motorSpeed));
        form.addValidator(new FormRangeValidator(0, 1023));

    }

    form.finalize();
}

void BlindsControlPlugin::createWebUI(WebUI &webUI) {

    auto row = &webUI.addRow();
    row->setExtraClass(JJ(title));
    row->addGroup(F("Blinds"), false);

    row = &webUI.addRow();
    row->addBadgeSensor(FSPGM(blinds_controller_channel1_sensor), F("Turn"), JsonString());

    row = &webUI.addRow();
    row->addSwitch(FSPGM(blinds_controller_channel1), F("Channel 1"));

    row = &webUI.addRow();
    row->addBadgeSensor(FSPGM(blinds_controller_channel2_sensor), F("Move"), JsonString());

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

void BlindsControlPlugin::loopMethod() {
    plugin._loopMethod();
}

#endif

#if AT_MODE_SUPPORTED

#include "at_mode.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(BCMS, "BCMS", "<channel=0/1>,<0=closed/1=open>", "Set channel state");
PROGMEM_AT_MODE_HELP_COMMAND_DEF(BCMD, "BCMD", "<0=swap channels/1=channel0/2=channel2>,<0/1>", "Set swap channel/channel 0/1 direction", "Display settings");
PROGMEM_AT_MODE_HELP_COMMAND_DEF(BCMC, "BCMC", "<channel=0/1>,<level=0-1023>,<open-time/ms>,<close-time/ms>,<current-limit=0-1023>,<limit-time/ms>", "Configure channel", "Display settings");

#if IOT_BLINDS_CTRL_TESTMODE

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(BCME, "BCME", "<channel=0/1>,<direction=0/1>,<max-time/ms>,<level=0-1023>,<limit=0-1023>,<limit-time>", "Enable motor # for max-time milliseconds");

void BlindsControlPlugin::_printTestInfo() {

#if IOT_BLINDS_CTRL_RPM_PIN
    MySerial.printf_P(PSTR("%umA %u peak %u rpm %u position %u\n"), ADC_TO_CURRENT(_adcIntegral), _adcIntegral, _peakCurrent, _getRpm(), _rpmCounter);
#else
    MySerial.printf_P(PSTR("%umA %u peak %u\n"), ADC_TO_CURRENT(_adcIntegral), _adcIntegral, _peakCurrent);
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

bool BlindsControlPlugin::hasAtMode() const
{
    return true;
}

void BlindsControlPlugin::atModeHelpGenerator()
{
#if IOT_BLINDS_CTRL_TESTMODE
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(BCME), getName());
#endif
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(BCMS), getName());
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(BCMD), getName());
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(BCMC), getName());
}

bool BlindsControlPlugin::atModeHandler(AtModeArgs &args)
{
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(BCMS))) {
        if (args.requireArgs(2, 2)) {
            uint8_t channel = args.toInt(0) % _channels.size();
            _channels[channel].setState(args.isFalse(1) ? BlindsChannel::CLOSED : BlindsChannel::OPEN);
            _saveState();
            args.printf_P(PSTR("channel %u state %s"), channel, BlindsChannel::_stateStr(_channels[channel].getState()));
        }
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(BCMC))) {
        if (args.isQueryMode() || args.requireArgs(6, 6)) {
            auto &cfg = config._H_W_GET(Config().blinds_controller);
            uint8_t channel = 0xff;
            if (args.size() == 6) {
                channel = args.toInt(0) % 2;
                cfg.channels[channel].pwmValue = (uint16_t)args.toInt(1);
                cfg.channels[channel].openTime = (uint16_t)args.toInt(2);
                cfg.channels[channel].closeTime = (uint16_t)args.toInt(3);
                cfg.channels[channel].currentLimit = (uint16_t)args.toInt(4);
                cfg.channels[channel].currentLimitTime = (uint16_t)args.toInt(5);
                _readConfig();
            }
            for(uint8_t i = 0; i < 2; i++) {
                if (channel == i || channel == 0xff) {
                    args.printf_P(PSTR("channel=%u,level=%u,open=%ums,close=%ums,current limit=%u (%umA)/%ums"),
                        i,
                        cfg.channels[i].pwmValue,
                        cfg.channels[i].openTime,
                        cfg.channels[i].closeTime,
                        cfg.channels[i].currentLimit,
                        ADC_TO_CURRENT(cfg.channels[i].currentLimit),
                        cfg.channels[i].currentLimitTime
                    );
                }
            }
        }
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(BCMD))) {
        if (args.isQueryMode() || args.requireArgs(2, 2)) {
            auto &cfg = config._H_W_GET(Config().blinds_controller);
            if (args.size() == 2) {
                uint8_t item = args.toInt(0) % 3;
                uint8_t value = args.toInt(1) % 2;
                switch(item) {
                    case 0:
                        cfg.swap_channels = value;
                        break;
                    case 1:
                        cfg.channel0_dir = value;
                        break;
                    case 2:
                        cfg.channel1_dir = value;
                        break;
                }
                _readConfig();
            }
            args.printf_P(PSTR("swap channels=%u"), cfg.swap_channels);
            args.printf_P(PSTR("channel 0 direction=%u"), cfg.channel0_dir);
            args.printf_P(PSTR("channel 1 direction=%u"), cfg.channel1_dir);
        }
        return true;
    }
#if IOT_BLINDS_CTRL_TESTMODE
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(BCME))) {
        if (args.requireArgs(6, 6)) {
            uint8_t pins[] = { IOT_BLINDS_CTRL_M1_PIN, IOT_BLINDS_CTRL_M2_PIN, IOT_BLINDS_CTRL_M3_PIN, IOT_BLINDS_CTRL_M4_PIN };
            uint8_t channel = args.toInt(0);
            uint8_t direction = args.toInt(1) % 2;
            uint32_t time = args.toInt(2);
            uint16_t pwmLevel = args.toInt(3);

            auto cfg = config._H_GET(Config().blinds_controller);
            if (cfg.swap_channels) {
                channel++;
            }
            channel %= _channels.size();
            if (channel == 0 && cfg.channel0_dir) {
                direction++;
            }
            if (channel == 1 && cfg.channel1_dir) {
                direction++;
            }
            direction %= 2;

            _currentLimit = args.toInt(4);
            _currentLimitMinCount = args.toInt(5);
            _activeChannel = channel;

            analogWriteFreq(IOT_BLINDS_CTRL_PWM_FREQ);
            for(uint i = 0; i < 4; i++) {
                analogWrite(pins[i], LOW);
            }
            analogWrite(pins[(channel << 1) | direction], pwmLevel);
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
#endif
    return false;
}

#endif
