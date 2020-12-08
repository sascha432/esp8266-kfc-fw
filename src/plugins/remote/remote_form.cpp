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

        form.addObjectGetterSetter(F("ast"), cfg, cfg.get_bits_autoSleepTime, cfg.set_bits_autoSleepTime);
        form.addFormUI(F("Auto Sleep Time"), FormUI::Suffix(F("seconds")));
        cfg.addRangeValidatorFor_autoSleepTime(form);

        form.addObjectGetterSetter(F("dst"), cfg, cfg.get_bits_deepSleepTime, cfg.set_bits_deepSleepTime);
        form.addFormUI(F("Deep Sleep Time"), FormUI::Suffix(F("minutes (0 = indefinitely)")));
        cfg.addRangeValidatorFor_deepSleepTime(form);

        form.addObjectGetterSetter(F("spt"), cfg, cfg.get_bits_shortpressTime, cfg.set_bits_shortpressTime);
        form.addFormUI(F("Short Press Time"), FormUI::Suffix(F("milliseconds")));
        cfg.addRangeValidatorFor_shortpressTime(form);

        form.addObjectGetterSetter(F("lpt"), cfg, cfg.get_bits_longpressTime, cfg.set_bits_longpressTime);
        form.addFormUI(F("Long Press Time"), FormUI::Suffix(F("milliseconds")));
        cfg.addRangeValidatorFor_longpressTime(form);

        form.addObjectGetterSetter(F("rt"), cfg, cfg.get_bits_repeatTime, cfg.set_bits_repeatTime);
        form.addFormUI(F("Repeat Time"), FormUI::Suffix(F("milliseconds")));
        cfg.addRangeValidatorFor_repeatTime(form);

        mainGroup.end();
        auto &udpGroup = form.addCardGroup(F("udp"), F("UDP Packets"), true);

        form.addStringGetterSetter(F("udph"), Plugins::RemoteControl::getUdpHost, Plugins::RemoteControl::setUdpHost);
        form.addFormUI(F("Host"));
        form.addValidator(FormUI::Validator::Hostname(FormUI::AllowedType::HOST_OR_IP_OR_EMPTY));

        form.addObjectGetterSetter(F("udpp"), cfg, cfg.get_bits_udpPort, cfg.set_bits_udpPort);
        form.addFormUI(FormUI::Type::NUMBER, F("Port"), FormUI::PlaceHolder(7881));
        form.addValidator(FormUI::Validator::NetworkPort(true));

        using UdpActionType = Plugins::RemoteControl::UdpActionType;

        FormUI::Container::List actions(
            UdpActionType::NONE, F("Disabled"),
            UdpActionType::INCLUDE_UP_DOWN, F("Including *-down, *-up actions"),
            UdpActionType::EXCLUDE_UP_DOWN, F("*-press, *-hold, *-long-press only")
        );

        form.addObjectGetterSetter(F("udpa"), cfg, cfg.get_int_udpAction, cfg.set_int_udpAction);
        form.addFormUI(F("Action"), actions);

        udpGroup.end();

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

        FormUI::Container::List actions(0, F("None")
        , 100, F("100"), 200, F("200"), 300, F("300"), 400, F("400")
        );

#if HOME_ASSISTANT_INTEGRATION

        Plugins::HomeAssistant::ActionVector vector;
        Plugins::HomeAssistant::getActions(vector);

        for(const auto &action: vector) {
            auto str = action.getEntityId();
            str += F(": ");
            str += action.getActionFStr();
            //auto str = PrintString(F("%s: %s"), action.getEntityId().c_str(), action.getActionFStr());
            if (action.getNumValues()) {
                // str.printf_P(PSTR(" %d"), action.getValue(0));
                str += ' ';
                str += String(action.getValue(0));
            }
            actions.emplace_back(action.getId(), str);
        }

#endif

        PROGMEM_DEF_LOCAL_VARNAMES(_VAR_, IOT_REMOTE_CONTROL_BUTTON_COUNT, grp, mgrp, sc, db, lp, hd, pe, re, mc0, mc1, mc2, mc3);

        for(uint8_t i = 0; i < _buttonPins.size(); i++) {

            auto &group = form.addCardGroup(F_VAR(grp, i), PrintString(F("Button %u Action"), i + 1),
                cfg.actions[i].hasAction()
            );

            form.addObjectGetterSetter(F_VAR(pe, i), cfg.actions[i], cfg.actions[0].get_bits_pressed, cfg.actions[0].set_bits_pressed);
            form.addFormUI(F("Pressed Event"), actions);

            form.addObjectGetterSetter(F_VAR(sc, i), cfg.actions[i], cfg.actions[0].get_bits_single_click, cfg.actions[0].set_bits_single_click);
            form.addFormUI(F("Single Click Event"), actions);

            form.addObjectGetterSetter(F_VAR(db, i), cfg.actions[i], cfg.actions[0].get_bits_double_click, cfg.actions[0].set_bits_double_click);
            form.addFormUI(F("Double Click Event"), actions);

            form.addObjectGetterSetter(F_VAR(lp, i), cfg.actions[i], cfg.actions[0].get_bits_longpress, cfg.actions[0].set_bits_longpress);
            form.addFormUI(F("Long Press Event"), actions);

            form.addObjectGetterSetter(F_VAR(hd, i), cfg.actions[i], cfg.actions[0].get_bits_held, cfg.actions[0].set_bits_held);
            form.addFormUI(F("Held Event"), actions);

            form.addObjectGetterSetter(F_VAR(re, i), cfg.actions[i], cfg.actions[0].get_bits_pressed, cfg.actions[0].set_bits_pressed);
            form.addFormUI(F("Released Event"), actions);

            group.end();

            auto &multiGroup = form.addCardGroup(F_VAR(mgrp, i), PrintString(F("Button %u Multi Click Action"), i + 1),
                cfg.actions[i].hasMultiClick()
            );

            form.addObjectGetterSetter(F_VAR(mc0, i), cfg.actions[i].multi_click[0], cfg.actions[0].multi_click[0].get_bits_action, cfg.actions[0].multi_click[0].set_bits_action);
            form.addFormUI(F("Multi Click Event"), actions);

            form.addObjectGetterSetter(F_VAR(mc0, i), cfg.actions[i].multi_click[0], cfg.actions[0].multi_click[0].get_bits_clicks, cfg.actions[0].multi_click[0].set_bits_clicks);
            form.addFormUI(F("Number Of Clicks"), actions);
            cfg.actions[0].multi_click[0].addRangeValidatorFor_clicks(form, true);


            form.addObjectGetterSetter(F_VAR(mc1, i), cfg.actions[i].multi_click[1], cfg.actions[0].multi_click[0].get_bits_action, cfg.actions[0].multi_click[0].set_bits_action);
            form.addFormUI(F("Multi Click Event"), actions);

            form.addObjectGetterSetter(F_VAR(mc1, i), cfg.actions[i].multi_click[1], cfg.actions[0].multi_click[0].get_bits_clicks, cfg.actions[0].multi_click[0].set_bits_clicks);
            form.addFormUI(F("Number Of Clicks"), actions);
            cfg.actions[0].multi_click[0].addRangeValidatorFor_clicks(form, true);


            form.addObjectGetterSetter(F_VAR(mc2, i), cfg.actions[i].multi_click[2], cfg.actions[0].multi_click[0].get_bits_action, cfg.actions[0].multi_click[0].set_bits_action);
            form.addFormUI(F("Multi Click Event"), actions);

            form.addObjectGetterSetter(F_VAR(mc2, i), cfg.actions[i].multi_click[2], cfg.actions[0].multi_click[0].get_bits_clicks, cfg.actions[0].multi_click[0].set_bits_clicks);
            form.addFormUI(F("Number Of Clicks"), actions);
            cfg.actions[0].multi_click[0].addRangeValidatorFor_clicks(form, true);


            form.addObjectGetterSetter(F_VAR(mc3, i), cfg.actions[i].multi_click[3], cfg.actions[0].multi_click[0].get_bits_action, cfg.actions[0].multi_click[0].set_bits_action);
            form.addFormUI(F("Multi Click Event"), actions);

            form.addObjectGetterSetter(F_VAR(mc3, i), cfg.actions[i].multi_click[3], cfg.actions[0].multi_click[0].get_bits_clicks, cfg.actions[0].multi_click[0].set_bits_clicks);
            form.addFormUI(F("Number Of Clicks"), actions);
            cfg.actions[0].multi_click[0].addRangeValidatorFor_clicks(form, true);


            multiGroup.end();

        }

    }
    else if (String_equals(formName, PSTR("actions"))) {

        ui.setTitle(F("Remote Control Actions"));
        ui.setContainerId(F("remotectrl_actions"));

    }

    form.finalize();
}
