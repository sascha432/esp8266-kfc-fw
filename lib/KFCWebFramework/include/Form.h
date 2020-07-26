/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <functional>
#include <vector>
#include <PrintHtmlEntities.h>
#include "FormField.h"
#include "FormValue.h"
#include "FormObject.h"
#include "FormBitValue.h"
#include "FormString.h"
#include "FormStringObject.h"

class FormError;
class FormData;

class Form {
public:
    typedef std::vector<FormField *> FieldsVector;
    typedef std::vector<FormError> ErrorsVector;
    typedef std::function<bool(Form &form)> ValidateCallback_t;

    using PrintInterface = FormField::PrintInterface;

    Form(FormData *data = nullptr);
    virtual ~Form();

    void clearForm();
    void setFormData(FormData *data);
    void setInvalidMissing(bool invalidMissing);

    // name field:
    // #my-id = set id="my-id"1
    // random-str = set id="random-str"
    // .my-class = add class="my-class"
    // empty = no id/no class
    FormGroup &addGroup(const String &name, const String &label, bool expanded, FormUI::TypeEnum_t type = FormUI::TypeEnum_t::GROUP_START);
    // dependencies field:
    // the string must contain a valid JSON object. ' is replaced with " to avoid too much escaping. use \\' for javascript strings (becomes ")
    //
    // i: '#input', 't': '#target', 's': { '#input_value': 'code to execute', 'm': 'code to execute if value is not found in s', 'e': 'code to always execute before' }
    // code: $I input, $T target, $V value
    //
    // #group-div-class is the target, #my_input_field: value 0 and 1 hide the div, 2 shows it
    // auto &group = form.addDivGroup(F("group-div-class"), F("{'i':'#my_input_field','s':{'0':'$T.hide()','1':'$T.hide()','2':'$T.show()'}},'m':'alert(\\'Invalid value: \\'+$V)'"));
    FormGroup &addDivGroup(const String &id, const String &dependencies = String()) {
        return addGroup(id, dependencies, false, FormUI::TypeEnum_t::GROUP_START_DIV);
    }
    FormGroup &addHrGroup(const String &id, const String &label = String()) {
        return addGroup(id, label, false, FormUI::TypeEnum_t::GROUP_START_HR);
    }

    int add(FormField *field);
    FormField *_add(FormField *field);

    //
    // NOTE: variable names that fit into the String's SSOSIZE saves a lot memory (ESP8266 length < 11)
    //

    FormField *add(const String &name, const String &value, FormField::InputFieldType type = FormField::InputFieldType::TEXT);

    //
    // NOTE: lambda functions as callbacks requires memory. use a static function whenever possible
    //

    FormField *add(const String &name, const String &value, FormStringObject::SetterCallback_t setter = nullptr, FormField::InputFieldType type = FormField::InputFieldType::TEXT);
    FormField *add(const String &name, String *value, FormField::InputFieldType type = FormField::InputFieldType::TEXT);

    FormField *add(const String &name, const IPAddress &value, typename FormObject<IPAddress>::SetterCallback_t setter = nullptr, FormField::InputFieldType type = FormField::InputFieldType::TEXT) {
        return _add(new FormObject<IPAddress>(name, value, setter, type));
    }

    template <class T>
    FormField *add(const String &name, T value, typename FormValue<T>::GetterSetterCallback_t callback = nullptr, FormField::InputFieldType type = FormField::InputFieldType::SELECT) {
        return _add(new FormValue<T>(name, value, callback, type));
    }

    template <class T>
    FormField *add(const String &name, T *value, typename FormValue<T>::GetterSetterCallback_t callback, FormField::InputFieldType type = FormField::InputFieldType::SELECT) {
        return _add(new FormValue<T>(name, value, callback, type));
    }

    template <class T, size_t N>
    FormField *add(const String &name, T *value, std::array<T, N> bitmask, FormField::InputFieldType type = FormField::InputFieldType::SELECT) {
        return _add(new FormBitValue<T, N>(name, value, bitmask, type));
    }

    template <class T>
    FormField *add(const String &name, T *value, FormField::InputFieldType type = FormField::InputFieldType::SELECT) {
        return _add(new FormValue<T>(name, value, type));
    }

    template <size_t size>
    FormField *add(const String &name, char *value, FormField::InputFieldType type = FormField::InputFieldType::TEXT) {
        return _add(new FormString<size>(name, value, type));
    }

    // template <class C, bool isObject>
    // FormField *add(const String &name, C *object, FormField::InputFieldType type = FormField::InputFieldType::TEXT) {
    //     return _add(new FormObject<C>(name, object, type));
    // }

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
    void createJavascript(PrintInterface &out) const;

    void setFormUI(const String &title, const String &submit);
    void setFormUI(const String &title);
    void createHtml(PrintInterface &out);

    void dump(Print &out, const String &prefix) const;

    FormData *getFormData() const {
        return _data;
    }

    void setValidateCallback(ValidateCallback_t validateCallback) {
        _validateCallback = validateCallback;
    }

    // FieldsVector &getFields() {
    //     return _fields;
    // }

private:
    static const char *_jsonEncodeString(const String &str, PrintInterface &out);

    FormData *_data;
    FieldsVector _fields;
    ErrorsVector _errors;
    bool _invalidMissing;
    bool _hasChanged;
    String _formTitle;
    String _formSubmit;
    ValidateCallback_t _validateCallback;
};
