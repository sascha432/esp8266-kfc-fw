/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <functional>
#include <vector>
#include <PrintHtmlEntitiesString.h>
#include "FormField.h"
#include "FormValue.h"
#include "FormObject.h"
#include "FormBitValue.h"
#include "FormString.h"
#include "FormStringObject.h"

#ifndef DEBUG_KFC_FORMS
#define DEBUG_KFC_FORMS         0
#endif

#if DEBUG_KFC_FORMS
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#ifdef DEFAULT
#undef DEFAULT
#endif

class FormError;
class FormData;

class Form {
public:
    using PrintInterface = FormField::PrintInterface;
    using FormFieldPtr = std::unique_ptr<FormField>;
    using FieldsVector = std::vector<FormFieldPtr>;
    using ErrorsVector = std::vector<FormError> ;
    using ValidateCallback = std::function<bool(Form &form)> ;
    using CStringGetter = std::function<const char *()>;
    using CStringSetter = std::function<void(const char *)>;

    Form(FormData *data = nullptr);

    void clearForm();
    void setFormData(FormData *data);
    void setInvalidMissing(bool invalidMissing);

    FormField *getField(const String &name) const;
    FormField &getField(int index) const;
    size_t hasFields() const;

    // --------------------------------------------------------------------
    // Validators
    //
    // addValidator adds the validator to the last FormField that was added

    template<typename T>
    T &addValidator(T &&validator)
    {
        return _fields.back()->addValidator<T>(std::move(validator));
    }

    template<typename T>
    T &addValidator(FormField &field, T &&validator)
    {
        return field.addValidator<T>(std::move(validator));
    }

    // --------------------------------------------------------------------
    // FormUI
    //

    FormUI::Config &getFormUIConfig() {
        if (!_uiConfig) {
            _uiConfig.reset(new FormUI::Config(_strings));
        }
        return *_uiConfig;
    }

    void setFormUI(const String &title, const String &submit) __attribute__ ((deprecated)) {
        auto &cfg = getFormUIConfig();
        cfg.setTitle(title);
        cfg.setSaveButtonLabel(submit);
    }
    void setFormUI(const String &title) __attribute__ ((deprecated)) {
        getFormUIConfig().setTitle(title);
    }


    // addFormUI adds the FormUI object or elements to the last FormField that was added
    // Arguments for addFormUI(...)
    //
    // FormUI:Label
    // FormUI:PlaceHolder
    // FormUI:Suffix
    // FormUI:ItemList					type will be set to SELECT
    // FormUI:BoolItems					type will be set to SELECT
    // FormUI:*
    // const __FlashStringHelper		converted to FormUI:Label
    // FormUI::Type:*                   default = TEXT

    template <typename... Args>
    FormUI::UI &addFormUI(Args &&... args) {
        auto parent = _fields.back().get();
        return addFormUI(new FormUI::UI(parent, std::forward<Args>(args)...));
    }

    FormUI::UI &addFormUI(FormUI::UI *formUI);

    // --------------------------------------------------------------------
    // Form data handling
    //
    void clearErrors();
    bool validate();
    bool validateOnly();
    bool isValid() const;
    bool hasChanged() const;
    bool hasError(FormField &field) const;
    void copyValidatedData();
    const ErrorsVector &getErrors() const;

    // call when all fields have been added
    void finalize() const;

    void createHtml(PrintInterface &out);
    bool process(const String &name, Print &output);
    void createJavascript(PrintInterface &out);

    void dump(Print &out, const String &prefix) const;

    FormData *getFormData() const;
    void setValidateCallback(ValidateCallback validateCallback);

    // --------------------------------------------------------------------
    // Methods for adding fields
    //
    // NOTES:
    // - variable names that fit into the String's SSOSIZE saves a lot memory (ESP8266 length <= 10)
    // - lambda functions as callback requires memory. use a static function whenever possible
    // - ESP8266/32 accessing packed structures that have been passed as reference or pointer might
    //   cause an exception cause of unaligend reads/writes. use getter and setters
    //

    // getter and setter for const char *
    FormField &addCStringGetterSetter(const String &name, CStringGetter getter, CStringSetter setter, FormField::Type type = FormField::Type::TEXT) {
        return add(name, String(getter()), [setter](const String &str, FormField &, bool store) {
            if (store) {
                setter(str.c_str());
            }
            return false;
        }, type);
    }

    // getter and setters for enum, bitfields etc...
    // obj must exist until the form object has been deleted
    template<typename ObjType, typename VarType>
    FormField &addObjectGetterSetter(const String &name, ObjType &obj, VarType(*getter)(const ObjType &obj), void(*setter)(ObjType &obj, VarType), FormField::Type type = FormField::Type::TEXT) {
        __LDBG_printf("name=%s value=%s obj=%p", name.c_str(), String(getter(obj)).c_str(), &obj);
        return add(name, getter(obj), [&obj, setter _IF_DEBUG(, getter)](const VarType &value, FormField &field, bool store) {
            if (store) {
                setter(obj, value);
            }
            __LDBG_printf("name=%s value=%s obj=%p new_value=%s store=%u", field.getName().c_str(), String(value).c_str(), &obj, String(getter(obj)).c_str(), store);
            return false;
        }, type);
    }

    template<typename ObjType>
    FormField &addIPGetterSetter(const String &name, ObjType &obj, IPAddress(*getter)(const ObjType &obj), void(*setter)(ObjType &obj, const IPAddress &value), FormField::Type type = FormField::Type::TEXT) {
        __LDBG_printf("name=%s value=%s obj=%p", name.c_str(), getter(obj).toString().c_str(), &obj);
        return add(name, getter(obj), [&obj, setter _IF_DEBUG(, getter)](const IPAddress &value, FormField &field, bool store) {
            if (store) {
                setter(obj, value);
            }
            __LDBG_printf("name=%s value=%s obj=%p new_value=%s store=%u", field.getName().c_str(), value.toString().c_str(), &obj, getter(obj).toString().c_str(), store);
            return false;
        }, type);
    }

    // ESP8266 support for accessing unaligned member variables
    // obj must exist until the form object has been deleted
    template<typename ObjType, typename VarType, typename CastVarType = VarType>
    FormField &addMemberVariable(const String &name, ObjType &obj, VarType ObjType::*memberPtr, FormField::Type type = FormField::Type::TEXT) {
        __LDBG_printf("name=%s value=%s obj=%p ptr=%p", name.c_str(), String(static_cast<VarType>(obj.*memberPtr)).c_str(), &obj, memberPtr);
        return add<VarType>(name, static_cast<VarType>(obj.*memberPtr), [&obj, memberPtr](const VarType &value, FormField &field, bool store) {
            if (store) {
                obj.*memberPtr = static_cast<CastVarType>(value);
            }
            __LDBG_printf("name=%s value=%s obj=%p ptr=%p cast=%s new_value=%s store=%u", field.getName().c_str(), String(static_cast<VarType>(value)).c_str(), &obj, memberPtr, String(static_cast<CastVarType>(value)).c_str(), String(static_cast<VarType>(obj.*memberPtr)).c_str(), store);
            return false;
        }, type);
    }

    FormField &add(const String &name, const String &value, FormField::Type type = FormField::Type::TEXT) {
        return _add(new FormField(name, value, type));
    }

    FormField &add(const String &name, const String &value, FormStringObject::Callback callback = nullptr, FormField::Type type = FormField::Type::TEXT) {
        return _add(new FormStringObject(name, value, callback, type));
    }

    FormField &add(const String &name, String &value, FormField::Type type = FormField::Type::TEXT) {
        return _add(new FormStringObject(name, value, type));
    }

    FormField &add(const String &name, const IPAddress &value, typename FormObject<IPAddress>::Callback callback = nullptr, FormField::Type type = FormField::Type::TEXT) {
        return _add(new FormObject<IPAddress>(name, value, callback, type));
    }

    template <typename VarType>
    FormField &add(const String &name, VarType value, typename FormValue<VarType>::Callback callback = nullptr, FormField::Type type = FormField::Type::SELECT) {
        return _add(new FormValue<VarType>(name, value, callback, type));
    }

    template <typename ArrayElementType, size_t kArraySize>
    FormField &add(const String &name, ArrayElementType *value, std::array<ArrayElementType, kArraySize> bitmask, FormField::Type type = FormField::Type::SELECT) {
        return _add(new FormBitValue<ArrayElementType, kArraySize>(name, value, bitmask, type));
    }

    template <size_t kMaxSize>
    FormField &add(const String &name, char *value, FormField::Type type = FormField::Type::TEXT) {
        return _add(new FormCString(name, value, kMaxSize, type));
    }

    // --------------------------------------------------------------------
    // Methods for adding groups
    //
    // auto &grp = form.addGroup(...);
    // ...fields inside the group...
    // grp.end();

    // expandable group
    //
    // name field:
    // #my-id = set id="my-id"
    // .my-class = add to class="my-class"
    // random-str = set id="random-str"
    // empty = no id/no class
    FormGroup &addGroup(const String &name, const FormUI::Label &label, bool expanded, FormUI::Type type = FormUI::Type::GROUP_START);

    // title and separator
    FormGroup &addHrGroup(const String &id, const FormUI::Label &label = String()) {
        return addGroup(id, label, false, FormUI::Type::GROUP_START_HR);
    }

    // no disible attributes
    //
    // dependencies field:
    // the string must contain a valid JSON object. ' is replaced with " to avoid too much escaping. use \\' for javascript strings (becomes ")
    //
    // i: '#input', 't': '#target', 's': { '#input_value': 'code to execute', 'm': 'code to execute if value is not found in s', 'e': 'code to always execute before' }
    // code: $I input, $T target, $V value
    //
    // #group-div-class is the target, #my_input_field: value 0 and 1 hide the div, 2 shows it
    // auto &group = form.addDivGroup(F("group-div-class"), F("{'i':'#my_input_field','s':{'0':'$T.hide()','1':'$T.hide()','2':'$T.show()'}},'m':'alert(\\'Invalid value: \\'+$V)'"));
    FormGroup &addDivGroup(const String &id, const String &dependencies = String()) {
        return addGroup(id, FormUI::Label(dependencies, true), false, FormUI::Type::GROUP_START_DIV);
    }

    // form/field.getFormUIConfig() provides the container id
    // id is "header-<id>" for the card header and "collapse-<id>" for the card body
    // if label is empty, the card header is not created and the card body cannot be collapsed
    // expanded is the intial state of the cardd body either show=true or hide=false
    FormGroup &addCardGroup(const String &id, const FormUI::Label &label, bool expanded = false) {
        return addGroup(id, label, expanded, FormUI::Type::GROUP_START_CARD);
    }

private:
    FormField &_add(FormField *field);
    FormGroup &_add(FormGroup *field) {
        return reinterpret_cast<FormGroup &>(_add(reinterpret_cast<FormField *>(field)));
    }

    const char *jsonEncodeString(const String &str, PrintInterface &out);

    // const char *jsonEncodeString(const String &str);

    // // creates an encoded string that is attached to output
    // const char *encodeHtmlEntities(const char *str, bool attribute);

    // const char *encodeHtmlEntities(const String &str, bool attribute) {
    //     return encodeHtmlEntities(str.c_str(), attribute);
    // }


    FormData *_data;
    FieldsVector _fields;
    ErrorsVector _errors;
    bool _invalidMissing: 1;
    bool _hasChanged: 1;
    FormUI::ConfigPtr _uiConfig;
    ValidateCallback _validateCallback;
    StringDeduplicator _strings;
};

#if DEBUG_KFC_FORMS
#include <debug_helper_disable.h>
#endif
