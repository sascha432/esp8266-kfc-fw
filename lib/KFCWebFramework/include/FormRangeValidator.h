/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "FormValidator.h"

class FormRangeValidator : public FormValidator {
public:
    FormRangeValidator(long min, long max, bool allowZero = false);
    FormRangeValidator(const String &message, long min, long max, bool allowZero = false);

    bool validate();
    virtual String getMessage() override;

private:
    long _min;
    long _max;
    bool _allowZero;
};

class FormRangeValidatorDouble : public FormValidator {
public:
    FormRangeValidatorDouble(double min, double max, uint8_t digits = 2, bool allowZero = false);
    FormRangeValidatorDouble(const String &message, double min, double max, uint8_t digits = 2, bool allowZero = false);

    bool validate();
    virtual String getMessage() override;

private:
    double _min;
    double _max;
    uint8_t _digits;
    bool _allowZero;
};

