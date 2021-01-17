/**
 * Author: sascha_lammers@gmx.de
 */

auto &fadingGroup = form.addCardGroup(F("fading"), F("Random Color Fading"), true);

form.addPointerTriviallyCopyable(F("fade_sp"), &cfg.fading.speed);
form.addFormUI(F("Time Between Fading Colors"), FormUI::Suffix(FSPGM(seconds)));
form.addValidator(FormUI::Validator::Range(Clock::FadingAnimation::kMinSeconds, Clock::FadingAnimation::kMaxSeconds));

form.addPointerTriviallyCopyable(F("fade_dy"), &cfg.fading.delay);
form.addFormUI(F("Delay Before Start Fading To Next Random Color"), FormUI::Suffix(FSPGM(seconds)));
form.addValidator(FormUI::Validator::Range(0, Clock::FadingAnimation::kMaxDelay));

form.add(F("fade_cf"), Color(cfg.fading.factor.value).toString(), [&cfg](const String &value, FormUI::Field::BaseField &field, bool store) {
    if (store) {
        cfg.fading.factor.value = Color::fromString(value);
    }
    return false;
});
form.addFormUI(F("Random Color Factor"));

fadingGroup.end();

