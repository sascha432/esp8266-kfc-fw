/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include "Types.h"
#include "Form/BaseForm.h"
#include "Field/BaseField.h"
#include "Field/Value.h"
#include "Validator/Enum.h"
#include "Validator/Range.h"
#include "Validator/Length.h"
#include "Validator/Match.h"

using FormData = FormUI::Form::Data;
using FormField = FormUI::Field::BaseField;

namespace FormUI {

    //using Suffix = Container::Suffix;
    //using Label = Container::Label;
    //using BoolItems = Container::BoolItems;
    //using CheckboxButtonSuffix = Container::CheckboxButtonSuffix;

    using Type = WebUI::Type;
    using StyleType = WebUI::StyleType;

}
