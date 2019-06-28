/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "FormValidator.h"

class FormRangeValidator : public FormValidator {
public:
    FormRangeValidator(long min, long max);
    FormRangeValidator(const String &message, long min, long max);

    bool validate();
    virtual String getMessage() override;

private:
    long _min;
    long _max;
};

