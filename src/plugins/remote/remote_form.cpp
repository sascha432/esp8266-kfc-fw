/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include <kfc_fw_config.h>
#include <Form.h>
#include "Utility/ProgMemHelper.h"
#include "remote_form.h"
#include "remote.h"

using KFCConfigurationClasses::MainConfig;
using KFCConfigurationClasses::Plugins;

static FormUI::Container::List getActions()
{
    return FormUI::Container::List(0, F("None")); //, 100, F("100"), 200, F("200"), 300, F("300"), 400, F("400"));
}

void RemoteControlPlugin::createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request)
{
    if (!isCreateFormCallbackType(type)) {
        return;
    }

    auto &ui = form.createWebUI();
    auto &cfg = Plugins::RemoteControl::getWriteableConfig();
    ui.setStyle(FormUI::WebUI::StyleType::ACCORDION);

    if (String_equals(formName, PSTR("general"))) {

        ui.setTitle(F("Remote Control Configuration"));
        ui.setContainerId(F("remotectrl_general"));

        auto &mainGroup = form.addCardGroup(F("main"), F("General"), true);

        form.addObjectGetterSetter(F("ast"), cfg, cfg.get_bits_auto_sleep_time, cfg.set_bits_auto_sleep_time);
        form.addFormUI(F("Auto Sleep Time"), FormUI::Suffix(F("seconds")));
        cfg.addRangeValidatorFor_auto_sleep_time(form);

        form.addObjectGetterSetter(F("dst"), cfg, cfg.get_bits_deep_sleep_time, cfg.set_bits_deep_sleep_time);
        form.addFormUI(F("Deep Sleep Time"), FormUI::Suffix(F("minutes (0 = indefinitely)")));
        cfg.addRangeValidatorFor_deep_sleep_time(form);

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

        form.addStringGetterSetter(F("udph"), Plugins::RemoteControl::getUdpHost, Plugins::RemoteControl::setUdpHost);
        form.addFormUI(F("Host"));
        form.addValidator(FormUI::Validator::Hostname(FormUI::AllowedType::HOST_OR_IP_OR_EMPTY));

        form.addObjectGetterSetter(F("udpp"), cfg, cfg.get_bits_udp_port, cfg.set_bits_udp_port);
        form.addFormUI(FormUI::Type::NUMBER, F("Port"), FormUI::PlaceHolder(7881));
        form.addValidator(FormUI::Validator::NetworkPort(true));

        udpGroup.end();

    }
    else if (String_equals(formName, PSTR("events"))) {

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

                auto &enabled = form.addObjectGetterSetter(F_VAR(en, i), cfg.events[i], cfg.events[0].get_bits_enabled, cfg.events[0].set_bits_enabled);
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
    else if (String_equals(formName, PSTR("combos"))) {

        ui.setTitle(F("Remote Control Button Combinations"));
        ui.setContainerId(F("remotectrl_combos"));

        FormUI::Container::List buttons(0, F("None"));
        for(uint8_t i = 1; i <= _buttonPins.size(); i++) {
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
    else if (String_equals(formName, PSTR("buttons"))) {

        ui.setTitle(F("Remote Control Buttons"));
        ui.setContainerId(F("remotectrl_buttons"));

        auto actions = getActions();

        PROGMEM_DEF_LOCAL_VARNAMES(_VAR_, IOT_REMOTE_CONTROL_BUTTON_COUNT, grp, dn, up, pe, sc, db, lp, hd, re, udn, uup, upe, usc, udb, ulp, uhd, ure);

        for(uint8_t i = 0; i < _buttonPins.size(); i++) {

            auto &group = form.addCardGroup(F_VAR(grp, i), PrintString(F("Button %u Action"), i + 1),
                cfg.actions[i].hasAction()
            );

            auto downEnabled = FormUI::Conditional<FormUI::DisabledAttribute>(!cfg.events[0].enabled, FormUI::DisabledAttribute());
            auto &udpDown = form.addObjectGetterSetter(F_VAR(udn, i), cfg.actions[i], cfg.actions[0].get_bits_udp_down, cfg.actions[0].set_bits_udp_down);
            form.addFormUI(FormUI::Type::HIDDEN, downEnabled);

            form.addObjectGetterSetter(F_VAR(dn, i), cfg.actions[i], cfg.actions[0].get_bits_down, cfg.actions[0].set_bits_down);
            form.addFormUI(F("Down Event"), actions, FormUI::CheckboxButtonSuffix(udpDown, F("Enable UDP")), downEnabled);

            auto upEnabled = FormUI::Conditional<FormUI::DisabledAttribute>(!cfg.events[1].enabled, FormUI::DisabledAttribute());
            auto &udpUp = form.addObjectGetterSetter(F_VAR(uup, i), cfg.actions[i], cfg.actions[0].get_bits_udp_up, cfg.actions[0].set_bits_udp_up);
            form.addFormUI(FormUI::Type::HIDDEN, upEnabled);

            form.addObjectGetterSetter(F_VAR(up, i), cfg.actions[i], cfg.actions[0].get_bits_up, cfg.actions[0].set_bits_up);
            form.addFormUI(F("Up Event"), actions, FormUI::CheckboxButtonSuffix(udpUp, F("Enable UDP")), upEnabled);

            auto pressEnabled = FormUI::Conditional<FormUI::DisabledAttribute>(!cfg.events[2].enabled, FormUI::DisabledAttribute());
            auto &udpPress = form.addObjectGetterSetter(F_VAR(upe, i), cfg.actions[i], cfg.actions[0].get_bits_udp_press, cfg.actions[0].set_bits_udp_press);
            form.addFormUI(FormUI::Type::HIDDEN, pressEnabled);

            form.addObjectGetterSetter(F_VAR(pe, i), cfg.actions[i], cfg.actions[0].get_bits_press, cfg.actions[0].set_bits_press);
            form.addFormUI(F("Press Event"), actions, FormUI::CheckboxButtonSuffix(udpPress, F("Enable UDP")), pressEnabled);

            auto singleClickEnabled = FormUI::Conditional<FormUI::DisabledAttribute>(!cfg.events[3].enabled, FormUI::DisabledAttribute());
            auto &udpSingleClick = form.addObjectGetterSetter(F_VAR(usc, i), cfg.actions[i], cfg.actions[0].get_bits_udp_single_click, cfg.actions[0].set_bits_udp_single_click);
            form.addFormUI(FormUI::Type::HIDDEN, singleClickEnabled);

            form.addObjectGetterSetter(F_VAR(sc, i), cfg.actions[i], cfg.actions[0].get_bits_single_click, cfg.actions[0].set_bits_single_click);
            form.addFormUI(F("Single-Click Event"), actions, FormUI::CheckboxButtonSuffix(udpSingleClick, F("Enable UDP")), singleClickEnabled);

            auto doubleClickEnabled = FormUI::Conditional<FormUI::DisabledAttribute>(!cfg.events[4].enabled, FormUI::DisabledAttribute());
            auto &udpDoubleClick = form.addObjectGetterSetter(F_VAR(udb, i), cfg.actions[i], cfg.actions[0].get_bits_udp_double_click, cfg.actions[0].set_bits_udp_double_click);
            form.addFormUI(FormUI::Type::HIDDEN, doubleClickEnabled);

            form.addObjectGetterSetter(F_VAR(db, i), cfg.actions[i], cfg.actions[0].get_bits_double_click, cfg.actions[0].set_bits_double_click);
            form.addFormUI(F("Double-Click Event"), actions, FormUI::CheckboxButtonSuffix(udpDoubleClick, F("Enable UDP")), doubleClickEnabled);

            auto longPressEnabled = FormUI::Conditional<FormUI::DisabledAttribute>(!cfg.events[5].enabled, FormUI::DisabledAttribute());
            auto &udpLongPress = form.addObjectGetterSetter(F_VAR(ulp, i), cfg.actions[i], cfg.actions[0].get_bits_udp_long_press, cfg.actions[0].set_bits_udp_long_press);
            form.addFormUI(FormUI::Type::HIDDEN, longPressEnabled);

            form.addObjectGetterSetter(F_VAR(lp, i), cfg.actions[i], cfg.actions[0].get_bits_long_press, cfg.actions[0].set_bits_long_press);
            form.addFormUI(F("Long Press Event"), actions, FormUI::CheckboxButtonSuffix(udpLongPress, F("Enable UDP")), longPressEnabled);

            auto holdEnabled = FormUI::Conditional<FormUI::DisabledAttribute>(!cfg.events[6].enabled, FormUI::DisabledAttribute());
            auto &udpHold = form.addObjectGetterSetter(F_VAR(uhd, i), cfg.actions[i], cfg.actions[0].get_bits_udp_hold, cfg.actions[0].set_bits_udp_hold);
            form.addFormUI(FormUI::Type::HIDDEN, holdEnabled);

            form.addObjectGetterSetter(F_VAR(hd, i), cfg.actions[i], cfg.actions[0].get_bits_hold, cfg.actions[0].set_bits_hold);
            form.addFormUI(F("Hold Event"), actions, FormUI::CheckboxButtonSuffix(udpHold, F("Enable UDP")), holdEnabled);

            auto holdReleaseEnabled = FormUI::Conditional<FormUI::DisabledAttribute>(!cfg.events[7].enabled, FormUI::DisabledAttribute());
            auto &udpHoldRelease = form.addObjectGetterSetter(F_VAR(ure, i), cfg.actions[i], cfg.actions[0].get_bits_udp_hold_released, cfg.actions[0].set_bits_udp_hold_released);
            form.addFormUI(FormUI::Type::HIDDEN, holdReleaseEnabled);

            form.addObjectGetterSetter(F_VAR(re, i), cfg.actions[i], cfg.actions[0].get_bits_hold_released, cfg.actions[0].set_bits_hold_released);
            form.addFormUI(F("Hold-Release Event"), actions, FormUI::CheckboxButtonSuffix(udpHoldRelease, F("Enable UDP")), holdReleaseEnabled);

            group.end();

        }

    }
    else if (String_equals(formName, PSTR("actions"))) {

        ui.setTitle(F("Remote Control Actions (Currently not implemented)"));
        ui.setContainerId(F("remotectrl_actions"));

    }

    form.finalize();
}
