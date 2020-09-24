/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <KFCForms.h>
#include <kfc_fw_config.h>
#include <PrintHtmlEntitiesString.h>
#include "blinds_plugin.h"
#include "Utility/ProgMemHelper.h"

#if DEBUG_IOT_BLINDS_CTRL
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using KFCConfigurationClasses::System;
using KFCConfigurationClasses::Plugins;

void BlindsControlPlugin::createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request)
{
    if (!isCreateFormCallbackType(type)) {
        return;
    }

    auto &cfg = Plugins::Blinds::getWriteableConfig();
    using ConfigType = Plugins::Blinds::BlindsConfig_t;
    using ChannelConfigType = Plugins::Blinds::BlindsConfigChannel_t;
    using OperationType = Plugins::Blinds::OperationType;

    auto &ui = form.createWebUI();
    ui.setTitle(F("Blinds Controller"));
    ui.setContainerId(F("blinds_settings"));
    ui.setStyle(FormUI::WebUI::StyleType::ACCORDION);

    if (String_equals(formName, PSTR("channels"))) {

        // FormUI::Container::List currentLimitItems(
        //      500, F("Extra Fast (500µs)"),
        //     1250, F("Fast (1250µs)"),
        //     2500, F("Medium (1250µs)"),
        //     5000, F("Slow (5000µs)"),
        //    10000, F("Extra Slow (10000µs)")
        // );

        FormUI::Container::List operationTypeItems(
            OperationType::NONE, FSPGM(None),
            OperationType::OPEN_CHANNEL0, F("Open Channel 0"),
            OperationType::OPEN_CHANNEL1, F("Open Channel 1"),
            OperationType::CLOSE_CHANNEL0, F("Close Channel 0"),
            OperationType::CLOSE_CHANNEL1, F("Close Channel 1"),
            OperationType::OPEN_CHANNEL0_FOR_CHANNEL1, F("Open Channel 0 For Channel 1"),
            OperationType::OPEN_CHANNEL1_FOR_CHANNEL0, F("Open Channel 1 For Channel 0"),
            OperationType::CLOSE_CHANNEL0_FOR_CHANNEL1, F("Close Channel 0 For Channel 1"),
            OperationType::CLOSE_CHANNEL1_FOR_CHANNEL0, F("Close Channel 1 For Channel 0")
        );

        PROGMEM_DEF_LOCAL_VARNAMES(_VAR_, 2, grp, n0, n1, otl, ctl, il, ip, dac, pwm);
        PROGMEM_DEF_LOCAL_VARNAMES(_VAR_, BLINDS_CONFIG_MAX_OPERATIONS, ot, od, ct, cd);

        const __FlashStringHelper *names[2] = { F("Channel 0"), F("Channel 1") };
        for (uint8_t i = 0; i < kChannelCount; i++) {
            auto &channelGroup = form.addCardGroup(F_VAR(grp, i), names[i], i == 0);

            switch(i) {
                case 0:
                    form.addStringGetterSetter(F_VAR(n0, i), Plugins::Blinds::getChannel0Name, Plugins::Blinds::setChannel0Name);
                    Plugins::Blinds::addChannel0NameLengthValidator(form);
                    break;
                case 1:
                    form.addStringGetterSetter(F_VAR(n1, i), Plugins::Blinds::getChannel1Name, Plugins::Blinds::setChannel1Name);
                    Plugins::Blinds::addChannel1NameLengthValidator(form);
                    break;
            }
            form.addFormUI(FSPGM(Name), FormUI::PlaceHolder(names[i]));

            form.addObjectGetterSetter(F_VAR(otl, i), cfg.channels[i], ChannelConfigType::get_bits_open_time_ms, ChannelConfigType::set_bits_open_time_ms);
            form.addFormUI(F("Open Time Limit"), FormUI::Suffix(FSPGM(ms)));
            ChannelConfigType::addRangeValidatorFor_open_time_ms(form);

            form.addObjectGetterSetter(F_VAR(ctl, i), cfg.channels[i], ChannelConfigType::get_bits_close_time_ms, ChannelConfigType::set_bits_close_time_ms);
            form.addFormUI(F("Close Time Limit"), FormUI::Suffix(FSPGM(ms)));
            ChannelConfigType::addRangeValidatorFor_close_time_ms(form);

            form.addObjectGetterSetter(F_VAR(il, i), cfg.channels[i], ChannelConfigType::get_bits_current_limit_mA, ChannelConfigType::set_bits_current_limit_mA);
            form.addFormUI(F("Current Limit"), FormUI::Suffix(FSPGM(mA)), FormUI::IntAttribute(F("data-adc-multiplier"), BlindsControllerConversion::kConvertADCValueToCurrentMulitplier * 0xffff));
            ChannelConfigType::addRangeValidatorFor_current_limit_mA(form);

            form.addObjectGetterSetter(F_VAR(ip, i), cfg.channels[i], ChannelConfigType::get_bits_current_avg_period_us, ChannelConfigType::set_bits_current_avg_period_us);
            form.addFormUI(F("Current Averaging Period"), FormUI::Suffix(F("period in microseconds")));
            ChannelConfigType::addRangeValidatorFor_current_avg_period_us(form);

            form.addObjectGetterSetter(F_VAR(dac, i), cfg.channels[i], ChannelConfigType::get_bits_dac_pwm_value, ChannelConfigType::set_bits_dac_pwm_value);
            form.addFormUI(F("DRV8870 Current Limit / DAC PWM"), FormUI::Suffix(F("0 - " _STRINGIFY(PWMRANGE))), FormUI::IntAttribute(F("data-dac-multiplier"), BlindsControllerConversion::kDACPWMtoVoltage * 0xffff));
            ChannelConfigType::addRangeValidatorFor_dac_pwm_value(form);

            form.addObjectGetterSetter(F_VAR(pwm, i), cfg.channels[i], ChannelConfigType::get_bits_pwm_value, ChannelConfigType::set_bits_pwm_value);
            form.addFormUI(F("Motor PWM"), FormUI::Suffix(F("0 - " _STRINGIFY(PWMRANGE))));
            ChannelConfigType::addRangeValidatorFor_pwm_value(form);

            channelGroup.end();
        }

        auto &autoGroup = form.addCardGroup(FSPGM(open, "open"), F("Open Automation"), false);

        for(size_t i = 0; i < BLINDS_CONFIG_MAX_OPERATIONS; i++) {
            form.addPointerTriviallyCopyable(F_VAR(ot, i), &cfg.open[i].type);
            form.addFormUI(FSPGM(Action, "Action"), operationTypeItems);

            form.addPointerTriviallyCopyable(F_VAR(od, i), &cfg.open[i].delay);
            form.addFormUI(i ? FSPGM(Delay) : F("Delay After Execution"), FormUI::Suffix(FSPGM(seconds)));
            form.addValidator(FormUI::Validator::Range(0, 3600));

        }

        auto &closeGroup = autoGroup.end().addCardGroup(FSPGM(close), F("Close Automation"), false);

        for(size_t i = 0; i < BLINDS_CONFIG_MAX_OPERATIONS; i++) {
            form.addPointerTriviallyCopyable(F_VAR(ct, i), &cfg.close[i].type);
            form.addFormUI(FSPGM(Action), operationTypeItems);

            form.addPointerTriviallyCopyable(F_VAR(cd, i), &cfg.close[i].delay);
            form.addFormUI(i ? FSPGM(Delay) : F("Delay After Execution"), FormUI::Suffix(FSPGM(seconds)));
            form.addValidator(FormUI::Validator::Range(0, 3600));
        }

        closeGroup.end();

    }
    else {

        FormUI::Container::List pins(KFCConfigurationClasses::createFormPinList());

        auto &pinsGroup = form.addCardGroup(FSPGM(config), F("Pin Configuration"), false);

        form.addPointerTriviallyCopyable(F("pin0"), &cfg.pins[0]);
        form.addFormUI(F("Channel 0 Open Pin"), pins);

        form.addPointerTriviallyCopyable(F("pin1"), &cfg.pins[1]);
        form.addFormUI(F("Channel 0 Close Pin"), pins);

        form.addPointerTriviallyCopyable(F("pin2"), &cfg.pins[2]);
        form.addFormUI(F("Channel 1 Open Pin"), pins);

        form.addPointerTriviallyCopyable(F("pin3"), &cfg.pins[3]);
        form.addFormUI(F("Channel 1 Close Pin"), pins);

        form.addObjectGetterSetter(F("shmp"), cfg, cfg.get_bits_adc_multiplexer, cfg.set_bits_adc_multiplexer);
        form.addFormUI(FormUI::Type::HIDDEN);

        form.addPointerTriviallyCopyable(F("pin4"), &cfg.pins[4]);
        form.addFormUI(F("Shunt Multiplexer Pin"), pins, FormUI::SuffixHtml(F("<select data-target=\"#shmp\" data-action=\"transfer-hidden-field\" class=\"input-group-text form-select\"><option value=\"0\">Enable for Channel 0</option><option value=\"1\">Enable for Channel 1</option></select>")));

        form.addPointerTriviallyCopyable(F("pin5"), &cfg.pins[5]);
        form.addFormUI(F("DAC Pin for DRV8870 Vref"), pins);

        pinsGroup.end();

        auto &motorGroup = form.addCardGroup(F("ctrl"), F("Controller Configuration"), true);

        form.addObjectGetterSetter(F("pwm"), cfg, cfg.get_bits_pwm_frequency, cfg.set_bits_pwm_frequency);
        form.addFormUI(F("PWM Frequency"), FormUI::PlaceHolder(ConfigType::kDefaultValueFor_pwm_frequency), FormUI::Suffix(F("Hz")));
        ConfigType::addRangeValidatorFor_pwm_frequency(form);

        form.addObjectGetterSetter(F("pss"), cfg, cfg.get_bits_pwm_softstart_time, cfg.set_bits_pwm_softstart_time);
        form.addFormUI(F("PWM Soft Start Ramp-up Time"), FormUI::PlaceHolder(ConfigType::kDefaultValueFor_pwm_softstart_time), FormUI::Suffix(F("microseconds")));
        ConfigType::addRangeValidatorFor_pwm_softstart_time(form);

        form.addObjectGetterSetter(F("adci"), cfg, cfg.get_bits_adc_read_interval, cfg.set_bits_adc_read_interval);
        form.addFormUI(F("ADC Read Interval"), FormUI::PlaceHolder(ConfigType::kDefaultValueFor_adc_read_interval), FormUI::Suffix(F("microseconds")));
        ConfigType::addRangeValidatorFor_adc_read_interval(form);

        form.addObjectGetterSetter(F("adcrt"), cfg, cfg.get_bits_adc_recovery_time, cfg.set_bits_adc_recovery_time);
        form.addFormUI(F("ADC Recovery Time"), FormUI::PlaceHolder(ConfigType::kDefaultValueFor_adc_recovery_time), FormUI::Suffix(F("microseconds")));
        ConfigType::addRangeValidatorFor_adc_recovery_time(form);

        form.addObjectGetterSetter(F("adcrr"), cfg, cfg.get_bits_adc_recoveries_per_second, cfg.set_bits_adc_recoveries_per_second);
        form.addFormUI(F("ADC Repeat Recovery"), FormUI::PlaceHolder(ConfigType::kDefaultValueFor_adc_recoveries_per_second), FormUI::Suffix(F("per second")));
        ConfigType::addRangeValidatorFor_adc_recoveries_per_second(form);

        form.addObjectGetterSetter(F("adco"), cfg, cfg.get_bits_adc_offset, cfg.set_bits_adc_offset);
        form.addFormUI(F("ADC Offset"), FormUI::Suffix(F("raw value")));
        ConfigType::addRangeValidatorFor_adc_offset(form);

        motorGroup.end();

    }

    form.finalize();

}
