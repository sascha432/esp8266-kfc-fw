/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <type_traits>
#include "FormValidator.h"

class FormCallbackValidator : public FormValidator {
public:
    typedef std::function<bool(String, FormField &)> Callback_t;

    FormCallbackValidator(Callback_t callback);
    FormCallbackValidator(const String &message, Callback_t callback);

    bool validate();

private:
    Callback_t _callback;
};

template <typename T>
class FormTCallbackValidator : public FormValidator {
public:
    typedef std::function<bool(T, FormField &)> Callback_t;

    FormTCallbackValidator(Callback_t callback) : FormTCallbackValidator(String(), callback) {
    }
    FormTCallbackValidator(const String &message, Callback_t callback) {
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
    Callback_t _callback;
};
