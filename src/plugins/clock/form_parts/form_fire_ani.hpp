/**
 * Author: sascha_lammers@gmx.de
 */

auto &fireGroup = form.addCardGroup(F("fire"), F("Fire Animation"), true);

form.addPointerTriviallyCopyable(F("firec"), &cfg.fire.cooling);
form.addFormUI(F("Cooling Value:"));
form.addValidator(FormUI::Validator::Range(0, 255));

form.addPointerTriviallyCopyable(F("fires"), &cfg.fire.sparking);
form.addFormUI(F("Sparking Value"));
form.addValidator(FormUI::Validator::Range(0, 255));

form.addPointerTriviallyCopyable(F("firesp"), &cfg.fire.speed);
form.addFormUI(F("Speed"), FormUI::Suffix("milliseconds"));
form.addValidator(FormUI::Validator::Range(5, 100));

auto &invertHidden = form.addObjectGetterSetter(F("firei"), cfg.fire, cfg.fire.get_bit_invert_direction, cfg.fire.set_bit_invert_direction);
form.addFormUI(FormUI::Type::HIDDEN);

auto orientationItems = FormUI::List(
    Plugins::ClockConfig::FireAnimation_t::Orientation::VERTICAL, "Vertical",
    Plugins::ClockConfig::FireAnimation_t::Orientation::HORIZONTAL, "Horizontal"
);

form.addObjectGetterSetter(F("fireo"), cfg.fire, cfg.fire.get_int_orientation, cfg.fire.set_int_orientation);
form.addFormUI(F("Orientation"), orientationItems, FormUI::CheckboxButtonSuffix(invertHidden, F("Invert Direction")));

fireGroup.end();
