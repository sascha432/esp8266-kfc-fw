/**
 * Author: sascha_lammers@gmx.de
 */

#include "dimmer_base.h"
#include "dimmer_channel.h"
#include "Utility/ProgMemHelper.h"
#include <stl_ext/array.h>

#if DEBUG_IOT_DIMMER_MODULE
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

#pragma GCC diagnostic ignored "-Waddress-of-packed-member"

using Plugins = KFCConfigurationClasses::PluginsType;

namespace Dimmer {

    void Base::createConfigureForm(PluginComponent::FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request)
    {
        if (type == PluginComponent::FormCallbackType::SAVE) {
            if (formName == F("channels")) {
                auto &cfg = Plugins::Dimmer::getWriteableConfig();
                for(size_t i = 0; i < kNumChannels; i++) {
                    if (cfg.level.from[i] > cfg.level.to[i]) {
                        cfg.level.from[i] = cfg.level.to[i];
                    }
                }
            }
            else if (formName == F("advanced") || formName == F("general")) {
                LoopFunctions::callOnce([this]() {
                    writeConfig(_config);
                });
            }
            return;
        }

        if (!PluginComponent::isCreateFormCallbackType(type)) {
            return;
        }

        auto &cfg = Plugins::Dimmer::getWriteableConfig();
        if (type == PluginComponent::FormCallbackType::CREATE_GET) {
            if (formName == F("channels")) {
                for(size_t i = 0; i < kNumChannels; i++) {
                    if (cfg.level.from[i] > cfg.level.to[i]) {
                        cfg.level.from[i] = cfg.level.to[i];
                    }
                }
            }
            else if (formName == F("advanced") || formName == F("general")) {
                if (!_config._version) {
                    auto str = PrintString(F("../dimmer-fw?type=read-config&redirect=dimmer/%s.html"), formName.c_str());
                    auto response = request->beginResponse(302);
                    HttpHeaders httpHeaders(false);
                    httpHeaders.addNoCache();
                    httpHeaders.add<HttpLocationHeader>(str);
                    httpHeaders.replace<HttpConnectionHeader>(HttpConnectionHeader::ConnectionType::CLOSE);
                    httpHeaders.setResponseHeaders(response);
                    WebServer::Plugin::send(request, response);
                    return;
                }
            }
        }
        auto &firmwareConfig = _config._firmwareConfig;
        auto configValidAttr = FormUI::Conditional<FormUI::DisabledAttribute>(_config == false, FormUI::DisabledAttribute());

        auto &ui = form.createWebUI();
        ui.setTitle(F("Dimmer Configuration"));
        ui.setContainerId(F("dimmer_settings"));
        ui.setStyle(FormUI::WebUI::StyleType::ACCORDION);

        if (formName == F("general")) {

            auto &mainGroup = form.addCardGroup(FSPGM(config), F("General"), true);

            form.addObjectGetterSetter(F("onft"), FormGetterSetter(cfg, on_fadetime));
            form.addFormUI(F("On Fade Time"), FormUI::Suffix(FSPGM(seconds)));
            cfg.addRangeValidatorFor_on_fadetime(form);

            form.addObjectGetterSetter(F("offft"), FormGetterSetter(cfg, off_fadetime));
            form.addFormUI(F("Off Fade Time"), FormUI::Suffix(FSPGM(seconds)));
            cfg.addRangeValidatorFor_off_fadetime(form);

            form.addCallbackSetter(F("restore"), firmwareConfig.bits.restore_level, [&firmwareConfig](const uint8_t value, FormField &) {
                firmwareConfig.bits.restore_level = value;
            });
            form.addFormUI(F("After Power Failure"), configValidAttr, FormUI::BoolItems(F("Restore last brightness level"), F("Do not turn on")));

            form.addPointerTriviallyCopyable(F("maxtmp"), &firmwareConfig.max_temp);
            form.addFormUI(F("Max. Temperature"), configValidAttr, FormUI::PlaceHolder(75), FormUI::Suffix(FSPGM(UTF8_degreeC)));
            form.addValidator(FormUI::Validator::Range(F("Temperature out of range: %min%-%max%"), 55, 125));

            form.addPointerTriviallyCopyable(F("meint"), &firmwareConfig.report_metrics_interval);
            form.addFormUI(F("Metrics Report Interval"), configValidAttr, FormUI::PlaceHolder(10), FormUI::Suffix(FSPGM(seconds)));
            form.addValidator(FormUI::Validator::Range(2, 60));

            mainGroup.end();

            #if IOT_ATOMIC_SUN_V2

                auto &channelsGroup = form.addCardGroup(FSPGM(channels), F("Channels"), false);

                form.addPointerTriviallyCopyable(F("ch0"), &cfg.channel_mapping[0]);
                form.addFormUI(F("Channel Warm White 1"), FormUI::PlaceHolder(IOT_ATOMIC_SUN_CHANNEL_WW1));
                form.addValidator(FormUI::Validator::Range(0, 3));

                form.addPointerTriviallyCopyable(F("ch1"), &cfg.channel_mapping[1]);
                form.addFormUI(F("Channel Warm White 2"), FormUI::PlaceHolder(IOT_ATOMIC_SUN_CHANNEL_WW2));
                form.addValidator(FormUI::Validator::Range(0, 3));

                form.addPointerTriviallyCopyable(F("ch2"), &cfg.channel_mapping[2]);
                form.addFormUI(F("Channel Cold White 1"), FormUI::PlaceHolder(IOT_ATOMIC_SUN_CHANNEL_CW1));
                form.addValidator(FormUI::Validator::Range(0, 3));

                form.addPointerTriviallyCopyable(F("ch3"), &cfg.channel_mapping[3]);
                form.addFormUI(F("Channel Cold White 2"), FormUI::PlaceHolder(IOT_ATOMIC_SUN_CHANNEL_CW2));
                form.addValidator(FormUI::Validator::Range(0, 3));

                channelsGroup.end();

            #endif

        }
        else if (formName == F("channels")) {

            auto &channelGroup = form.addCardGroup(F("chcfg"), F("Channel Configuration"), true);

            #if !IOT_DIMMER_MODULE_HAS_BUTTONS

                form.addObjectGetterSetter(F("minbr"), FormGetterSetter(cfg, min_brightness));
                form.addFormUI(F("Min. Brightness"), FormUI::Suffix(F("%")));
                cfg.addRangeValidatorFor_min_brightness(form);

                form.addObjectGetterSetter(F("maxbr"), FormGetterSetter(cfg, max_brightness));
                form.addFormUI(F("Max. Brightness"), FormUI::Suffix(F("%")));
                cfg.addRangeValidatorFor_max_brightness(form);

            #endif

            #if IOT_ATOMIC_SUN_V2
                PROGMEM_DEF_LOCAL_VARNAMES(_VAR_, IOT_DIMMER_MODULE_CHANNELS, cr, co);
            #else
                PROGMEM_DEF_LOCAL_VARNAMES(_VAR_, IOT_DIMMER_MODULE_CHANNELS, cn, cr, co);
            #endif

            for(size_t i = 0; i < kNumChannels; i++) {

                #if IOT_ATOMIC_SUN_V2
                    String nameStr = getChannelName(i);
                    nameStr += F(" - ");
                    auto name = nameStr.c_str();
                #else
                    form.addCallbackGetterSetter<String>(F_VAR(cn, i), [cfg, i](String &name, FormField &, bool store) {
                        if (store) {
                            KFCConfigurationClasses::Plugins::DimmerConfigNS::Dimmer::setChannelName(i, name);
                        }
                        else {
                            name = KFCConfigurationClasses::Plugins::DimmerConfigNS::Dimmer::getChannelName(i);
                        }
                        return true;
                    });
                    form.addFormUI(FormUI::Label(PrintString(F("Channel %u Name"), i + 1)));
                    KFCConfigurationClasses::Plugins::DimmerConfigNS::Dimmer::addChannel1NameLengthValidator(form, true);

                    auto name = PSTR("Channel");
                #endif

                form.addPointerTriviallyCopyable(F_VAR(cr, i), &cfg.level.from[i]);
                form.addFormUI(FormUI::Label(PrintString(F("%s %u Minimum Level Limit"), name, i + 1)));
                form.addValidator(FormUI::Validator::Range(0, kMaxLevelsChannel - ((kMaxLevelsChannel / 100 + 3))));

                form.addPointerTriviallyCopyable(F_VAR(co, i), &cfg.level.to[i]);
                form.addFormUI(FormUI::Label(PrintString(F("%s %u Maximum Level Limit"), name, i + 1)));
                form.addValidator(FormUI::Validator::Range((kMaxLevelsChannel / 100 + 3), kMaxLevelsChannel));

                // form.addValidator(FormUI::Validator::Callback([i, &vars](const String &to, FormUI::Field::BaseField &field) -> bool {
                //     auto from = field.getForm().getField(vars[i][0])->getValue().toInt();
                //     return (from < to.toInt());
                // }));
            }

            channelGroup.end();

        }
        #if IOT_DIMMER_MODULE_HAS_BUTTONS
            else if (formName == F("buttons")) {

                auto &buttonGroup = form.addCardGroup(F("btncfg"), F("Button Configuration"), false);

                form.addObjectGetterSetter(F("sptime"), FormGetterSetter(cfg, shortpress_time));
                form.addFormUI(F("Click Time"), FormUI::Suffix(FSPGM(milliseconds)));
                cfg.addRangeValidatorFor_longpress_time(form);

                form.addObjectGetterSetter(F("lptime"), FormGetterSetter(cfg, longpress_time));
                form.addFormUI(F("Long Press Time"), FormUI::Suffix(FSPGM(milliseconds)), FormUI::IntAttribute(F("disabled-value"), 0));
                cfg.addRangeValidatorFor_longpress_time(form, true);

                form.addObjectGetterSetter(F("lpft"), FormGetterSetter(cfg, lp_fadetime));
                form.addFormUI(F("Button Hold Fade Time"), FormUI::Suffix(FSPGM(seconds)));
                cfg.addRangeValidatorFor_lp_fadetime(form);

                form.addObjectGetterSetter(F("sstep"), FormGetterSetter(cfg, shortpress_steps));
                form.addFormUI(F("Brightness Steps"), FormUI::Suffix(F("per 100%")));
                cfg.addRangeValidatorFor_shortpress_steps(form);

                form.addObjectGetterSetter(F("snrt"), FormGetterSetter(cfg, single_click_time));
                form.addFormUI(F("Double Click Speed:"), FormUI::Suffix(FSPGM(milliseconds)));
                form.setInvalidMissing(false);
                cfg.addRangeValidatorFor_single_click_time(form);

                form.addObjectGetterSetter(F("minbr"), FormGetterSetter(cfg, min_brightness));
                form.addFormUI(F("Min. Brightness"), FormUI::Suffix(F("%")));
                cfg.addRangeValidatorFor_min_brightness(form);

                form.addObjectGetterSetter(F("maxbr"), FormGetterSetter(cfg, max_brightness));
                form.addFormUI(F("Max. Brightness"), FormUI::Suffix(F("%")));
                cfg.addRangeValidatorFor_max_brightness(form);

                form.addObjectGetterSetter(F("lpmaxb"), FormGetterSetter(cfg, longpress_max_brightness));
                form.addFormUI(F("Long Press Up/Max. Brightness"), FormUI::Suffix(F("%")), FormUI::IntAttribute(F("disabled-value"), 0));
                cfg.addRangeValidatorFor_longpress_max_brightness(form, true);

                form.addObjectGetterSetter(F("lpminb"), FormGetterSetter(cfg, longpress_min_brightness));
                form.addFormUI(F("Long Press Down/Min. Brightness"), FormUI::Suffix(F("%")), FormUI::IntAttribute(F("disabled-value"), 0));
                cfg.addRangeValidatorFor_longpress_min_brightness(form, true);

                buttonGroup.end();

                FormUI::Container::List pins(KFCConfigurationClasses::createFormPinList());

                auto &buttonPinGroup = form.addCardGroup(F("pincfg"), F("Pin Configuration"), false);

                auto &pin0Inverted = form.addObjectGetterSetter(F("pupi"), FormGetterSetter(cfg, pin_ch0_up_inverted));
                form.addFormUI(FormUI::Type::HIDDEN);

                form.addObjectGetterSetter(F("pinup"), FormGetterSetter(cfg, pin_ch0_up));
                form.addFormUI(F("Button Up Pin #"), FormUI::CheckboxButtonSuffix(pin0Inverted, F("Active Low")), pins);

                auto &pin1Inverted = form.addObjectGetterSetter(F("pdbi"), FormGetterSetter(cfg, pin_ch0_down_inverted));
                form.addFormUI(FormUI::Type::HIDDEN);

                form.addObjectGetterSetter(F("pindn"),  FormGetterSetter(cfg, pin_ch0_down));
                form.addFormUI(F("Button Down Pin #"), FormUI::CheckboxButtonSuffix(pin1Inverted, F("Active Low")), pins);

                buttonPinGroup.end();

            }
        #endif
        else if (formName == F("advanced")) {

            auto &fwGroup = form.addCardGroup(F("fwcfg"), F("Advanced Firmware Configuration"), true);

            #if DIMMER_FIRMWARE_VERSION >= 0x020200

                form.addCallbackSetter(F("dmode"), firmwareConfig.bits.leading_edge, [&firmwareConfig](const bool value, FormField &) {
                    firmwareConfig.bits.leading_edge = value;
                });
                form.addFormUI(F("Operation Mode"), configValidAttr, FormUI::Container::List(0, F("Trailing Edge"), 1, F("Leading Edge")));

                form.addPointerTriviallyCopyable(F("rbeg"), &firmwareConfig.range_begin);
                form.addFormUI(F("Level Range Start"), configValidAttr, FormUI::Suffix(F("0 - Range End")));
                form.addValidator(FormUI::Validator::Range(0, kMaxLevelsChannel));

                form.addCallbackSetter(F("rend"), firmwareConfig.get_range_end(), [&firmwareConfig](uint16_t value, FormField &) {
                    firmwareConfig.set_range_end(value);
                });
                form.addFormUI(F("Level Range End"), configValidAttr, FormUI::Suffix(ARRAY_F(stdex::array_concat(stdex::str_to_array("Range Start - "), stdex::int_to_array<int, kMaxLevelsChannel>()))));
                form.addValidator(FormUI::Validator::Range(0, kMaxLevelsChannel));

            #endif

            #if DIMMER_FIRMWARE_VERSION <= 0x020105

                form.addPointerTriviallyCopyable(F("zc_offset"), &firmwareConfig.zero_crossing_delay_ticks);
                form.addFormUI(F("Zero Crossing Offset"), configValidAttr, FormUI::Suffix(FSPGM(ticks, "ticks")));
                form.addValidator(FormUI::Validator::Range(0, 65535));

            #endif

            #if DIMMER_FIRMWARE_VERSION >= 0x020200

                auto &negativeZCOffset = form.addCallbackSetter(F("nzcd"), firmwareConfig.bits.negative_zc_delay, [&firmwareConfig](const uint8_t value, FormField &) {
                    firmwareConfig.bits.negative_zc_delay = value;
                });
                form.addFormUI(FormUI::Type::HIDDEN, configValidAttr);

                form.addPointerTriviallyCopyable(F("zc_offset"), &firmwareConfig.zero_crossing_delay_ticks);
                form.addFormUI(F("Zero Crossing Offset"), configValidAttr, FormUI::Suffix(FSPGM(ticks, "ticks")), FormUI::CheckboxButtonSuffix(negativeZCOffset, F("Negative Offset")));
                form.addValidator(FormUI::Validator::Range(0, 65535));


            #endif

            form.addPointerTriviallyCopyable(F("min_on"), &firmwareConfig.minimum_on_time_ticks);
            form.addFormUI(F("Minimum On-time"), configValidAttr, FormUI::Suffix(FSPGM(ticks)));
            form.addValidator(FormUI::Validator::Range(1, 65535));

            form.addPointerTriviallyCopyable(F("min_off"), &firmwareConfig.minimum_off_time_ticks);
            form.addFormUI(F("Minimum Off-time"), configValidAttr, FormUI::Suffix(FSPGM(ticks)));
            form.addValidator(FormUI::Validator::Range(1, 65535));

            form.addCallbackSetter<float>(F("vref11"), firmwareConfig.internal_vref11, [&firmwareConfig](float value, FormField &) {
                firmwareConfig.internal_vref11 = value;
            });
            form.addFormUI(F("ATmega 1.1V Reference Calibration"), configValidAttr, FormUI::PlaceHolder(1.1, 1), FormUI::Suffix(F("V")));
            form.addValidator(FormUI::Validator::RangeDouble(internal_vref11_t((int8_t)-128), internal_vref11_t((int8_t)127), 1));

            form.add<float>(F("temp_ofs"), firmwareConfig.ntc_temp_cal_offset, [&firmwareConfig](const float value, FormField &, bool) {
                firmwareConfig.ntc_temp_cal_offset = value;
                return false;
            });
            form.addFormUI(F("Temperature Offset (NTC)"), configValidAttr, FormUI::PlaceHolder(0), FormUI::Suffix(FSPGM(UTF8_degreeC)));

            form.addPointerTriviallyCopyable(F("tsofs"), &firmwareConfig.internal_temp_calibration.ts_offset);
            form.addFormUI(F("TS Offset (ATmega Sensor)"), configValidAttr, FormUI::PlaceHolder(0), FormUI::Suffix(F("temperature sensor offset correction")));
            form.addValidator(FormUI::Validator::Range(0, 255));

            form.addPointerTriviallyCopyable(F("tsgain"), &firmwareConfig.internal_temp_calibration.ts_gain);
            form.addFormUI(F("TS Gain"), configValidAttr, FormUI::PlaceHolder(0), FormUI::Suffix(F("8-bit unsigned fixed point 1/128th units")));
            form.addValidator(FormUI::Validator::Range(0, 255));
            // , [&firmwareConfig](const uint8_t value, FormField &) {
            //     firmwareConfig.int_temp_offset = value;
            //     return false;
            // });

            fwGroup.end();

        }

        form.finalize();
    }

}
