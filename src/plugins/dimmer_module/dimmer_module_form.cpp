/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_ATOMIC_SUN_V2
#include "../src/plugins/atomic_sun/atomic_sun_v2.h"
#else
#include "dimmer_plugin.h"
#endif

#if DEBUG_IOT_DIMMER_MODULE
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

void DimmerModuleForm::_createConfigureForm(PluginComponent::FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request)
{
    if (type == PluginComponent::FormCallbackType::SAVE) {
        writeConfig(Plugins::Dimmer::getWriteableConfig());
        return;
    }
    else if (!PluginComponent::isCreateFormCallbackType(type)) {
        return;
    }

    auto &cfg = Plugins::Dimmer::getWriteableConfig();
    if (type == PluginComponent::FormCallbackType::CREATE_GET) {
        readConfig(cfg);
    }

    auto &ui = form.createWebUI();
    ui.setTitle(F("Dimmer Configuration"));
    ui.setContainerId(F("dimmer_settings"));
    ui.setStyle(FormUI::WebUI::StyleType::ACCORDION);

    auto configValidAttr = FormUI::ConditionalAttribute(cfg.config_valid == false, FSPGM(disabled), FSPGM(disabled));

    auto &mainGroup = form.addCardGroup(FSPGM(config), F("General"), true);

    form.add(F("onft"), _H_W_STRUCT_VALUE(cfg, on_fadetime));
    form.addFormUI(F("On Fade Time"), FormUI::PlaceHolder(7.5, 1), FormUI::Suffix(FSPGM(seconds)));
    form.addValidator(FormUI::Validator::RangeDouble(1.0, 30.0));

    form.add(F("offft"), _H_W_STRUCT_VALUE(cfg, off_fadetime));
    form.addFormUI(F("Off Fade Time"), FormUI::PlaceHolder(7.5, 1), FormUI::Suffix(FSPGM(seconds)));
    form.addValidator(FormUI::Validator::RangeDouble(1.0, 30.0));

    auto &offDelaySignal = form.addObjectGetterSetter(F("offdlys"), cfg, cfg.get_bits_off_delay_signal, cfg.set_bits_off_delay_signal);
    form.addFormUI(FormUI::Type::HIDDEN);

    form.addObjectGetterSetter(F("offdly"), cfg, cfg.get_bits_off_delay, cfg.set_bits_off_delay);
    // PrintHtmlEntitiesString tmp;
    // FormUI::UI::appendGroupText(tmp, FSPGM(seconds));
    // FormUI::UI::appendGroupText(tmp, FormUI::UI::createCheckBoxButton(offDelaySignal, F("Confirm Signal")));
    // , FormUI::StringSuffix(tmp)
    form.addFormUI(F("Off Delay"), FormUI::PlaceHolder(0));
    cfg.addRangeValidatorFor_off_delay(form);

#if DIMMER_FIRMWARE_VERSION < 0x030000
    form.add(F("lcf"), _H_W_STRUCT_VALUE(cfg, fw.linear_correction_factor));
    form.addFormUI(F("Linear Correction Factor"), configValidAttr, FormUI::PlaceHolder(1.0, 1));
#endif

    form.add<bool>(F("restore"), _H_W_STRUCT_VALUE(cfg, fw.bits.restore_level));
    form.addFormUI(F("After Power Failure"), configValidAttr, FormUI::BoolItems(F("Restore last brightness level"), F("Do not turn on")));

    form.add<uint8_t>(F("max_temp"), _H_W_STRUCT_VALUE(cfg, fw.max_temp));
    form.addFormUI(F("Max. Temperature"), configValidAttr, FormUI::PlaceHolder(80), FormUI::Suffix(FSPGM(_degreeC)));
    form.addValidator(FormUI::Validator::Range(F("Temperature out of range: %min%-%max%"), 45, 110));

    form.add<uint8_t>(F("metricsint"), _H_W_STRUCT_VALUE(cfg, fw.report_metrics_max_interval));
    form.addFormUI(F("Metrics Report Interval"), configValidAttr, FormUI::PlaceHolder(10), FormUI::Suffix(FSPGM(seconds)));
    form.addValidator(FormUI::Validator::Range(5, 60));

    mainGroup.end();

#if IOT_ATOMIC_SUN_V2

    auto &channelsGroup = form.addCardGroup(FSPGM(channels), F("Channels"), false);

    form.add<int8_t>(F("channel0"), _H_W_STRUCT_VALUE_TYPE(cfg, channel_mapping[0], int8_t));
    form.addFormUI(F("Channel Warm White 1"), FormUI::PlaceHolder(IOT_ATOMIC_SUN_CHANNEL_WW1));
    form.addValidator(FormUI::Validator::Range(0, 3));

    form.add<int8_t>(F("channel1"), _H_W_STRUCT_VALUE_TYPE(cfg, channel_mapping[1], int8_t));
    form.addFormUI(F("Channel Warm White 2"), FormUI::PlaceHolder(IOT_ATOMIC_SUN_CHANNEL_WW2));
    form.addValidator(FormUI::Validator::Range(0, 3));

    form.add<int8_t>(F("channel2"), _H_W_STRUCT_VALUE_TYPE(cfg, channel_mapping[2], int8_t));
    form.addFormUI(F("Channel Cold White 1"), FormUI::PlaceHolder(IOT_ATOMIC_SUN_CHANNEL_CW1));

    form.addValidator(FormUI::Validator::Range(0, 3));
    form.add<int8_t>(F("channel3"), _H_W_STRUCT_VALUE_TYPE(cfg, channel_mapping[3], int8_t));
    form.addFormUI(F("Channel Cold White 2"), FormUI::PlaceHolder(IOT_ATOMIC_SUN_CHANNEL_CW2));
    form.addValidator(FormUI::Validator::Range(0, 3));

    channelsGroup.end();

#endif

#if IOT_DIMMER_MODULE_HAS_BUTTONS

    auto &buttonGroup = form.addCardGroup(F("btncfg"), F("Button Configuration"), false);

    form.addObjectGetterSetter(F("sptime"), cfg, cfg.get_bits_shortpress_time, cfg.set_bits_shortpress_time);
    form.addFormUI(F("Click Time"), FormUI::Suffix(FSPGM(milliseconds)), FormUI::PlaceHolder(cfg.kDefaultValueFor_shortpress_time));
    cfg.addRangeValidatorFor_longpress_time(form);

    form.addObjectGetterSetter(F("lptime"), cfg, cfg.get_bits_longpress_time, cfg.set_bits_longpress_time);
    form.addFormUI(F("Long Press Time"), FormUI::Suffix(FSPGM(milliseconds)), FormUI::PlaceHolder(cfg.kDefaultValueFor_longpress_time));
    cfg.addRangeValidatorFor_longpress_time(form);

    form.add<float>(F("lpft"), _H_W_STRUCT_VALUE(cfg, lp_fadetime));
    form.addFormUI(F("Button Held Fade Time"), FormUI::Suffix(FSPGM(seconds)), FormUI::PlaceHolder(3.5, 1));
    form.addValidator(FormUI::Validator::RangeDouble(1.0, 30.0));

    form.addObjectGetterSetter(F("sstep"), cfg, cfg.get_bits_shortpress_steps, cfg.set_bits_shortpress_steps);
    form.addFormUI(F("Brightness Steps"), FormUI::Suffix(F("per 100%")), FormUI::PlaceHolder(cfg.kDefaultValueFor_shortpress_steps));
    cfg.addRangeValidatorFor_shortpress_steps(form);

    form.addObjectGetterSetter(F("snrt"), cfg, cfg.get_bits_single_click_time, cfg.set_bits_single_click_time);
    form.addFormUI(F("Double Click Speed:"), FormUI::Suffix(FSPGM(milliseconds)), FormUI::PlaceHolder(cfg.kDefaultValueFor_shortpress_time));
    cfg.addRangeValidatorFor_single_click_time(form);

    form.addObjectGetterSetter(F("maxbr"), cfg, cfg.get_bits_max_brightness, cfg.set_bits_max_brightness);
    form.addFormUI(F("Min. Brightness"), FormUI::Suffix('%'), FormUI::PlaceHolder(cfg.kDefaultValueFor_max_brightness));
    cfg.addRangeValidatorFor_max_brightness(form);

    form.addObjectGetterSetter(F("minbr"), cfg, cfg.get_bits_min_brightness, cfg.set_bits_min_brightness);
    form.addFormUI(F("Min. Brightness"), FormUI::Suffix('%'), FormUI::PlaceHolder(cfg.kDefaultValueFor_min_brightness));
    cfg.addRangeValidatorFor_min_brightness(form);

    form.addObjectGetterSetter(F("lpmaxb"), cfg, cfg.get_bits_longpress_max_brightness, cfg.set_bits_longpress_max_brightness);
    form.addFormUI(F("Long Press Up/Max. Brightness"), FormUI::Suffix('%'), FormUI::PlaceHolder(cfg.kDefaultValueFor_longpress_max_brightness));
    cfg.addRangeValidatorFor_longpress_max_brightness(form);

    form.addObjectGetterSetter(F("lpminb"), cfg, cfg.get_bits_longpress_min_brightness, cfg.set_bits_longpress_min_brightness);
    form.addFormUI(F("Long Press Down/Min. Brightness"), FormUI::Suffix('%'), FormUI::PlaceHolder(cfg.kDefaultValueFor_longpress_min_brightness));
    cfg.addRangeValidatorFor_longpress_min_brightness(form);

    buttonGroup.end();

#endif

    auto &buttonPinGroup = form.addCardGroup(F("btncfg"), F("Button Pin Configuration"), false);

    auto &pin0Inverted = form.addObjectGetterSetter(F("pupi"), cfg, cfg.get_bits_pin_ch0_up_inverted, cfg.set_bits_pin_ch0_up_inverted);
    form.addFormUI(FormUI::Type::HIDDEN);

    form.addObjectGetterSetter(F("pinup"), cfg, cfg.get_bits_pin_ch0_up, cfg.set_bits_pin_ch0_up);
    form.addFormUI(F("Button Up Pin #"), FormUI::UI::createCheckBoxButton(pin0Inverted, F("Active Low")));
    cfg.addRangeValidatorFor_pin_ch0_up(form);

    auto &pin1Inverted = form.addObjectGetterSetter(F("pdbi"), cfg, cfg.get_bits_pin_ch0_down_inverted, cfg.set_bits_pin_ch0_down_inverted);
    form.addFormUI(FormUI::Type::HIDDEN);

    form.addObjectGetterSetter(F("pindn"),  cfg, cfg.get_bits_pin_ch0_down, cfg.set_bits_pin_ch0_down);
    form.addFormUI(F("Button Down Pin #"), FormUI::UI::createCheckBoxButton(pin1Inverted, F("Active Low")));
    cfg.addRangeValidatorFor_pin_ch0_down(form);

    buttonPinGroup.end();

    auto &fwGroup = form.addCardGroup(F("fwcfg"), F("Advanced Firmware Configuration"), false);

    form.add<uint8_t>(F("zc_offset"), _H_W_STRUCT_VALUE(cfg, fw.zero_crossing_delay_ticks));
    form.addFormUI(F("Zero Crossing Offset"), configValidAttr, FormUI::Suffix(FSPGM(ticks, "ticks")));
    form.addValidator(FormUI::Validator::Range(0, 255));

    form.add<uint16_t>(F("min_on"), _H_W_STRUCT_VALUE(cfg, fw.minimum_on_time_ticks));
    form.addFormUI(F("Minimum On-time"), configValidAttr, FormUI::Suffix(FSPGM(ticks)));
    form.addValidator(FormUI::Validator::Range(1, 65535));

    form.add<uint16_t>(F("min_off"), _H_W_STRUCT_VALUE(cfg, fw.adjust_halfwave_time_ticks));
    form.addFormUI(F("Minimum Off-time"), configValidAttr, FormUI::Suffix(FSPGM(ticks)));
    form.addValidator(FormUI::Validator::Range(1, 65535));

    form.add<float>(F("vref11"), _H_W_STRUCT_VALUE(cfg, fw.internal_1_1v_ref));
    form.addFormUI(F("ATmega 1.1V Reference Calibration"), configValidAttr, FormUI::PlaceHolder(1.1, 1), FormUI::Suffix('V'));
    form.addValidator(FormUI::Validator::RangeDouble(0.9, 1.3, 1));

    form.add<float>(F("temp_ofs"), (cfg.fw.ntc_temp_offset / DIMMER_TEMP_OFFSET_DIVIDER), [&cfg](const float &value, FormField &, bool) {
        cfg.fw.ntc_temp_offset = value * DIMMER_TEMP_OFFSET_DIVIDER;
        return false;
    });
    form.addFormUI(F("Temperature Offset (NTC)"), configValidAttr, FormUI::PlaceHolder(0), FormUI::Suffix(FSPGM(_degreeC)));

    form.add<float>(F("temp2_ofs"), (cfg.fw.int_temp_offset / DIMMER_TEMP_OFFSET_DIVIDER), [&cfg](const float &value, FormField &, bool) {
        cfg.fw.int_temp_offset = value * DIMMER_TEMP_OFFSET_DIVIDER;
        return false;
    });
    form.addFormUI(F("Temperature Offset 2 (ATmega)"), configValidAttr, FormUI::PlaceHolder(0), FormUI::Suffix(FSPGM(_degreeC)));

    fwGroup.end();

    form.finalize();
}
