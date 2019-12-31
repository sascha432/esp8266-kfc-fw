/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <functional>
#include <vector>
#include "FormField.h"
#include "FormValue.h"
#include "FormObject.h"
#include "FormBitValue.h"
#include "FormString.h"

class FormError;
class FormData;

class Form {
public:
    typedef std::vector<FormField *> FieldsVector;
    typedef std::vector<FormError> ErrorsVector;

    Form();
    Form(FormData *data);
    virtual ~Form();

    void clearForm();
    void setFormData(FormData *data);
    void setInvalidMissing(bool invalidMissing);

    int add(FormField *field);
    FormField *_add(FormField *field);
    FormField *add(const String &name, const String &value, FormField::FieldType_t type = FormField::INPUT_TEXT);

    template <typename T>
    FormField* add(const String& name, T value, typename FormValue<T>::GetterSetterCallback_t callback = nullptr, FormField::FieldType_t type = FormField::INPUT_SELECT) {
        return _add(new FormValue<T>(name, value, callback, type));
    }

    template <typename T>
    FormField* add(const String& name, T *value, typename FormValue<T>::GetterSetterCallback_t callback, FormField::FieldType_t type = FormField::INPUT_SELECT) {
        return _add(new FormValue<T>(name, value, callback, type));
    }

    template <typename T, size_t N>
    FormField *add(const String &name, T *value, std::array<T, N> bitmask, FormField::FieldType_t type = FormField::INPUT_SELECT) {
        return _add(new FormBitValue<T, N>(name, value, bitmask, type));
    }

    template <typename T>
    FormField* add(const String& name, T* value, FormField::FieldType_t type = FormField::INPUT_SELECT) {
        return _add(new FormValue<T>(name, value, type));
    }

    template <size_t size>
    FormField *add(const String &name, char *value, FormField::FieldType_t type = FormField::INPUT_TEXT) {
        return _add(new FormString<size>(name, value, type));
    }

    template <class C, bool O>
    FormField *add(const String &name, C *object, FormField::FieldType_t type = FormField::INPUT_TEXT) {
        return _add(new FormObject<C>(name, object, type));
    }

    FormValidator *addValidator(int index, FormValidator *validator);
    FormValidator *addValidator(FormValidator *validator);
    FormValidator *addValidator(const String &name, FormValidator *validator);

    FormField *getField(const String &name) const;
    FormField &getField(int index) const;
    size_t hasFields() const;

    void clearErrors();
    bool validate();
    bool validateOnly();
    bool isValid() const;
    bool hasChanged() const;
    bool hasError(FormField *field) const;
    void copyValidatedData();
    const ErrorsVector &getErrors() const;
    void finalize() const;

    const char *process(const String &name) const;
    void createJavascript(Print &out);

    void setFormUI(const String& title, const String& submit);
    void setFormUI(const String& title);
    void createHtml(Print& out);

    void dump(Print &out, const String &prefix) const;

    FormData *getFormData() const {
        return _data;
    }

private:
    FormData *_data;
    FieldsVector _fields;
    ErrorsVector _errors;
    bool _invalidMissing;
    bool _hasChanged;
    String _formTitle;
    String _formSubmit;
};
