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

class FormNetworkPortValidator : public FormRangeValidator
{
public:
    static constexpr long kPortMin = 1;
    static constexpr long kPortMax = 65535;

    FormNetworkPortValidator(long min, long max, bool allowZero = false);
    FormNetworkPortValidator(bool allowZero = false) : FormNetworkPortValidator(kPortMin, kPortMax, allowZero) {}
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

template<class T>
class FormRangeValidatorEnum : public FormRangeValidator
{
public:
    FormRangeValidatorEnum() : FormRangeValidator(static_cast<long>(T::MIN), static_cast<long>(T::MAX) - 1) {}
    FormRangeValidatorEnum(const String &message) : FormRangeValidator(message, static_cast<long>(T::MIN), static_cast<long>(T::MAX) - 1) {}
};

template<class T>
class FormRangeValidatorType : public FormRangeValidator
{
public:
    FormRangeValidatorType() : FormRangeValidator(std::numeric_limits<T>::min(), std::numeric_limits<T>::max()) {}
    FormRangeValidatorType(const String &message) : FormRangeValidator(message, std::numeric_limits<T>::min(), std::numeric_limits<T>::max()) {}
};
