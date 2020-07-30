/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <type_traits>
#include "FormValidator.h"

class FormCallbackValidator : public FormValidator {
public:
    using Callback = std::function<bool(String, FormField &)>;

    FormCallbackValidator(Callback callback);
    FormCallbackValidator(const String &message, Callback callback);

    bool validate();

private:
    Callback _callback;
};

template <typename T>
class FormTCallbackValidator : public FormValidator {
public:
    using Callback = std::function<bool(T, FormField &)>;

    FormTCallbackValidator(Callback callback) : FormTCallbackValidator(String(), callback) {
    }
    FormTCallbackValidator(const String &message, Callback callback) {
        _callback = callback;
    }

    bool validate() {
        if (std::is_floating_point<T>::value) {
            return FormValidator::validate() && _callback((T)getField().getValue().toFloat(), getField());
        }
        else {
            return FormValidator::validate() && _callback((T)getField().getValue().toInt(), getField());
        }
    }

private:
    Callback _callback;
};
