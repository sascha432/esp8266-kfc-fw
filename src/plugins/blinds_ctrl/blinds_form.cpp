/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <KFCForms.h>
#include <kfc_fw_config.h>
#include <PrintHtmlEntitiesString.h>
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
    if (!isCreateFormCallbackType(type)) {
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
    ui.setContainerId(F("blinds_settings"));
    ui.setStyle(FormUI::StyleType::ACCORDION);

    if (String_equals(formName, PSTR("channels"))) {

        for (uint8_t i = 0; i < kChannelCount; i++) {
            String prefix = PrintString(F("ch%u_"), i);
            String name = PrintString(F("Channel %u"), i);

            auto &channelGroup = form.addCardGroup(prefix + F("grp"), name, i == 0);

            switch(i) {
                case 0:
                    form.addStringGetterSetter(prefix + FSPGM(name), Plugins::Blinds::getChannel0Name, Plugins::Blinds::setChannel0Name);
                    Plugins::Blinds::addChannel0NameLengthValidator(form);
                    break;
                case 1:
                    form.addStringGetterSetter(prefix + FSPGM(name), Plugins::Blinds::getChannel1Name, Plugins::Blinds::setChannel1Name);
                    Plugins::Blinds::addChannel1NameLengthValidator(form);
                    break;
            }
            form.addFormUI(FSPGM(Name), FormUI::PlaceHolder(name));

            form.addPointer(prefix + F("ot"), &cfg.channels[i].open_time);
            form.addFormUI(F("Open Time Limit"), FormUI::Suffix(FSPGM(ms)));

            form.addPointer(prefix + F("ct"), &cfg.channels[i].close_time);
            form.addFormUI(F("Close Time Limit"), FormUI::Suffix(FSPGM(ms)));

            form.addPointer(prefix + F("il"), &cfg.channels[i].current_limit);
            form.addFormUI(F("Current Limit"), FormUI::Suffix(FSPGM(mA)));
            form.addValidator(FormRangeValidator(BlindsControllerConversion::kMinCurrent, BlindsControllerConversion::kMaxCurrent));

            form.addPointer(prefix + F("ilt"), &cfg.channels[i].current_limit_time);
            form.addFormUI(F("Current Limit Trigger Time"), currentLimitItems);

            form.addPointer(prefix + F("ih"), &cfg.channels[i].dac_current_limit);
            form.addFormUI(F("Hard Current Limit (DRV8870)"), FormUI::Suffix(FSPGM(mA)));
            form.addValidator(FormRangeValidator(BlindsControllerConversion::kMinCurrent, BlindsControllerConversion::kMaxCurrent));

            form.addPointer(prefix + FSPGM(pwm), &cfg.channels[i].pwm_value);
            form.addFormUI(F("Motor PWM"), FormUI::Suffix(String(0) + '-' + String(PWMRANGE)));
            form.addValidator(FormRangeValidator(0, PWMRANGE));

            channelGroup.end();

        }

        auto &autoGroup = form.addCardGroup(FSPGM(open, "open"), PrintString(F("Open Automation")), false);

        for(size_t i = 0; i < sizeof(cfg.open) / sizeof(cfg.open[0]); i++) {
            String prefix = PrintString(F("oq%u_"), i);
            form.addPointer(prefix + String('t'), &cfg.open[i].type);
            form.addFormUI(FSPGM(Action, "Action"), operationTypeItems);

            form.addPointer(prefix + String('d'), &cfg.open[i].delay);
            form.addFormUI(i ? FSPGM(Delay) : F("Delay After Execution"), FormUI::Suffix(FSPGM(seconds)));
            form.addValidator(FormRangeValidator(0, 3600));

        }

        auto &closeGroup = autoGroup.end().addCardGroup(FSPGM(close), PrintString(F("Close Automation")), false);

        for(size_t i = 0; i < sizeof(cfg.close) / sizeof(cfg.close[0]); i++) {
            String prefix = PrintString(F("cq%u_"), i);
            form.addPointer(prefix + String('t'), &cfg.close[i].type);
            form.addFormUI(FSPGM(Action), operationTypeItems);

            form.addPointer(prefix + String('d'), &cfg.close[i].delay);
            form.addFormUI(i ? FSPGM(Delay) : F("Delay After Execution"), FormUI::Suffix(FSPGM(seconds)));
            form.addValidator(FormRangeValidator(0, 3600));
        }

        closeGroup.end();

    }
    else {

        auto &pinsGroup = form.addCardGroup(FSPGM(config), F("Pin Configuration"), false);

        form.addPointer(F("pin0"), &cfg.pins[0]);
        form.addFormUI(F("Channel 0 Open Pin"), FormUI::Type::INTEGER, FormUI::PlaceHolder(IOT_BLINDS_CTRL_M1_PIN));

        form.addPointer(F("pin1"), &cfg.pins[1]);
        form.addFormUI(F("Channel 0 Close Pin"), FormUI::Type::INTEGER, FormUI::PlaceHolder(IOT_BLINDS_CTRL_M2_PIN));

        form.addPointer(F("pin2"), &cfg.pins[2]);
        form.addFormUI(F("Channel 1 Open Pin"), FormUI::Type::INTEGER, FormUI::PlaceHolder(IOT_BLINDS_CTRL_M3_PIN));

        form.addPointer(F("pin3"), &cfg.pins[3]);
        form.addFormUI(F("Channel 1 Close Pin"), FormUI::Type::INTEGER, FormUI::PlaceHolder(IOT_BLINDS_CTRL_M4_PIN));

        auto &multiplexer = form.addObjectGetterSetter(F("shmp"), cfg, cfg.get_int_multiplexer, cfg.set_int_multiplexer);
        form.addFormUI(FormUI::Type::HIDDEN);

        form.addPointer(F("pin4"), &cfg.pins[4]);
        form.addFormUI(F("Shunt Multiplexer Pin"), FormUI::Type::INTEGER, FormUI::PlaceHolder(IOT_BLINDS_CTRL_MULTIPLEXER_PIN), FormUI::UI::createCheckBoxButton(multiplexer, F("HIGH State For Channel 0")));

        form.addPointer(F("pin5"), &cfg.pins[5]);
        form.addFormUI(F("DAC Current Limit Pin"), FormUI::Type::INTEGER, FormUI::PlaceHolder(IOT_BLINDS_CTRL_DAC_PIN));

        pinsGroup.end();

        auto &motorGroup = form.addCardGroup(F("ctrl"), F("Controller Configuration"), true);

        using ConfigType = typename std::remove_reference<decltype(cfg)>::type;

        form.addMemberVariable(F("pwm"), cfg, &ConfigType::pwm_frequency);
        form.addFormUI(F("PWM Frequency"), FormUI::Type::INTEGER, FormUI::PlaceHolder(Plugins::Blinds::ConfigStructType::kPwmFrequencyDefault), FormUI::Suffix(F("Hz")));
        form.addValidator(FormRangeValidator(1000, 40000));

        form.addMemberVariable(F("pss"), cfg, &ConfigType::pwm_softstart_time);
        form.addFormUI(F("PWM Soft Start Ramp-up Time"), FormUI::Type::INTEGER, FormUI::PlaceHolder(Plugins::Blinds::ConfigStructType::kPwmSoftStartTime), FormUI::Suffix(F("microseconds")));
        form.addValidator(FormRangeValidator(100U, 150U * 1000U));

        form.addMemberVariable(F("adca"), cfg, &ConfigType::adc_divider);
        form.addFormUI(F("ADC Averaging"), FormUI::Type::INTEGER, FormUI::PlaceHolder(Plugins::Blinds::ConfigStructType::kAdcDividerDefault), FormUI::Suffix(F("period in milliseconds")));
        form.addValidator(FormRangeValidator(1, 1000));

        form.addMemberVariable(F("adci"), cfg, &ConfigType::adc_read_interval);
        form.addFormUI(F("ADC Read Interval"), FormUI::Type::INTEGER, FormUI::PlaceHolder(Plugins::Blinds::ConfigStructType::kAdcReadIntervalDefault), FormUI::Suffix(F("microseconds")));
        form.addValidator(FormRangeValidator(500, 20000));

        form.addMemberVariable(F("adcrt"), cfg, &ConfigType::adc_recovery_time);
        form.addFormUI(F("ADC Recovery Time"), FormUI::Type::INTEGER, FormUI::PlaceHolder(Plugins::Blinds::ConfigStructType::kAdcRecoveryTimeDefault), FormUI::Suffix(F("microseconds")));
        form.addValidator(FormRangeValidator(2000, 50000));

        form.addMemberVariable(F("adcrr"), cfg, &ConfigType::adc_recoveries_per_second);
        form.addFormUI(F("ADC Repeat Recovery"), FormUI::Type::INTEGER, FormUI::PlaceHolder(Plugins::Blinds::ConfigStructType::kAdcRecoveriesPerSecDefault), FormUI::Suffix(F("per second")));
        form.addValidator(FormRangeValidator(1, 20));

        form.addMemberVariable(F("adco"), cfg, &ConfigType::adc_offset);
        form.addFormUI(F("ADC Offset"), FormUI::Type::INTEGER, FormUI::Suffix(F("raw value")));
        form.addValidator(FormRangeValidator(-1023, 1023));

        motorGroup.end();

    }

    form.finalize();

}
