/**
 * Author: sascha_lammers@gmx.de
 */
// --------------------------------------------------------------------
auto &animationGroup = form.addCardGroup(F("anicfg"), FSPGM(Animation), true);

form.addObjectGetterSetter(F("ani"), cfg, cfg.get_int_animation, cfg.set_int_animation);
form.addFormUI(FSPGM(Type), animationTypeItems);
//form.addValidator(FormUI::Validator::RangeEnum<AnimationType>());

form.add(F("col"), Color(cfg.solid_color.value).toString(), [&cfg](const String &value, FormUI::Field::BaseField &field, bool store) {
    if (store) {
        cfg.solid_color.value = Color::fromString(value);
    }
    return false;
}, FormUI::Field::Type::TEXT);
form.addFormUI(FSPGM(Solid_Color));

form.addPointerTriviallyCopyable(F("flash_sp"), &cfg.flashing_speed);
form.addFormUI(F("Flashing Speed"), FormUI::Suffix(FSPGM(milliseconds)));
form.addValidator(FormUI::Validator::Range(kMinFlashingSpeed, 0xffff));

animationGroup.end();
