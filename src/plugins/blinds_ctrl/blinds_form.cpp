/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <KFCForms.h>
#include <kfc_fw_config.h>
#include <PrintHtmlEntitiesString.h>
#include <StringKeyValueStore.h>
#include "blinds_plugin.h"

#if DEBUG_IOT_BLINDS_CTRL
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using KFCConfigurationClasses::System;
using KFCConfigurationClasses::Plugins;

void BlindsControlPlugin::createConfigureForm(FormCallbackType type, const String &formName, Form &form, AsyncWebServerRequest *request)
{
    if (type == FormCallbackType::SAVE) {

        // using KeyValueStorage::Container;
        // using KeyValueStorage::ContainerPtr;
        // using KeyValueStorage::Item;

        // auto &cfg = Plugins::Blinds::getWriteableConfig();

        // auto blinds = config._H_GET(Config().blinds_controller);
        // bool dir[2] = { blinds.channel0_dir, blinds.channel1_dir };
        // auto container = ContainerPtr(new Container());
        // container->add(Item::create(F("blinds_swap_ch"), blinds.swap_channels));
        // for(uint8_t i = 0; i < 2; i++) {
        //     container->add(Item::create(PrintString(F("blinds[%u]dir"), i), dir[i]));
        //     container->add(Item::create(PrintString(F("blinds[%u]close_time"), i), blinds.channels[i].closeTime));
        //     container->add(Item::create(PrintString(F("blinds[%u]I_limit"), i), blinds.channels[i].currentLimit));
        //     container->add(Item::create(PrintString(F("blinds[%u]I_limit_time"), i), blinds.channels[i].currentLimitTime));
        //     container->add(Item::create(PrintString(F("blinds[%u]open_time"), i), blinds.channels[i].openTime));
        //     container->add(Item::create(PrintString(F("blinds[%u]pwm"), i), blinds.channels[i].pwmValue));
        // }
        // config.callPersistantConfig(container);

        return;
    } else if (!isCreateFormCallbackType(type)) {
        return;
    }


    auto &cfg = Plugins::Blinds::getWriteableConfig();

    FormUI::ItemsList currentLimitItems(
        5, F("Extra Fast (5ms)"),
        20, F("Fast (20ms)"),
        50, F("Medium (50ms)"),
        150, F("Slow (150ms)"),
        250, F("Extra Slow (250ms)")
    );

    FormUI::ItemsList stateTypeItems(
        Plugins::Blinds::BlindsStateType::NONE, FSPGM(None),
        Plugins::Blinds::BlindsStateType::OPEN, FSPGM(Open),
        Plugins::Blinds::BlindsStateType::CLOSED, FSPGM(Closed)
    );

    auto &ui = form.getFormUIConfig();
    ui.setTitle(F("Blinds Controller"));
    ui.setContainerId(F("blinds_setttings"));
    ui.setStyle(FormUI::StyleType::ACCORDION);

    for (uint8_t i = 0; i < kChannelCount; i++) {
        String prefix = PrintString(F("ch%u_"), i);
        String name = PrintString(F("Channel %u"), i);

        auto &channelGroup = form.addCardGroup(prefix + F("grp"), name, i == 0);

        switch(i) {
            case 0:
                form.addCStringGetterSetter(prefix + FSPGM(name), Plugins::Blinds::getChannel0Name, Plugins::Blinds::setChannel0NameCStr);
                Plugins::Blinds::addChannel0NameLengthValidator(form);
                break;
            case 1:
                form.addCStringGetterSetter(prefix + FSPGM(name), Plugins::Blinds::getChannel1Name, Plugins::Blinds::setChannel1NameCStr);
                Plugins::Blinds::addChannel1NameLengthValidator(form);
                break;
        }
        form.addFormUI(FSPGM(Name), FormUI::PlaceHolder(name));

        form.add(prefix + F("ot"), _H_W_STRUCT_VALUE(cfg, channels[i].open_time, i));
        form.addFormUI(F("Open Time Limit"), FormUI::Suffix(FSPGM(ms)));

        form.add(prefix + F("ct"), _H_W_STRUCT_VALUE(cfg, channels[i].close_time, i));
        form.addFormUI(F("Close Time Limit"), FormUI::Suffix(FSPGM(ms)));

        form.add(prefix + F("il"), _H_W_STRUCT_VALUE(cfg, channels[i].current_limit, i));
        form.addFormUI(F("Current Limit"), FormUI::Suffix(FSPGM(mA)));
        form.addValidator(FormRangeValidator(BlindsControllerConversion::kMinCurrent, BlindsControllerConversion::kMaxCurrent));

        form.add(prefix + F("ilt"), _H_W_STRUCT_VALUE(cfg, channels[i].current_limit_time, i));
        form.addFormUI(F("Current Limit Trigger Time"), currentLimitItems);

        form.add(prefix + FSPGM(pwm), _H_W_STRUCT_VALUE(cfg, channels[i].pwm_value, i));
        form.addFormUI(F("Motor PWM"), FormUI::Suffix(String(0) + '-' + String(PWMRANGE)));
        form.addValidator(FormRangeValidator(0, PWMRANGE));

        auto &autoCloseGroup = channelGroup.end().addCardGroup(prefix + FSPGM(config), PrintString(F("Automation Channel %u"), i), false);

        form.add(prefix + F("ood"), _H_W_STRUCT_VALUE(cfg, channels[i].operation.open_delay, i));
        form.addFormUI(FormUI::Label(F("Open Delay:<br><small>Delay opening after the other channel has opened</small>"), true), FormUI::Suffix(FSPGM(seconds)));
        form.addValidator(FormRangeValidator(0, 255));

        form.add(prefix + F("ocd"), _H_W_STRUCT_VALUE(cfg, channels[i].operation.close_delay, i));
        form.addFormUI(FormUI::Label(F("Close Delay:<br><small>Delay closing after the other channel has closed</small>"), true), FormUI::Suffix(FSPGM(seconds)));
        form.addValidator(FormRangeValidator(0, 255));

        form.add(prefix + F("oos"), _H_W_STRUCT_VALUE(cfg, channels[i].operation.open_state, i));
        form.addFormUI(FormUI::Label(F("Required Open State:<br><small>Required state of the other channel, when this channel opens</small>"), true), stateTypeItems);

        form.add(prefix + F("ocs"), _H_W_STRUCT_VALUE(cfg, channels[i].operation.close_state, i));
        form.addFormUI(FormUI::Label(F("Required Close State:<br><small>Required state of the other channel, when this channel closes</small>"), true), stateTypeItems);

        autoCloseGroup.end();

    }

    auto &pinsGroup = form.addCardGroup(FSPGM(config), F("Motor Pins"), false);

    form.add(F("pin0"), _H_W_STRUCT_VALUE_TYPE(cfg, pins[0], uint8_t));
    form.addFormUI(F("Channel 0 Pin 1"), FormUI::Type::INTEGER, FormUI::PlaceHolder(IOT_BLINDS_CTRL_M1_PIN));

    form.add(F("pin1"), _H_W_STRUCT_VALUE_TYPE(cfg, pins[1], uint8_t));
    form.addFormUI(F("Channel 0 Pin 2"), FormUI::Type::INTEGER, FormUI::PlaceHolder(IOT_BLINDS_CTRL_M2_PIN));

    form.add(F("pin2"), _H_W_STRUCT_VALUE_TYPE(cfg, pins[2], uint8_t));
    form.addFormUI(F("Channel 1 Pin 1"), FormUI::Type::INTEGER, FormUI::PlaceHolder(IOT_BLINDS_CTRL_M3_PIN));

    form.add(F("pin3"), _H_W_STRUCT_VALUE_TYPE(cfg, pins[3], uint8_t));
    form.addFormUI(F("Channel 1 Pin 2"), FormUI::Type::INTEGER, FormUI::PlaceHolder(IOT_BLINDS_CTRL_M4_PIN));

    pinsGroup.end();

    form.finalize();

}
