/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include <kfc_fw_config.h>
#include <PrintHtmlEntitiesString.h>
#include <LoopFunctions.h>
#include <WiFiCallbacks.h>
#include <EventScheduler.h>
#include <KFCForms.h>
#include <KFCJson.h>
#include <Buffer.h>
#include "phone_ctrl.h"
#include "logger.h"
#include "web_server.h"
#include "plugins.h"
#include "plugins_menu.h"
#include "Utility/ProgMemHelper.h"
#include <stl_ext/algorithm.h>

#if DEBUG_PHONE_CTRL_MONITOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

// ------------------------------------------------------------------------
// class PhoneCtrlPlugin

class PhoneCtrlPlugin : public PluginComponent {
public:
    PhoneCtrlPlugin();

    virtual void setup(SetupModeType mode, const PluginComponents::DependenciesPtr &dependencies) override;
    virtual void reconfigure(const String &source) override;
    virtual void shutdown() override;
    virtual void getStatus(Print &output) override;

    virtual void createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request) override;

private:
    void _loop();
    void _resetButtons();

private:
    Event::Timer _timer;
    uint32_t _timerState;
    bool _allowAnswering;

    static constexpr uint8_t kPinRing = 5;
    static constexpr uint8_t kPinAnswer = 12;
    static constexpr uint8_t kPinDial6 = 13;
    static constexpr uint8_t kPinHangup = 14;
    static constexpr uint8_t kPinButton = 255;
    static constexpr uint8_t kPinButtonActiveLevel = LOW;
};

static PhoneCtrlPlugin plugin;

PROGMEM_DEFINE_PLUGIN_OPTIONS(
    PhoneCtrlPlugin,
    "phonectrl",        // name
    "Phone Controller", // friendly name
    "",                 // web_templates
    "phone-ctrl",       // config_forms
    "",                 // reconfigure_dependencies
    PluginComponent::PriorityType::PHONE_CTRL,
    PluginComponent::RTCMemoryId::NONE,
    static_cast<uint8_t>(PluginComponent::MenuType::AUTO),
    false,              // allow_safe_mode
    false,              // setup_after_deep_sleep
    true,               // has_get_status
    true,               // has_config_forms
    false,              // has_web_ui
    false,              // has_web_templates
    false,              // has_at_mode
    0                   // __reserved
);

PhoneCtrlPlugin::PhoneCtrlPlugin() :
    PluginComponent(PROGMEM_GET_PLUGIN_OPTIONS(PhoneCtrlPlugin)),
    _allowAnswering(false)
{
    REGISTER_PLUGIN(this, "PhoneCtrlPlugin");
}

void PhoneCtrlPlugin::setup(SetupModeType mode, const PluginComponents::DependenciesPtr &dependencies)
{
    pinMode(kPinRing, INPUT);
    pinMode(kPinButton, INPUT);
    _resetButtons();
    pinMode(kPinAnswer, OUTPUT);
    pinMode(kPinDial6, OUTPUT);
    pinMode(kPinHangup, OUTPUT);

    LoopFunctions::add([this]() {
        _loop();
    }, this);
    _allowAnswering = true;
}

void PhoneCtrlPlugin::reconfigure(const String &source)
{
}

void PhoneCtrlPlugin::shutdown()
{
    _allowAnswering = false;
    _resetButtons();
    LoopFunctions::remove(this);
    _Timer(_timer).remove();
}

void PhoneCtrlPlugin::getStatus(Print &output)
{
    output.print(F("Phone Controller - Answering "));
    if (_allowAnswering) {
        output.println(F("enabled"));
    }
    else {
        output.println(F("disabled"));
    }
}

void PhoneCtrlPlugin::createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request)
{
}

void PhoneCtrlPlugin::_resetButtons()
{
    digitalWrite(kPinAnswer, LOW);
    digitalWrite(kPinDial6, LOW);
    digitalWrite(kPinHangup, LOW);
}

void PhoneCtrlPlugin::_loop()
{
    bool doAnswer = _allowAnswering && digitalRead(kPinRing);

    if (kPinButton != 255 && digitalRead(kPinButton) == kPinButtonActiveLevel) {
        static constexpr int kDelayMillis = 5;
        static constexpr int kLongPressCount = 500 / kDelayMillis;
        // debounce
        int i = 0;
        delay(50);
        while(digitalRead(kPinButton) == kPinButtonActiveLevel) {
            delay(kDelayMillis);
            if (++i > kLongPressCount) {
                break;
            }
        }
        if (i > kLongPressCount) { // long press >500ms toggles answer mode
            _allowAnswering = !_allowAnswering;
            __LDBG_printf("answer=%u", _allowAnswering);
        }
        else  if (i <= kLongPressCount) { // short press forces answer
            __LDBG_printf("ANSWER: forced by button (timer=%u)", !!_timer);
            doAnswer = true;
            _timer.remove();
            _resetButtons();
            delay(100);
        }
        // wait for release
        while(digitalRead(kPinButton) != kPinButtonActiveLevel) {
            delay(5);
        }
    }

    if (doAnswer) {
        if (!_timer) {
            __LDBG_printf("ANSWER: running");
            Logger_security(F("Answering Doorbell"));
            // reset state
            _timerState = 0;
            // run timer every 100ms
            _Timer(_timer).add(100, true, [this](Event::CallbackTimerPtr timer) {
                // 20000ms
                if (_timerState > 200)  {
                    __LDBG_printf("ANSWER: done");
                    timer->disarm();
                    return;
                }
                switch(_timerState++) {
                    // 1000ms
                    case 10:
                        __LDBG_printf("BUTTON: answer");
                        digitalWrite(kPinAnswer, HIGH);
                        break;
                    case 11:
                        digitalWrite(kPinAnswer, LOW);
                        break;
                    // 2000ms
                    case 20:
                        __LDBG_printf("BUTTON: 6");
                        digitalWrite(kPinDial6, HIGH);
                        break;
                    case 21:
                        digitalWrite(kPinDial6, LOW);
                        break;
                    // 3000ms
                    case 30:
                        __LDBG_printf("BUTTON: 6");
                        digitalWrite(kPinDial6, HIGH);
                        break;
                    case 31:
                        digitalWrite(kPinDial6, LOW);
                        break;
                    // 4000ms
                    case 40:
                        __LDBG_printf("BUTTON: hangup #1");
                        digitalWrite(kPinHangup, HIGH);
                        break;
                    case 41:
                        digitalWrite(kPinHangup, LOW);
                        break;
                    // 5000ms
                    case 50:
                        __LDBG_printf("BUTTON: hangup #2");
                        digitalWrite(kPinHangup, HIGH);
                        break;
                    case 51:
                        digitalWrite(kPinHangup, LOW);
                        break;
                }
            });
        }
    }
}
