/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "FormField.h"

template <typename ObjType>
class FormObject : public FormField {
public:
    typedef std::function<bool(const ObjType &value, FormField &field, bool store)> Callback;

    FormObject(const String &name, ObjType obj, Callback callback = nullptr, Type type = Type::TEXT) : FormField(name, obj.toString(), type), _object(obj), _callback(callback) {
    }
    FormObject(const String &name, ObjType obj, Type type = Type::TEXT) : FormField(name, obj.toString(), type), _object(obj), _callback(nullptr) {
    }

    virtual void copyValue() {
        _object.fromString(getValue());
        if (_callback) {
            _callback(_object, *this, true);
        }
    }

private:
    ObjType _object;
    Callback _callback;
};

