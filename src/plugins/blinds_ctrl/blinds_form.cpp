/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <KFCForms.h>
#include <kfc_fw_config.h>
#include <PrintHtmlEntitiesString.h>
#include <StringKeyValueStore.h>
#include "blinds_ctrl.h"

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
    auto reverse = F("Reverse (Open/close time is reversed as well)");
    auto motorSpeed = F("0-1023");

    FormUI::ItemsList currentLimitItems(
        5, F("Extra Fast (5ms)"),
        20, F("Fast (20ms)"),
        50, F("Medium (50ms)"),
        150, F("Slow (150ms)"),
        250, F("Extra Slow (250ms)")
    );

    auto &ui = form.getFormUIConfig();
    ui.setTitle(F("Blinds Controller"));
    ui.setContainerId(F("blinds_setttings"));
    ui.setStyle(FormUI::StyleType::ACCORDION);

    auto &commonGroup = form.addCardGroup(FSPGM(config), F("Channel Setup"), true);

    form.addObjectGetterSetter(F("ch0_dir"), cfg, Plugins::Blinds::ConfigStructType::get_bit_channel0_dir, Plugins::Blinds::ConfigStructType::set_bit_channel0_dir);
    form.addFormUI(F("Channel 0 Direction"), FormUI::BoolItems(reverse, FSPGM(Forward)));

    form.addObjectGetterSetter(F("ch1_dir"), cfg, Plugins::Blinds::ConfigStructType::get_bit_channel1_dir, Plugins::Blinds::ConfigStructType::set_bit_channel1_dir);
    form.addFormUI(F("Channel 1 Direction"), FormUI::BoolItems(reverse, FSPGM(Forward)));

    form.addObjectGetterSetter(F("swap_ch"), cfg, Plugins::Blinds::ConfigStructType::get_bit_swap_channels, Plugins::Blinds::ConfigStructType::set_bit_swap_channels);
    form.addFormUI(F("Swap Channels"), FormUI::BoolItems(FSPGM(Yes), FSPGM(No)));

    commonGroup.end();


    // form.add<bool>(F("channel0_dir"), &blinds->channel0_dir)->setFormUI(new FormUI::UI(FormUI::Type::SELECT, F("Channel 0 Direction")))->setBoolItems(reverse, forward));
    // form.add<bool>(F("channel1_dir"), &blinds->channel1_dir)->setFormUI(new FormUI::UI(FormUI::Type::SELECT, F("Channel 1 Direction")))->setBoolItems(reverse, forward));
    // form.add<bool>(F("swap_channels"), &blinds->swap_channels)->setFormUI(new FormUI::UI(FormUI::Type::SELECT, F("Swap Channels")))->setBoolItems(FSPGM(Yes), FSPGM(No)));

    for (uint8_t i = 0; i < kChannelCount; i++) {
        String prefix = PrintString(F("ch%u_"), i);
        String name = PrintString(F("Channel %u"), i);

        auto &channelGroup = form.addCardGroup(prefix + F("grp"), name, true);

        form.addCStringGetterSetter(prefix + F("name"), Plugins::Blinds::getChannel0Name, Plugins::Blinds::setChannel0NameCStr);
        form.addFormUI(F("Name"), FormUI::PlaceHolder(name));
        Plugins::Blinds::addChannel0NameLengthValidator(form, true);

        form.addMemberVariable(prefix + F("ot"), cfg.channels[i], &Plugins::BlindsConfig::BlindsConfigChannel_t::open_time);
        form.addFormUI(F("Open Time Limit"), FormUI::Suffix(FSPGM(ms)));

        form.addMemberVariable(prefix + F("ct"), cfg.channels[i], &Plugins::BlindsConfig::BlindsConfigChannel_t::close_time);
        form.addFormUI(F("Close Time Limit"), FormUI::Suffix(FSPGM(ms)));

        form.add(prefix + F("il"), (uint16_t)(ADC_TO_CURRENT(cfg.channels[i].current_limit)), [&cfg, i](uint16_t value, FormField &, bool store) {
            if (store) {
                cfg.channels[i].current_limit = CURRENT_TO_ADC(value);
            }
            return false;
        });
        form.addFormUI(F("Current Limit"), FormUI::Suffix(FSPGM(mA)));
        form.addValidator(FormRangeValidator(ADC_TO_CURRENT(0), ADC_TO_CURRENT(1023)));

        form.addMemberVariable(prefix + F("ilt"), cfg.channels[i], &Plugins::BlindsConfig::BlindsConfigChannel_t::current_limit_time);
        form.addFormUI(F("Current Limit Trigger Time"), currentLimitItems);

        form.addMemberVariable(prefix + F("pwm"), cfg.channels[i], &Plugins::BlindsConfig::BlindsConfigChannel_t::current_limit_time);
        form.addFormUI(F("Motor PWM"), FormUI::Suffix(motorSpeed));
        form.addValidator(FormRangeValidator(0, 1023));

        channelGroup.end();


        // form.add<uint16_t>(PrintString(F("channel%u_close_time"), i), &blinds->channels[i].closeTime)
        //     ->setFormUI(new FormUI::UI(FormUI::Type::TEXT, PrintString(F("Channel %u Open Time Limit"), i)))->setSuffix(ms));
        // form.add<uint16_t>(PrintString(F("channel%u_open_time"), i), &blinds->channels[i].openTime)
        //     ->setFormUI(new FormUI::UI(FormUI::Type::TEXT, PrintString(F("Channel %u Close Time Limit"), i)))->setSuffix(ms));
        // form.add<uint16_t>(PrintString(F("channel%u_current_limit"), i), &blinds->channels[i].currentLimit, [](uint16_t &value, FormField &field, bool isSetter){
        //         if (isSetter) {
        //             value = CURRENT_TO_ADC(value);
        //         }
        //         else {
        //             value = ADC_TO_CURRENT(value);
        //         }
        //         return true;
        //     })
        //     ->setFormUI(new FormUI::UI(FormUI::Type::TEXT, PrintString(F("Channel %u Current Limit"), i)))->setSuffix(mA));
        // form.addValidator(FormRangeValidator(ADC_TO_CURRENT(0), ADC_TO_CURRENT(1023)));

        // form.add<uint16_t>(PrintString(F("channel%u_current_limit_time"), i), &blinds->channels[i].currentLimitTime)
        //     ->setFormUI(new FormUI::UI(FormUI::Type::SELECT, PrintString(F("Channel %u Current Limit Trigger Time"), i)))->addItems(currentLimitItems));
        // form.add<uint16_t>(PrintString(F("channel%u_pwm_value"), i), &blinds->channels[i].pwmValue)
        //     ->setFormUI(new FormUI::UI(FormUI::Type::TEXT, PrintString(F("Channel %u Motor PWM"), i)))->setSuffix(motorSpeed));
        // form.addValidator(FormRangeValidator(0, 1023));

    }

    form.finalize();

}
