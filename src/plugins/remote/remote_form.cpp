 /**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include <kfc_fw_config.h>
#include <Form.h>
#include "Utility/ProgMemHelper.h"
#include "remote_form.h"
#include "remote.h"

#if DEBUG_IOT_REMOTE_CONTROL
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using KFCConfigurationClasses::MainConfig;
using Plugins = KFCConfigurationClasses::PluginsType;
using EventNameType = Plugins::RemoteControl::EventNameType;

static FormUI::Container::List getActions()
{
    return FormUI::Container::List(0, F("None")); //, 100, F("100"), 200, F("200"), 300, F("300"), 400, F("400"));
}

void RemoteControlPlugin::createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request)
{
    // if (type == FormCallbackType::SAVE) {
    //     return;
    // }
    if (!isCreateFormCallbackType(type)) {
        return;
    }

    auto &ui = form.createWebUI();
    auto &cfg = Plugins::RemoteControl::getWriteableConfig();
    ui.setStyle(FormUI::WebUI::StyleType::ACCORDION);

    if (formName == F("general")) {

        ui.setTitle(F("Remote Control Configuration"));
        ui.setContainerId(F("remotectrl_general"));

        auto &mainGroup = form.addCardGroup(F("main"), F("General"), true);

        form.addObjectGetterSetter(F("ast"), cfg, cfg.get_bits_auto_sleep_time, cfg.set_bits_auto_sleep_time);
        form.addFormUI(F("Auto Sleep Time"), FormUI::Suffix(F("seconds")));
        cfg.addRangeValidatorFor_auto_sleep_time(form);

        form.addObjectGetterSetter(F("mwt"), cfg, cfg.get_bits_max_awake_time, cfg.set_bits_max_awake_time);
        form.addFormUI(F("Max. Time Awake On Batteries"), FormUI::Suffix(F("minutes")));
        cfg.addRangeValidatorFor_max_awake_time(form);

        form.addObjectGetterSetter(F("dst"), cfg, cfg.get_bits_deep_sleep_time, cfg.set_bits_deep_sleep_time);
        form.addFormUI(F("Deep Sleep Time"), FormUI::Suffix(F("minutes")), FormUI::IntAttribute(F("disabled-value"), 0), FormUI::FPStringAttribute(F("disabled-value-placeholder"), F("Indefinitely")));
        cfg.addRangeValidatorFor_deep_sleep_time(form, true);

        form.addObjectGetterSetter(F("spt"), cfg, cfg.get_bits_click_time, cfg.set_bits_click_time);
        form.addFormUI(F("Short Press/Click Time"), FormUI::Suffix(F("milliseconds")));
        cfg.addRangeValidatorFor_click_time(form);

        form.addObjectGetterSetter(F("lpt"), cfg, cfg.get_bits_hold_time, cfg.set_bits_hold_time);
        form.addFormUI(F("Long Press/Hold Time"), FormUI::Suffix(F("milliseconds")));
        cfg.addRangeValidatorFor_hold_time(form);

        form.addObjectGetterSetter(F("rt"), cfg, cfg.get_bits_hold_repeat_time, cfg.set_bits_hold_repeat_time);
        form.addFormUI(F("Hold Repeat Time"), FormUI::Suffix(F("milliseconds")));
        cfg.addRangeValidatorFor_hold_repeat_time(form);

        mainGroup.end();
        auto &udpGroup = form.addCardGroup(F("udp"), F("UDP Packets"), true);

        form.addObjectGetterSetter(F("udpe"), cfg, cfg.get_bits_udp_enable, cfg.set_bits_udp_enable);
        form.addFormUI(F("UDP Support"), FormUI::BoolItems());

        form.addStringGetterSetter(F("udph"), Plugins::RemoteControl::getUdpHost, Plugins::RemoteControl::setUdpHost);
        form.addFormUI(F("Host"));
        form.addValidator(FormUI::Validator::Hostname(FormUI::AllowedType::EMPTY_OR_HOST_OR_IP));

        form.addObjectGetterSetter(F("udpp"), cfg, cfg.get_bits_udp_port, cfg.set_bits_udp_port);
        form.addFormUI(FormUI::Type::NUMBER, F("Port"), FormUI::PlaceHolder(7881));
        form.addValidator(FormUI::Validator::NetworkPort(true));

        udpGroup.end();

        auto &mqttGroup = form.addCardGroup(F("mqtt"), F("MQTT"), true);

        form.addObjectGetterSetter(F("mqtte"), cfg, cfg.get_bits_mqtt_enable, cfg.set_bits_mqtt_enable);
        form.addFormUI(F("MQTT Device Triggers"), FormUI::BoolItems());

        mqttGroup.end();

    }
    else if (formName == F("events")) {

        ui.setTitle(F("Remote Control Events"));
        ui.setContainerId(F("remotectrl_events"));

        {
            PROGMEM_DEF_LOCAL_VARNAMES(_VAR_, IOT_REMOTE_CONTROL_BUTTON_COUNT, bn);

            auto &nameGroup = form.addCardGroup(F("bn"), F("Button Names"), true);

            for(uint8_t i = 0; i < Plugins::RemoteControl::kButtonCount; i++) {

                form.addCallbackGetterSetter<String>(F_VAR(bn, i), [i](String &str, Field::BaseField &, bool store) {
                    if (store) {
                        Plugins::RemoteControl::setName(i, str.c_str());
                    } else {
                        str = Plugins::RemoteControl::getName(i);
                    }
                    return true;
                }, InputFieldType::TEXT);
                form.addFormUI(FormUI::Label(PrintString(F("Button #%u"), i + 1)));
                Plugins::RemoteControl::addName1LengthValidator(form); // length is the same for all

            }

            nameGroup.end();
        }
        {
            auto &mGroup = form.addCardGroup(F("smm"), F("System Maintenance Mode"), true);

            form.add(F("sm1"), String(F("Button #1")));
            form.addFormUI(F("Combination Button 1"), FormUI::ReadOnlyAttribute());

            form.add(F("sm2"), String(F("Button #4")));
            form.addFormUI(F("Combination Button 2"), FormUI::ReadOnlyAttribute());

            mGroup.end();
        }
        {
            auto &eventGroup = form.addCardGroup(F("en"), F("Event Names"), true);

            PROGMEM_DEF_LOCAL_VARNAMES(_VAR_, 9, en, na);

            const __FlashStringHelper *eventNames[] = {
                F("Button Down"),
                F("Button Up"),
                F("Button Press"),
                F("Button Long Press"),
                F("Button Single-Click"),
                F("Button Double-Click"),
                F("Button Multi-Click"),
                F("Button Hold"),
                F("Button Hold-Release")
            };

            for(uint8_t i = 0; i < Plugins::RemoteControl::kEventCount; i++) {

                auto &enabled = form.addCallbackGetterSetter<bool>(F_VAR(en, i), [&cfg, i](bool &value, Field::BaseField &field, bool store) {
                    if (store) {
                        if (value) {
                            cfg.enabled.event_bits |= _BV(i);
                        }
                        else {
                            cfg.enabled.event_bits &= ~_BV(i);
                        }
                    }
                    else {
                        value = cfg.enabled[i];
                    }
                    return true;
                });
                form.addFormUI(FormUI::Type::HIDDEN);

                form.addCallbackGetterSetter<String>(F_VAR(na, i), [i](String &str, Field::BaseField &, bool store) {
                    if (store) {
                        Plugins::RemoteControl::setEventName(i, str.c_str());
                    } else {
                        str = Plugins::RemoteControl::getEventName(i);
                    }
                    return true;
                }, InputFieldType::TEXT);
                form.addFormUI(eventNames[i], FormUI::CheckboxButtonSuffix(enabled, F("Enabled")));
                Plugins::RemoteControl::addEvent1LengthValidator(form); // length is the same for all

            }

            eventGroup.end();

        }

    }
    else if (formName == F("combos")) {

        ui.setTitle(F("Remote Control Button Combinations"));
        ui.setContainerId(F("remotectrl_combos"));

        FormUI::Container::List buttons(0, F("None"));
        for(uint8_t i = 1; i <= kButtonPins.size(); i++) {
            buttons.emplace_back(i, PrintString(F("Button %u"), i));
        }

        // 0
        uint8_t i = 0;
        auto comboGroup = &form.addCardGroup(F("cmb0"), F("Reset Device"), true);

        form.addObjectGetterSetter(F("rst"), cfg.combo[i], cfg.combo[0].get_bits_time, cfg.combo[0].set_bits_time);
        form.addFormUI(F("Press Time"), FormUI::Suffix(F("milliseconds")));

        form.addObjectGetterSetter(F("rst0"), cfg.combo[i], cfg.combo[0].get_bits_btn0, cfg.combo[0].set_bits_btn0);
        form.addFormUI(F("First Button"), buttons);

        form.addObjectGetterSetter(F("rst1"), cfg.combo[i++], cfg.combo[0].get_bits_btn1, cfg.combo[0].set_bits_btn1);
        form.addFormUI(F("Second Button"), buttons);

        // 1
        comboGroup = &comboGroup->end().addCardGroup(F("cmb1"), F("Disable Auto Sleep for 300 seconds"), false);

        form.addObjectGetterSetter(F("das"), cfg.combo[i], cfg.combo[0].get_bits_time, cfg.combo[0].set_bits_time);
        form.addFormUI(F("Press Time"));

        form.addObjectGetterSetter(F("das0"), cfg.combo[i], cfg.combo[0].get_bits_btn0, cfg.combo[0].set_bits_btn0);
        form.addFormUI(F("First Button"), buttons);

        form.addObjectGetterSetter(F("das1"), cfg.combo[i++], cfg.combo[0].get_bits_btn1, cfg.combo[0].set_bits_btn1);
        form.addFormUI(F("Second Button"), buttons);

        // 2
        comboGroup = &comboGroup->end().addCardGroup(F("cmb2"), F("Enter Deep Sleep"), false);

        form.addObjectGetterSetter(F("dsp"), cfg.combo[i], cfg.combo[0].get_bits_time, cfg.combo[0].set_bits_time);
        form.addFormUI(F("Press Time"));

        form.addObjectGetterSetter(F("dsp0"), cfg.combo[i], cfg.combo[0].get_bits_btn0, cfg.combo[0].set_bits_btn0);
        form.addFormUI(F("First Button"), buttons);

        form.addObjectGetterSetter(F("dsp1"), cfg.combo[i++], cfg.combo[0].get_bits_btn1, cfg.combo[0].set_bits_btn1);
        form.addFormUI(F("Second Button"), buttons);

        // 3
        comboGroup = &comboGroup->end().addCardGroup(F("cmb3"), F("Broadcast Battery Statistics"), false);

        form.addObjectGetterSetter(F("bat"), cfg.combo[i], cfg.combo[0].get_bits_time, cfg.combo[0].set_bits_time);
        form.addFormUI(F("Press Time"));

        form.addObjectGetterSetter(F("bat0"), cfg.combo[i], cfg.combo[0].get_bits_btn0, cfg.combo[0].set_bits_btn0);
        form.addFormUI(F("First Button"), buttons);

        form.addObjectGetterSetter(F("bat1"), cfg.combo[i++], cfg.combo[0].get_bits_btn1, cfg.combo[0].set_bits_btn1);
        form.addFormUI(F("Second Button"), buttons);

        // 4
        comboGroup = &comboGroup->end().addCardGroup(F("cmb4"), F("Restore Factory Defaults"), false);

        form.addObjectGetterSetter(F("rfs"), cfg.combo[i], cfg.combo[0].get_bits_time, cfg.combo[0].set_bits_time);
        form.addFormUI(F("Press Time"));

        form.addObjectGetterSetter(F("rfs0"), cfg.combo[i], cfg.combo[0].get_bits_btn0, cfg.combo[0].set_bits_btn0);
        form.addFormUI(F("First Button"), buttons);

        form.addObjectGetterSetter(F("rfs1"), cfg.combo[i++], cfg.combo[0].get_bits_btn1, cfg.combo[0].set_bits_btn1);
        form.addFormUI(F("Second Button"), buttons);

        comboGroup->end();

    }
    else if (formName.startsWith(F("buttons"))) {

        auto start = formName.c_str() + 7;
        if (*start == '-') {
            start++;
        }
        uint8_t i = atoi(start) - 1;
        if (i >= kButtonPins.size()) {
            i = 0;
        }

        ui.setTitle(F("Remote Control Buttons"));
        ui.setContainerId(F("remotectrl_buttons"));

        auto actions = getActions();

        #define CUSTOM_CHECKBOX_SUFFIX_HIDDEN_FIELDS(var, var_suffix, var_name, event_nametype, label) {\
            auto conditional = FormUI::Conditional<FormUI::DisabledAttribute>(!cfg.enabled[event_nametype], FormUI::DisabledAttribute()); \
            auto &udp##var_name = form.addObjectGetterSetter(F("u" var), cfg.actions[i].udp, cfg.actions[0].udp.get_bits_event_##var_suffix, cfg.actions[0].udp.set_bits_event_##var_suffix); \
            form.addFormUI(FormUI::Type::HIDDEN, conditional); \
            auto &mqtt##var_name = form.addObjectGetterSetter(F("m" var), cfg.actions[i].mqtt, cfg.actions[0].mqtt.get_bits_event_##var_suffix, cfg.actions[0].mqtt.set_bits_event_##var_suffix); \
            form.addFormUI(FormUI::Type::HIDDEN, conditional); \
            form.addObjectGetterSetter(F(var), cfg.actions[i], cfg.actions[0].get_bits_##var_suffix, cfg.actions[0].set_bits_##var_suffix); \
            form.addFormUI(label, actions, FormUI::CheckboxButtonSuffix(udp##var_name, FSPGM(Enable_UDP)), FormUI::CheckboxButtonSuffix(mqtt##var_name, FSPGM(Enable_MQTT)), conditional); \
        }

        auto &group = form.addCardGroup(F("grp"), PrintString(F("Button %u Action"), i + 1), true);

        CUSTOM_CHECKBOX_SUFFIX_HIDDEN_FIELDS("e0", down, Down, EventNameType::BUTTON_DOWN, F("Down Event"));
        CUSTOM_CHECKBOX_SUFFIX_HIDDEN_FIELDS("e1", up, Up, EventNameType::BUTTON_UP, F("Up Event"));
        CUSTOM_CHECKBOX_SUFFIX_HIDDEN_FIELDS("e2", press, Press, EventNameType::BUTTON_PRESS, F("Press Event"));
        CUSTOM_CHECKBOX_SUFFIX_HIDDEN_FIELDS("e3", single_click, SingleClick, EventNameType::BUTTON_SINGLE_CLICK, F("Single-Click Event"));
        CUSTOM_CHECKBOX_SUFFIX_HIDDEN_FIELDS("e4", double_click, DoubleClick, EventNameType::BUTTON_DOUBLE_CLICK, F("Double-Click Event"));
        CUSTOM_CHECKBOX_SUFFIX_HIDDEN_FIELDS("e5", multi_click, MultiClick, EventNameType::BUTTON_MULTI_CLICK, F("Multi-Click Event"));
        CUSTOM_CHECKBOX_SUFFIX_HIDDEN_FIELDS("e6", long_press, LongPress, EventNameType::BUTTON_LONG_PRESS, F("Long Press Event"));
        CUSTOM_CHECKBOX_SUFFIX_HIDDEN_FIELDS("e7", hold, Hold, EventNameType::BUTTON_HOLD_REPEAT, F("Hold Event"));
        CUSTOM_CHECKBOX_SUFFIX_HIDDEN_FIELDS("e8", hold_released, HoldRelease, EventNameType::BUTTON_HOLD_RELEASE, F("Hold-Release Event"));

        group.end();

    }
    else if (formName == F("actions")) {

        ui.setTitle(F("Remote Control Actions (Currently not implemented)"));
        ui.setContainerId(F("remotectrl_actions"));

    }

    form.finalize();
}
