/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <functional>
#include <vector>
#include <stl_ext/is_trivially_copyable.h>
#include <stl_ext/type_traits.h>
#include <PrintHtmlEntitiesString.h>
#include "Types.h"
#include "Form/Error.h"
#include "Form/Data.h"
#include "Field/BaseField.h"
#include "Field/Value.h"
#include "Field/BitSet.h"
#include "Field/Group.h"
#include "Field/ValueCallback.h"
#include "Field/ValuePointer.h"
#include "Field/CString.h"
#include "WebUI/BaseUI.h"
#include "WebUI/Storage.h"
#include "WebUI/Containers.h"
#include "Utility/ForwardList.h"

#include "Utility/Debug.h"

AUTO_STRING_DEF(FormEnumValidator_default_message, "Invalid value: %allowed%")
AUTO_STRING_DEF(FormHostValidator_default_message, "Invalid hostname or IP address")
AUTO_STRING_DEF(FormLengthValidator_default_message, "This field must be between %min% and %max% characters")
AUTO_STRING_DEF(FormRangeValidator_default_message, "This fields value must be between %min% and %max%")
AUTO_STRING_DEF(FormRangeValidator_default_message_zero_allowed, "This fields value must be 0 or between %min% and %max%")
AUTO_STRING_DEF(FormRangeValidator_invalid_port, "Invalid Port")
AUTO_STRING_DEF(FormRangeValidator_invalid_port_range, "Invalid Port (%min%-%max%)")
AUTO_STRING_DEF(FormValidator_allowed_macro, "%allowed%")
AUTO_STRING_DEF(FormValidator_max_macro, "%max%")
AUTO_STRING_DEF(FormValidator_min_macro, "%min%")
AUTO_STRING_DEF(Form_value_missing_default_message, "This field is missing")


namespace FormUI {

    namespace Form {

        class BaseForm {
        public:

            BaseForm(Data *data = nullptr) :
                _data(data),
                _errors(nullptr),
                _validators(nullptr),
                _invalidMissing(true),
                _hasChanged(false),
                _uiConfig(nullptr)
            {
#if DEBUG_KFC_FORMS
                _duration.start();
#endif
            }

            virtual ~BaseForm() {
                _clearValidators();
                _clearErrors();
                if (_uiConfig) {
                    delete _uiConfig;
                }
            }

            void clearForm() {
                _fields.clear();
                _clearValidators();
            }

            inline void setFormData(Data *data) {
                _data = data;
            }

            inline void setInvalidMissing(bool invalidMissing) {
                _invalidMissing = invalidMissing;
            }

            Field::BaseField *getField(const __FlashStringHelper *name) const;

            Field::BaseField *getField(const char *name) const {
                return getField(FPSTR(name));
            }

            Field::BaseField *getField(const String &name) const {
                return getField(FPSTR(name.c_str()));
            }

            Field::BaseField &getField(size_t index) const {
                return *_fields.at(index);
            }

            size_t hasFields() const {
                return _fields.size();
            }

            const Field::Vector &getFields() const {
                return _fields;
            }

            inline void removeValidators() {
                _clearValidators();
            }

            // --------------------------------------------------------------------
            // Validators
            // --------------------------------------------------------------------
            // add validator to the last field that has been added

            template<typename T>
            T &addValidator(T &&validator) {
                auto valPtr = new T(std::move(validator));
                valPtr->setField(_fields.back().get());
                _addValidator(valPtr);
                return *valPtr;
            }

            // --------------------------------------------------------------------
            // FormUI
            // --------------------------------------------------------------------
            inline bool hasWebUIConfig() const {
                return _uiConfig;
            }

            // createWebUI() must be called before
            WebUI::Config &getWebUIConfig() {
                return *_uiConfig;
            }

            WebUI::Config &createWebUI();

            void setFormUI(const String &title, const String &submit) __attribute__ ((deprecated));
            void setFormUI(const String &title) __attribute__ ((deprecated));

            // addFormUI adds the FormUI object or elements to the last Field::Base that was added
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
            WebUI::BaseUI &addFormUI(Args &&... args) {
                auto parent = _fields.back().get();
                return addFormUI(new WebUI::BaseUI(parent, std::forward<Args>(args)...));
            }

            WebUI::BaseUI &addFormUI(WebUI::BaseUI *formUI);

            Field::BaseField &getLastField() {
                return *_fields.back().get();
            }

            // --------------------------------------------------------------------
            // Form data handling
            // --------------------------------------------------------------------

            void clearErrors();
            bool validate();
            bool validateOnly();
            bool isValid() const;
            bool hasChanged() const;

            bool hasErrors() const;
            bool hasError(Field::BaseField &field) const;
            void copyValidatedData();
            const Error::Vector &getErrors() const;

            // call when all fields have been added
            void finalize() const;

            void createHtml(PrintInterface &out);
            bool process(const String &name, Print &output);
            void createJavascript(PrintInterface &out);

            void dump(Print &out, const String &prefix) const;

            Data *getFormData() const;

#if KFC_FORMS_HAVE_VALIDATE_CALLBACK
            void setValidateCallback(Validator::Callback validateCallback);
#endif

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

            template<typename _Ta>
            using FormValueCallback = Field::ValueCallback<_Ta>;

            template<typename _Ta>
            using FormValuePointer = Field::ValuePointer<_Ta>;

            template<typename _Ta, size_t N>
            using FormBitValue = Field::BitSet<_Ta, N>;

            // --------------------------------------------------------------------
            // general add methods
            // --------------------------------------------------------------------

            template<typename ObjType, typename VarType, typename MemberVarType = stdex::member_pointer_value_t<VarType ObjType:: *>>
            FormValueCallback<MemberVarType> &add(const __FlashStringHelper *name, ObjType &obj, VarType ObjType:: *memberPtr, InputFieldType type = InputFieldType::TEXT) {
                return addMemberVariable<ObjType, VarType, MemberVarType>(name, obj, memberPtr, type);
            }

            Field::BaseField &add(const __FlashStringHelper *name, const String &value, InputFieldType type = InputFieldType::TEXT) {
                return _add<Field::BaseField>(name, value, type);
            }

            template <typename VarType>
            Field::ValueCallback<VarType> &add(const __FlashStringHelper *name, typename Field::ValueCallback<VarType>::GetterSetterCallback callback, InputFieldType type = InputFieldType::SELECT) {
                return addCallbackGetterSetter(name, callback, type);
            }

            template <typename VarType, typename Callback = typename Field::ValueCallback<VarType>::SetterCallback>
            Field::ValueCallback<VarType> &add(const __FlashStringHelper *name, VarType value, Callback callback, InputFieldType type = InputFieldType::SELECT) {
                return _add<Field::ValueCallback<VarType>>(name, value, callback, type);
            }

            // this method cannot be used for packed or unaligned structs
            // see addPointer, addReference and addMemberVariable
            template <typename VarType, typename _Tb = stdex::relaxed_underlying_type_t<VarType>>
            FormValuePointer<VarType> &add(const __FlashStringHelper *name, VarType *value, InputFieldType type = InputFieldType::SELECT) {
                #if defined(__AVR__) || defined(ESP8266) || defined(ESP32)
                    __DBG_assert_panic(((uintptr_t)value) % sizeof(uintptr_t) == 0, "FormValuePointer(%s, 0x%08x) pointer address not aligned", name, value);
                #endif
                return reinterpret_cast<FormValuePointer<VarType> &>(_add<FormValuePointer<_Tb>>(name, reinterpret_cast<_Tb *>(value), type));
            }

            // this method cannot be used for packed or unaligned structs
            // see addPointer, addReference and addMemberVariable
            template <typename VarType>
            FormValuePointer<VarType> &add(const __FlashStringHelper *name, VarType &value, InputFieldType type = InputFieldType::SELECT) {
                #if defined(__AVR__) || defined(ESP8266) || defined(ESP32)
                    __DBG_assert_panic(((uintptr_t)std::addressof(value)) % sizeof(uintptr_t) == 0, "FormValuePointer(%s, &0x%08x) reference address not aligned", name, std::addressof(value));
                #endif
                return _add<Field::ValuePointer<VarType>>(name, reinterpret_cast<VarType *>(std::addressof(value)), type);
            }

            template <typename ArrayElementType, size_t N>
            FormBitValue<ArrayElementType, N> &add(const __FlashStringHelper *name, ArrayElementType *value, std::array<ArrayElementType, N> bitmask, InputFieldType type = InputFieldType::SELECT) {
                return _add<FormBitValue<ArrayElementType, N>>(name, value, bitmask, type);
            }

            template <size_t kMaxSize>
            Field::CString &add(const __FlashStringHelper *name, char *value, InputFieldType type = InputFieldType::TEXT) {
                return _add<Field::CString>(name, value, kMaxSize, type);
            }

            // getter and setter for char *
            FormValueCallback<String> &addStringGetterSetter(const __FlashStringHelper *name, const char *(* getter)(), void(* setter)(const char *), InputFieldType type = InputFieldType::TEXT) {
                return _add<FormValueCallback<String>>(name, [setter, getter](String &str, Field::BaseField &, bool store) {
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
            FormValueCallback<VarType> &addObjectGetterSetter(const __FlashStringHelper *name, ObjType &obj, VarType(* getter)(const ObjType &obj), CallbackType setter, InputFieldType type = InputFieldType::TEXT) {
                return _add<FormValueCallback<VarType>>(name, [&obj, setter, getter](VarType &value, Field::BaseField &field, bool store) {
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
            // --------------------------------------------------------------------
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
            Group &addGroup(const __FlashStringHelper *name, const Container::Label &label, bool expanded, RenderType type = RenderType::GROUP_START);

            // no label
            Group &addGroup(const __FlashStringHelper *name, bool expanded, RenderType type = RenderType::GROUP_START);

            // title and separator
            Group &addHrGroup(const __FlashStringHelper *id, const Container::Label &label = String()) {
                return addGroup(id, label, false, RenderType::GROUP_START_HR);
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
            Group &addDivGroup(const __FlashStringHelper *id, const String &dependencies = String()) {
                return addGroup(id, Container::RawLabel(dependencies), false, RenderType::GROUP_START_DIV);
            }

            // form/field.createWebUI()/getWebUI() provides the container id
            // id is "header-<id>" for the card header and "collapse-<id>" for the card body
            // expanded is the intial state of the cardd body either show=true or hide=false
            Group &addCardGroup(const __FlashStringHelper *id, const Container::Label &label, bool expanded = false) {
                return addGroup(id, label, expanded, RenderType::GROUP_START_CARD);
            }

            // card body without header that cannot be collapsed
            Group &addCardGroup(const __FlashStringHelper *id) {
                return addGroup(id, true, RenderType::GROUP_START_CARD);
            }

            static constexpr size_t CLOSE_ALL_GROUPS = ~0;

            // end one or more groups starting with the inner ones
            // returns the number of groups that have been closed
            size_t endGroups(size_t levels = 1);

        protected:
            friend FormUI::Group;

            enum class GroupType : uint8_t {
                NONE,
                OPEN,
                CLOSE,
            };

            // use the returned Group object and call end()
            Form::BaseForm &endGroup(const __FlashStringHelper *name, RenderType type);
            GroupType getGroupType(RenderType type);
            RenderType getEndGroupType(RenderType type);

        public:
            // --------------------------------------------------------------------
            // specialized add methds for ARM
            // --------------------------------------------------------------------

        #ifndef _MSC_VER
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wstrict-aliasing"
        #endif

        // For accessing unaligned member variables in packed structures
            template<typename ObjType, typename VarType, typename MemberVarType = stdex::member_pointer_value_t<VarType ObjType::*>>
            FormValueCallback<MemberVarType> &addMemberVariable(const __FlashStringHelper *name, ObjType &obj, VarType ObjType:: *memberPtr, InputFieldType type = InputFieldType::TEXT) {
                static_assert(std::is_trivially_copyable<MemberVarType>::value, "only for TriviallyCopyable");
                auto objectBegin = reinterpret_cast<uint8_t *>(std::addressof(obj));
                auto valuePtr = (MemberVarType *)&objectBegin[*(uintptr_t *)&memberPtr];
                return addPointerTriviallyCopyable<MemberVarType>(name, valuePtr, type);
            }

            // use addMemberVariable for packed and unaligned structures
            template<typename VarType>
            FormValueCallback<VarType> &addReference(const __FlashStringHelper *name, VarType &valueRef, InputFieldType type = InputFieldType::TEXT) {
                static_assert(std::is_trivially_copyable<VarType>::value, "only for TriviallyCopyable");
                return addPointerTriviallyCopyable<VarType>(name, reinterpret_cast<VarType *>(std::addressof(valueRef)), type);
            }

            // data is copied with memcpy to avoid alignment issues when reading from/writing to unaligned/packed structures
            // only for TriviallyCopyable
            template<typename VarType>
            FormValueCallback<VarType> &addPointerTriviallyCopyable(const __FlashStringHelper *name, VarType *valuePtr, InputFieldType type = InputFieldType::TEXT) {
                static_assert(std::is_trivially_copyable<VarType>::value, "only for TriviallyCopyable");
                auto bytePtr = reinterpret_cast<uint8_t *>(valuePtr);
                return _add<FormValueCallback<VarType>>(name, [bytePtr](VarType &value, Field::BaseField &field, bool store) {
                    // __LDBG_printf("size=%u ptr=%p align=%u", sizeof(VarType), bytePtr, ((intptr_t)bytePtr) % 4);
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
            Field::ValueCallback<VarType> &addCallbackGetterSetter(const __FlashStringHelper *name, typename Field::ValueCallback<VarType>::GetterSetterCallback callback, InputFieldType type = InputFieldType::SELECT) {
                return _add<Field::ValueCallback<VarType>>(name, callback, type);
            }

            template <typename VarType>
            Field::ValueCallback<VarType> &addCallbackSetter(const __FlashStringHelper *name, VarType value, typename Field::ValueCallback<VarType>::SetterCallback callback, InputFieldType type = InputFieldType::SELECT) {
                return _add<Field::ValueCallback<VarType>>(name, value, callback, type);
            }

            template<class _Ta, class ...Args>
            _Ta &_add(const __FlashStringHelper *name, Args &&...args) {
                return reinterpret_cast<_Ta &>(__add(new _Ta(_strings.attachString(name), std::forward<Args>(args)...)));
            }

        protected:
            inline Field::BaseField &__add(Field::BaseField *field)
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


        protected:

            void _addError(Field::BaseField *field, const String &message) {
                if (_errors == nullptr) {
                    _errors = new Error::Vector();
                }
                _errors->emplace_back(field, message);
            }

            void _clearErrors() {
                if (_errors) {
                    delete _errors;
                    _errors = nullptr;
                }
            }

        protected:
            inline bool _hasValidators() const {
                return _validators != nullptr;
            }
            Validator::BaseValidator *_validatorFind(Validator::BaseValidator *iterator, Field::BaseField *field);
            Validator::BaseValidator *_validatorFindNext(Validator::BaseValidator *iterator, Field::BaseField *field);
            void _addValidator(Validator::BaseValidator *validator);
            void _clearValidators();

        protected:
            Data *_data;
            Field::Vector _fields;
            Error::Vector *_errors;
            Validator::BaseValidator *_validators;
            bool _invalidMissing: 1;
            bool _hasChanged: 1;
            WebUI::Config *_uiConfig;
#if KFC_FORMS_HAVE_VALIDATE_CALLBACK
            Validator::Callback _validateCallback;
#endif
            StringDeduplicator _strings;
#if DEBUG_KFC_FORMS
            MicrosTimer _duration;
#endif
        };

    }

}

#include <debug_helper_disable.h>
