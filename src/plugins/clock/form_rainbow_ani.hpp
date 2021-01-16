

// --------------------------------------------------------------------
auto &animationGroup = form.addCardGroup(F("anicfg"), FSPGM(Animation), true);

form.addObjectGetterSetter(F("ani"), cfg, cfg.get_bits_animation, cfg.set_bits_animation);
form.addFormUI(FSPGM(Type), FormUI::Container::List(
    AnimationType::NONE, FSPGM(Solid_Color),
    AnimationType::RAINBOW, FSPGM(Rainbow),
    AnimationType::FLASHING, FSPGM(Flashing),
    AnimationType::FADING, FSPGM(Fading)
));
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

// --------------------------------------------------------------------
auto &rainbowGroup = form.addCardGroup(F("rainbow"), F("Rainbow Animation"), true);

form.addPointerTriviallyCopyable(F("rb_mul"), &cfg.rainbow.multiplier.value);
form.addFormUI(FSPGM(Multiplier));
form.addValidator(FormUI::Validator::Range(0.1f, 100.0f));

form.addPointerTriviallyCopyable(F("rb_incr"), &cfg.rainbow.multiplier.incr);
form.addFormUI(F("Multiplier Increment Per Frame"));
form.addValidator(FormUI::Validator::Range(0.0f, 0.1f));

form.addPointerTriviallyCopyable(F("rb_min"), &cfg.rainbow.multiplier.min);
form.addFormUI(F("Minimum Multiplier"));
form.addValidator(FormUI::Validator::Range(0.1f, 100.0f));

form.addPointerTriviallyCopyable(F("rb_max"), &cfg.rainbow.multiplier.max);
form.addFormUI(F("Maximum Multiplier"));
form.addValidator(FormUI::Validator::Range(0.1f, 100.0f));

form.addPointerTriviallyCopyable(F("rb_sp"), &cfg.rainbow.speed);
form.addFormUI(FSPGM(Speed));
form.addValidator(FormUI::Validator::Range(kMinRainbowSpeed, 0xffff));

form.add(F("rb_cf"), Color(cfg.rainbow.color.factor.value).toString(), [&cfg](const String &value, FormUI::Field::BaseField &field, bool store) {
    if (store) {
        cfg.rainbow.color.factor.value = Color::fromString(value);
    }
    return false;
});
form.addFormUI(F("Color Multiplier Factor"));

form.add(F("rb_mv"), Color(cfg.rainbow.color.min.value).toString(), [&cfg](const String &value, FormUI::Field::BaseField &field, bool store) {
    if (store) {
        cfg.rainbow.color.min.value = Color::fromString(value);
    }
    return false;
});
form.addFormUI(F("Minimum Color Value"));

form.addPointerTriviallyCopyable(F("rb_cre"), &cfg.rainbow.color.red_incr);
form.addFormUI(F("Color Increment Per Frame (Red)"));
form.addValidator(FormUI::Validator::Range(0.0f, 0.1f));

form.addPointerTriviallyCopyable(F("rb_cgr"), &cfg.rainbow.color.green_incr);
form.addFormUI(F("Color Increment Per Frame (Green)"));
form.addValidator(FormUI::Validator::Range(0.0f, 0.1f));

form.addPointerTriviallyCopyable(F("rb_cbl"), &cfg.rainbow.color.blue_incr);
form.addFormUI(F("Color Increment Per Frame (Blue)"));
form.addValidator(FormUI::Validator::Range(0.0f, 0.1f));


rainbowGroup.end();

