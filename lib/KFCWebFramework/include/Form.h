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
#define DEBUG_KFC_FORMS                             1
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
    using ErrorsVector = std::vector<FormError>;
    using ValidateCallback = std::function<bool(Form &form)>;

    Form(FormData *data = nullptr);

    void clearForm();
    void setFormData(FormData *data);
    void setInvalidMissing(bool invalidMissing);

    FormField *getField(const String &name) const;
    FormField &getField(size_t index) const {
        return *_fields.at(index);
    }
    size_t hasFields() const {
        return _fields.size();
    }
    const FieldsVector &getFields() const {
        return _fields;
    }

    void removeValidators();

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
    // - ESP8266/ESP32/other ARM arch: accessing packed/unaligned structures by reference or pointer might
    //   cause an expcetion or undefined behavior. use addPointer/addReference for TriviallyCopyable
    //   or getter/setters for other objects inside the structure
    //


    // --------------------------------------------------------------------
    // general add methods

    template<typename ObjType, typename VarType, typename MemberVarType = std::member_pointer_value_t<VarType typename ObjType:: *>>
    FormValueCallback<MemberVarType> &add(const String &name, ObjType &obj, VarType ObjType:: *memberPtr, FormField::Type type = FormField::Type::TEXT) {
        return addMemberVariable<ObjType, VarType, MemberVarType>(name, obj, memberPtr, type);
    }

    FormField &add(const String &name, const String &value, FormField::Type type = FormField::Type::TEXT) {
        return _add<FormField>(name, value, type);
    }

    template <typename VarType>
    FormValueCallback<VarType> &add(const String &name, typename FormValueCallback<VarType>::GetterSetterCallback callback, FormField::Type type = FormField::Type::SELECT) {
        return addCallbackGetterSetter(name, callback, type);
    }

    template <typename VarType, typename Callback = typename FormValueCallback<VarType>::SetterCallback>
    FormValueCallback<VarType> &add(const String &name, VarType value, Callback callback, FormField::Type type = FormField::Type::SELECT) {
        return _add<FormValueCallback<VarType>>(name, value, callback, type);
    }

    // this method cannot be used for packed or unaligned structs
    // see addPointer, addReference and addMemberVariable
    template <typename VarType>
    FormValuePointer<VarType> &add(const String &name, VarType *value, FormField::Type type = FormField::Type::SELECT) {
#if defined(__AVR__) || defined(ESP8266) || defined(ESP32)
        static_assert(((uintptr_t)value) % sizeof(uintptr_t) == 0, "address not aligned");
#endif
        return _add<FormValuePointer<VarType>>(name, value, type);
    }

    // this method cannot be used for packed or unaligned structs
    // see addPointer, addReference and addMemberVariable
    template <typename VarType>
    FormValuePointer<VarType> &add(const String &name, VarType &value, FormField::Type type = FormField::Type::SELECT) {
#if defined(__AVR__) || defined(ESP8266) || defined(ESP32)
        static_assert(((uintptr_t)std::addressof(value)) % sizeof(uintptr_t) == 0, "address not aligned");
#endif
        return _add<FormValuePointer<VarType>>(name, reinterpret_cast<VarType *>(std::addressof(value)), type);
    }

    template <typename ArrayElementType, size_t N>
    FormBitValue<ArrayElementType, N> &add(const String &name, ArrayElementType *value, std::array<ArrayElementType, N> bitmask, FormField::Type type = FormField::Type::SELECT) {
        return _add<FormBitValue<ArrayElementType, N>>(name, value, bitmask, type);
    }

    template <size_t kMaxSize>
    FormCString &add(const String &name, char *value, FormField::Type type = FormField::Type::TEXT) {
        return _add<FormCString>(name, value, kMaxSize, type);
    }

    // getter and setter for char *
    FormValueCallback<String> &addStringGetterSetter(const String &name, const char *(* getter)(), void(* setter)(const char *), FormField::Type type = FormField::Type::TEXT) {
        return _add<FormValueCallback<String>>(name, [setter, getter](String &str, FormField &, bool store) {
            if (store) {
                setter(str.c_str());
            }
            else {
                str = getter();
            }
            return true;
        }, type);
    }

    template<typename ObjType, typename VarType>
    struct ConstRefSetterCallback {
        using Setter = void(*)(ObjType &obj, const VarType &value);
        //typedef void(*setter)(ObjType &obj, const VarType &value);
    };
    template<typename ObjType, typename VarType>
    struct PointerSetterCallback {
        using Setter = void(*)(ObjType &obj, VarType *value);
        //typedef void(*setter)(ObjType &obj, VarType *value);
    };
    template<typename ObjType, typename VarType>
    struct CopySetterCallback {
        using Setter = void(*)(ObjType &obj, VarType value);
        //typedef void(*setter)(ObjType &obj, VarType value);
    };


    template<typename ObjType, typename VarType, typename CallbackType = typename ConstRefSetterCallback<ObjType, VarType>::Setter>
    FormValueCallback<VarType> &addObjectGetterSetter(const String &name, ObjType &obj, VarType(* getter)(const ObjType &obj), CallbackType setter, FormField::Type type = FormField::Type::TEXT) {
        return _add<FormValueCallback<VarType>>(name, [&obj, setter, getter](VarType &value, FormField &field, bool store) {
            if (store) {
                setter(obj, value);
            }
            else {
                value = getter(obj);
            }
            return true;
        }, type);
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

    // no visible attributes
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
        return addGroup(id, FormUI::RawLabel(dependencies), false, FormUI::Type::GROUP_START_DIV);
    }

    // form/field.getFormUIConfig() provides the container id
    // id is "header-<id>" for the card header and "collapse-<id>" for the card body
    // if label is empty, the card header is not created and the card body cannot be collapsed
    // expanded is the intial state of the cardd body either show=true or hide=false
    FormGroup &addCardGroup(const String &id, const FormUI::Label &label, bool expanded = false) {
        return addGroup(id, label, expanded, FormUI::Type::GROUP_START_CARD);
    }

    // --------------------------------------------------------------------
    // specialized add methds

#ifndef _MSC_VER
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif

   // For accessing unaligned member variables in packed structures
    template<typename ObjType, typename VarType, typename MemberVarType = std::member_pointer_value_t<VarType typename ObjType::*>>
    FormValueCallback<MemberVarType> &addMemberVariable(const String &name, ObjType &obj, VarType ObjType:: *memberPtr, FormField::Type type = FormField::Type::TEXT) {
        static_assert(std::is_trivially_copyable<MemberVarType>::value, "only for TriviallyCopyable");
        auto objectBegin = reinterpret_cast<uint8_t *>(std::addressof(obj));
        auto valuePtr = &objectBegin[*(uintptr_t *)&memberPtr];
        return addPointerTriviallyCopyable<MemberVarType>(name, valuePtr, type);
    }

    // use addMemberVariable for packed and unaligned structures
    template<typename VarType>
    FormValueCallback<VarType> &addReference(const String &name, VarType &valueRef, FormField::Type type = FormField::Type::TEXT) {
        static_assert(std::is_trivially_copyable<VarType>::value, "only for TriviallyCopyable");
        return addPointerTriviallyCopyable<VarType>(name, reinterpret_cast<VarType *>(std::addressof(valueRef)), type);
    }

    // data is copied with memcpy to avoid alignment issues when reading from/writing to unaligned/packed structures
    // only for TriviallyCopyable
    template<typename VarType>
    FormValueCallback<VarType> &addPointerTriviallyCopyable(const String &name, VarType *valuePtr, FormField::Type type = FormField::Type::TEXT) {
        static_assert(std::is_trivially_copyable<VarType>::value, "only for TriviallyCopyable");
        auto bytePtr = reinterpret_cast<uint8_t *>(valuePtr);
        return _add<FormValueCallback<VarType>>(name, [bytePtr](VarType &value, FormField &field, bool store) {
            __LDBG_printf("size=%u ptr=%p align=%u", sizeof(VarType), bytePtr, ((intptr_t)bytePtr) % 4);
            // use memcpy to avoid alignment issues
            if (store) {
                memcpy(bytePtr, &value, sizeof(VarType));
            }
            else {
                memcpy(&value, bytePtr, sizeof(VarType));
            }
            return true;
        }, type);
    }

#ifndef _MSC_VER
#pragma GCC diagnostic pop
#endif

    template <typename VarType>
    FormValueCallback<VarType> &addCallbackGetterSetter(const String &name, typename FormValueCallback<VarType>::GetterSetterCallback callback, FormField::Type type = FormField::Type::SELECT) {
        return _add<FormValueCallback<VarType>>(name, callback, type);
    }

    template <typename VarType>
    FormValueCallback<VarType> &addCallbackSetter(const String &name, VarType value, typename FormValueCallback<VarType>::SetterCallback callback, FormField::Type type = FormField::Type::SELECT) {
        return _add<FormValueCallback<VarType>>(name, value, callback, type);
    }

    template<class Ta, class ...Args>
    Ta &_add(Args &&...args) {
        return reinterpret_cast<Ta &>(__add(new Ta(std::forward<Args>(args)...)));
    }

private:
    inline FormField &__add(FormField *field)
    {
        field->setForm(this);
        _fields.emplace_back(field);
        return *_fields.back();
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
