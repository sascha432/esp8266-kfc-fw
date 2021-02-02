/**
 * Author: sascha_lammers@gmx.de
 */

form.addPointerTriviallyCopyable(FSPGM(brightness), &cfg.brightness);
form.addFormUI(FormUI::Type::RANGE_SLIDER, FSPGM(Brightness), FormUI::MinMax(0, 255));

#if IOT_CLOCK_AMBIENT_LIGHT_SENSOR

form.addPointerTriviallyCopyable(F("auto_br"), &cfg.auto_brightness);
form.addFormUI(F("Auto Brightness Value"), FormUI::SuffixHtml(F("<span class=\"input-group-text\">0-1023</span><span id=\"abr_sv\" class=\"input-group-text\"></span><button class=\"btn btn-secondary\" type=\"button\" id=\"dis_auto_br\">Disable</button>")));
form.addValidator(FormUI::Validator::Range(-1, 1023));

#endif

form.addObjectGetterSetter(F("ft"), cfg, cfg.get_bits_fading_time, cfg.set_bits_fading_time);
form.addFormUI(F("Fading Time From 0 To 100%"), FormUI::Suffix(FSPGM(milliseconds)));
cfg.addRangeValidatorFor_fading_time(form, true);

mainGroup.end();
