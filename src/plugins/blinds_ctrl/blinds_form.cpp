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

    FormUI::ItemsList operationTypeItems(
        Plugins::Blinds::OperationType::NONE, FSPGM(None),
        Plugins::Blinds::OperationType::OPEN_CHANNEL0, F("Open Channel 0"),
        Plugins::Blinds::OperationType::OPEN_CHANNEL1, F("Open Channel 1"),
        Plugins::Blinds::OperationType::CLOSE_CHANNEL0, F("Close Channel 0"),
        Plugins::Blinds::OperationType::CLOSE_CHANNEL1, F("Close Channel 1"),
        Plugins::Blinds::OperationType::OPEN_CHANNEL0_FOR_CHANNEL1, F("Open Channel 0 For Channel 1"),
        Plugins::Blinds::OperationType::OPEN_CHANNEL1_FOR_CHANNEL0, F("Open Channel 1 For Channel 0"),
        Plugins::Blinds::OperationType::CLOSE_CHANNEL0_FOR_CHANNEL1, F("Close Channel 0 For Channel 1"),
        Plugins::Blinds::OperationType::CLOSE_CHANNEL1_FOR_CHANNEL0, F("Close Channel 1 For Channel 0")
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

        channelGroup.end();

    }

    auto &autoGroup = form.addCardGroup(F("open"), PrintString(F("Open Automation")), false);

    for(size_t i = 0; i < 4; i++) {
        String prefix = PrintString(F("oq%u_"), i);
        form.add(prefix + String('t'), _H_W_STRUCT_VALUE(cfg, open[i].type, i));
        form.addFormUI(F("Action"), operationTypeItems);

        form.add(prefix + String('d'), _H_W_STRUCT_VALUE(cfg, open[i].delay, i));
        if (i == 0) {
            form.addFormUI(FormUI::Label(F("Delay:<br><small>The delay is skipped if the action is not executed</small>"), true), FormUI::Suffix(FSPGM(seconds)));
        }
        else {
            form.addFormUI(F("Delay"), FormUI::Suffix(FSPGM(seconds)));
        }
        form.addValidator(FormRangeValidator(0, 3600));
    }

    auto &closeGroup = autoGroup.end().addCardGroup(F("close"), PrintString(F("Close Automation")), false);

    for(size_t i = 0; i < 4; i++) {
        String prefix = PrintString(F("cq%u_"), i);
        form.add(prefix + String('t'), _H_W_STRUCT_VALUE(cfg, close[i].type, i));
        form.addFormUI(F("Action"), operationTypeItems);

        form.add(prefix + String('d'), _H_W_STRUCT_VALUE(cfg, close[i].delay, i));
        if (i == 0) {
            form.addFormUI(FormUI::Label(F("Delay:<br><small>The delay is skipped if the action is not executed</small>"), true), FormUI::Suffix(FSPGM(seconds)));
        }
        else {
            form.addFormUI(F("Delay"), FormUI::Suffix(FSPGM(seconds)));
        }
        form.addValidator(FormRangeValidator(0, 3600));
    }

    auto &pinsGroup = closeGroup.end().addCardGroup(FSPGM(config), F("Pin Configuration"), false);

    form.add(F("pin0"), _H_W_STRUCT_VALUE_TYPE(cfg, pins[0], uint8_t));
    form.addFormUI(F("Channel 0 Open Pin"), FormUI::Type::INTEGER, FormUI::PlaceHolder(IOT_BLINDS_CTRL_M1_PIN));

    form.add(F("pin1"), _H_W_STRUCT_VALUE_TYPE(cfg, pins[1], uint8_t));
    form.addFormUI(F("Channel 0 Close Pin"), FormUI::Type::INTEGER, FormUI::PlaceHolder(IOT_BLINDS_CTRL_M2_PIN));

    form.add(F("pin2"), _H_W_STRUCT_VALUE_TYPE(cfg, pins[2], uint8_t));
    form.addFormUI(F("Channel 1 Open Pin"), FormUI::Type::INTEGER, FormUI::PlaceHolder(IOT_BLINDS_CTRL_M3_PIN));

    form.add(F("pin3"), _H_W_STRUCT_VALUE_TYPE(cfg, pins[3], uint8_t));
    form.addFormUI(F("Channel 1 Close Pin"), FormUI::Type::INTEGER, FormUI::PlaceHolder(IOT_BLINDS_CTRL_M4_PIN));

    auto &multiplexer = form.addObjectGetterSetter(F("shmp"), cfg, cfg.get_int_multiplexer, cfg.set_int_multiplexer);
    form.addFormUI(FormUI::Type::HIDDEN);

    form.add(F("pin4"), _H_W_STRUCT_VALUE_TYPE(cfg, pins[4], uint8_t));
    form.addFormUI(F("Shunt Multiplexer Pin"), FormUI::Type::INTEGER, FormUI::PlaceHolder(IOT_BLINDS_CTRL_RSSEL_PIN)).addInputGroupAppendCheckBoxButton(multiplexer, F("HIGH State For Channel 0"));

    pinsGroup.end();

    form.finalize();

}
