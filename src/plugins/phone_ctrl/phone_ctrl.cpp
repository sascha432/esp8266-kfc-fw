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
#include <WebUISocket.h>
#include "pin_monitor.h"
#include "../src/plugins/mqtt/component.h"
#include "../src/plugins/mqtt/mqtt_client.h"

#if DEBUG_PHONE_CTRL_MONITOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

// ------------------------------------------------------------------------
// class PhoneCtrlPlugin

class PhoneCtrlPlugin : public PluginComponent, public MQTTComponent {
public:
    PhoneCtrlPlugin();

    virtual void setup(SetupModeType mode, const PluginComponents::DependenciesPtr &dependencies) override;
    virtual void reconfigure(const String &source) override;
    virtual void shutdown() override;
    virtual void getStatus(Print &output) override;

    virtual void createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request) override;

// WebUI
public:
    virtual void createWebUI(WebUINS::Root &webUI) override;
    virtual void getValues(WebUINS::Events &array) override;
    virtual void setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState) override;

// MQTTComponent
public:
    virtual AutoDiscovery::EntityPtr getAutoDiscovery(FormatType format, uint8_t num) override;
    virtual uint8_t getAutoDiscoveryCount() const override;
    virtual void onConnect() override;
    virtual void onMessage(const char *topic, const char *payload, size_t len) override;

public:
    enum class ButtonType : uint8_t {
        RING = 0,
        ACTION,
        MAX
    };

    class ButtonConfig : public PinMonitor::PushButtonConfig
    {
    public:
        using PushButtonConfig::PushButtonConfig;

        ButtonConfig(uint8_t button) : PushButtonConfig(EventType::ALL, 250, 600, 250) {}
    };

    class Button : public PinMonitor::PushButton {
    public:
        Button() :
            PushButton(),
            _button(0)
        {
        }
        Button(uint8_t pin, ButtonType button, PhoneCtrlPlugin &clock) :
            Button(pin, static_cast<uint8_t>(button), clock)
        {
        }
        Button(uint8_t pin, uint8_t button, PhoneCtrlPlugin &clock) :
            PushButton(pin, &clock, std::move(ButtonConfig(button))),
            _button(button)
        {
        }

        ButtonType getButtonType() const
        {
            return ButtonType(getButtonNum());
        }

        uint8_t getButtonNum() const
        {
            return _button;
        }

        virtual void event(EventType state, uint32_t now) override;

        PhoneCtrlPlugin &getBase() const
        {
            return *reinterpret_cast<PhoneCtrlPlugin *>(const_cast<void *>(getArg()));
        }

    protected:
        uint8_t _button;
    };


private:
    void _resetButtons();
    void _publishState();
    void _doAnswer();

private:
    Event::Timer _timer;
    uint32_t _timerState;
    bool _allowAnswering;

    static constexpr uint8_t kPinRing = 5;
    static constexpr bool kPinRingActiveLow = false;
    static constexpr uint8_t kPinAnswer = 12;
    static constexpr uint8_t kPinDial6 = 13;
    static constexpr uint8_t kPinHangup = 14;
    static constexpr uint8_t kPinButton = 0;
    static constexpr bool kPinButtonActiveLow = true;
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
    true,               // has_web_ui
    false,              // has_web_templates
    false,              // has_at_mode
    0                   // __reserved
);

PhoneCtrlPlugin::PhoneCtrlPlugin() :
    PluginComponent(PROGMEM_GET_PLUGIN_OPTIONS(PhoneCtrlPlugin)),
    MQTTComponent(ComponentType::SWITCH),
    _allowAnswering(false)
{
    REGISTER_PLUGIN(this, "PhoneCtrlPlugin");
}

void PhoneCtrlPlugin::setup(SetupModeType mode, const PluginComponents::DependenciesPtr &dependencies)
{
    // defaults
    _allowAnswering = true;

    // output
    _resetButtons();
    pinMode(kPinAnswer, OUTPUT);
    pinMode(kPinDial6, OUTPUT);
    pinMode(kPinHangup, OUTPUT);

    // input
    PinMonitor::pinMonitor.attachPinType<Button>(PinMonitor::HardwarePinType::SIMPLE, kPinRing, ButtonType::RING, *this).setInverted(kPinRingActiveLow);
    PinMonitor::pinMonitor.attach<Button>(kPinButton, ButtonType::ACTION, *this).setInverted(kPinButtonActiveLow);
    if (PinMonitor::pinMonitor.getPins().size()) {
        __LDBG_printf("pins attached=%u", PinMonitor::pinMonitor.getPins().size());
        PinMonitor::pinMonitor.begin();
    }

    // mqtt
    MQTT::Client::registerComponent(this);

    // update states when wifi has been connected
    WiFiCallbacks::add(WiFiCallbacks::EventType::CONNECTED, [this](WiFiCallbacks::EventType event, void *payload) {
        _publishState();
    }, this);

    // update states when wifi is already connected
    if (WiFi.isConnected()) {
        _publishState();
    }
}

void PhoneCtrlPlugin::reconfigure(const String &source)
{
    _publishState();
}

void PhoneCtrlPlugin::shutdown()
{
    _allowAnswering = false;
    PinMonitor::pinMonitor.end();
    _resetButtons();
    _Timer(_timer).remove();
    WiFiCallbacks::remove(WiFiCallbacks::EventType::CONNECTED, this);
    MQTT::Client::unregisterComponent(this);
}

void PhoneCtrlPlugin::getStatus(Print &output)
{
    output.print(F("Phone Controller - Answering automatically "));
    if (_allowAnswering) {
        output.println(F("enabled"));
    }
    else {
        output.println(F("disabled"));
    }
    if (_timer) {
        output.println(F(" (Intercom is active...)"));
    }
}

void PhoneCtrlPlugin::createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request)
{
}

MQTT::AutoDiscovery::EntityPtr PhoneCtrlPlugin::getAutoDiscovery(MQTT::FormatType format, uint8_t num)
{
    auto discovery = new MQTT::AutoDiscovery::Entity();
    auto baseTopic = MQTT::Client::getBaseTopicPrefix();
    switch(static_cast<int>(num)) {
        case 0:
            if (discovery->create(this, F("allow"), format)) {
                discovery->addStateTopic(MQTT::Client::formatTopic(F("/allow/state")));
                discovery->addCommandTopic(MQTT::Client::formatTopic(F("/allow/set")));
                discovery->addName(F("Automatically answer"));
                discovery->addObjectId(baseTopic + F("allow"));
            }
            break;
        case 1:
            if (discovery->create(ComponentType::BUTTON, F("answer"), format)) {
                discovery->addCommandTopic(MQTT::Client::formatTopic(F("/answer")));
                discovery->addName(F("Answer doorbell"));
                discovery->addObjectId(baseTopic + F("answer"));
            }
            break;
        default:
            break;
    }
    return discovery;
}

uint8_t PhoneCtrlPlugin::getAutoDiscoveryCount() const
{
    return 2;
}

void PhoneCtrlPlugin::onConnect()
{
    subscribe(MQTT::Client::formatTopic(F("/allow/set")));
    subscribe(MQTT::Client::formatTopic(F("/answer")));
}

void PhoneCtrlPlugin::onMessage(const char *topic, const char *payload, size_t len)
{
    __LDBG_printf("topic=%s payload=%s(%u)", topic, payload, MQTT::Client::toBool(payload, false));
    if (!strcmp_end_P(topic, PSTR("/allow/set"))) {
        _allowAnswering = MQTT::Client::toBool(payload, false);
        _publishState();
    }
    else if (!strcmp_end_P(topic, PSTR("/answer")) && !strcasecmp_P(payload, PSTR("press"))) {
        _doAnswer();
    }
}

void PhoneCtrlPlugin::createWebUI(WebUINS::Root &webUI)
{
    webUI.addRow(WebUINS::Row(WebUINS::Group(F("Phone Controller"), false)));

    WebUINS::Row row;
    row.append(WebUINS::Switch(F("allow"), F("Automatically answer"), true, WebUINS::NamePositionType::TOP, 4));
    row.append(WebUINS::Switch(F("answer"), F("Answer doorbell"), true, WebUINS::NamePositionType::TOP, 4));
    webUI.addRow(row);
}

void PhoneCtrlPlugin::getValues(WebUINS::Events &array)
{
    array.append(WebUINS::Values(F("allow"), static_cast<uint32_t>(_allowAnswering), true));
    array.append(WebUINS::Values(F("answer"), static_cast<uint32_t>(!!_timer), true));
}

void PhoneCtrlPlugin::setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState)
{
    __LDBG_printf("id=%s value=%s hasValue=%u state=%u hasState=%u", id.c_str(), value.c_str(), hasValue, state, hasState);
    if (hasValue) {
        int val = value.toInt();
        if (id == F("allow")) {
            _allowAnswering = val;
            _publishState();
        }
        else if (id == F("answer")) {
            if (val) {
                _doAnswer();
            }
        }
    }
}

void PhoneCtrlPlugin::_publishState()
{
    if (isConnected()) {
        using namespace MQTT::Json;
        publish(MQTT::Client::formatTopic(F("/allow/state")), true, MQTT::Client::toBoolOnOff(_allowAnswering));
    }
    if (WebUISocket::hasAuthenticatedClients()) {
        WebUINS::Events events;
        getValues(events);
        WebUISocket::broadcast(WebUISocket::getSender(), WebUINS::UpdateEvents(events));
    }
}

void PhoneCtrlPlugin::_doAnswer()
{
    __LDBG_printf("ANSWER: exec (timer=%u, allow=%u)", !!_timer, _allowAnswering);

    if (!_allowAnswering) {
        Logger_security(F("Answering Doorbell disabled"));
        return;
    }

    // stop timer and reset everything
    Logger_security(F("Answering Doorbell"));
    _timer.remove();
    _resetButtons();
    _timerState = 0;

    // update state once the timer is running
    LoopFunctions::callOnce([this]() {
        _publishState();
    });

    // run timer every 100ms
    _Timer(_timer).add(100, true, [this](Event::CallbackTimerPtr timer) {
        switch(_timerState++) {
            // 1000ms
            case 10:
                __LDBG_printf("ANSWER: btn answer");
                digitalWrite(kPinAnswer, HIGH);
                break;
            case 11:
                digitalWrite(kPinAnswer, LOW);
                break;
            // 2000ms
            case 20:
                __LDBG_printf("ANSWER: btn 6");
                digitalWrite(kPinDial6, HIGH);
                break;
            case 21:
                digitalWrite(kPinDial6, LOW);
                break;
            // 3000ms
            case 30:
                __LDBG_printf("ANSWER: btn 6");
                digitalWrite(kPinDial6, HIGH);
                break;
            case 31:
                digitalWrite(kPinDial6, LOW);
                break;
            // 4000ms
            case 40:
                __LDBG_printf("ANSWER: btn hangup #1");
                digitalWrite(kPinHangup, HIGH);
                break;
            case 41:
                digitalWrite(kPinHangup, LOW);
                break;
            // 5000ms
            case 50:
                __LDBG_printf("ANSWER: btn hangup #2");
                digitalWrite(kPinHangup, HIGH);
                break;
            case 51:
                digitalWrite(kPinHangup, LOW);
                break;
            // 6000ms
            case 60:
                __LDBG_printf("ANSWER: end");
                timer->disarm();
                // update state once the timer is done
                LoopFunctions::callOnce([this]() {
                    _publishState();
                });
                break;
        }
    });
}

void PhoneCtrlPlugin::_resetButtons()
{
    digitalWrite(kPinAnswer, LOW);
    digitalWrite(kPinDial6, LOW);
    digitalWrite(kPinHangup, LOW);
}

// virtual event handler callback for the buttons
void PhoneCtrlPlugin::Button::event(EventType eventType, uint32_t now)
{
    switch(static_cast<ButtonType>(_button)) {
        case ButtonType::RING:
            if (eventType == EventType::DOWN && !getBase()._timer) {
                getBase()._doAnswer();
            }
            break;
        case ButtonType::ACTION:
            __LDBG_printf("event_type=%s (%02x) repeat=%u button#=%u now=%u", eventTypeToString(eventType), eventType, _repeatCount, _button, now);
            switch(eventType) {
                case EventType::PRESSED:
                    if (!getBase()._timer) {
                        getBase()._doAnswer();
                    }
                    break;
                case EventType::HOLD_RELEASE:
                    getBase()._allowAnswering = !getBase()._allowAnswering;
                    getBase()._publishState();
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
}
