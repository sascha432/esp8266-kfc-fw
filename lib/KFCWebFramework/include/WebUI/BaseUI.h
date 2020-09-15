/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

//#if !_MSC_VER
//#pragma GCC diagnostic push
//#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
//#endif

#include <Arduino_compat.h>
#include <vector>
#include <PrintArgs.h>
#include "WebUI/Config.h"
#include "WebUI/Storage.h"
#include "WebUI/Containers.h"

namespace FormUI {

    namespace WebUI {

        class BaseUI {
        public:
            using Type = WebUI::Type;

            const __FlashStringHelper *kIconsNone = FPSTR(emptyString.c_str());
            static constexpr __FlashStringHelper *kIconsDefault = nullptr;

            template <typename... Args>
            BaseUI(Field::BaseField *parent, Args &&... args) :
                _parent(parent),
                _type(Type::TEXT)
            {
                _addAll(args...);
            }

            BaseUI(BaseUI &&ui) noexcept :
                _parent(ui._parent),
                _type(std::exchange(ui._type, Type::NONE)),
                _storage(std::move(ui._storage))
            {
            }

            BaseUI &operator=(BaseUI &&ui) noexcept {
                _parent = ui._parent;
                _type = std::exchange(ui._type, Type::NONE);
                _storage = std::move(ui._storage);
                return *this;
            }

            void html(PrintInterface &output);

            inline Type getType() const {
                return _type;
            }

        private:
            inline void _addItem(Type type) {
                _type = type;
            }

            const char *attachString(const char *str);

            inline const char *attachString(const __FlashStringHelper *fpstr) {
                return attachString((PGM_P)fpstr);
            }

            inline const char *attachString(const String &str) {
                return attachString(str.c_str());
            }

            const char *encodeHtmlEntities(const char *str);

            inline const char *encodeHtmlEntities(const __FlashStringHelper *fpstr) {
                return encodeHtmlEntities((PGM_P)fpstr);
            }

            inline const char *encodeHtmlEntities(const String &str) {
                return encodeHtmlEntities(str.c_str());
            }

            const char *encodeHtmlAttribute(const char *str);

            inline const char *encodeHtmlAttribute(const __FlashStringHelper *fpstr) {
                return encodeHtmlAttribute((PGM_P)fpstr);
            }

            inline const char *encodeHtmlAttribute(const String &str) {
                return encodeHtmlAttribute(str.c_str());
            }

            enum class AttachStringAsType : uint8_t {
                STRING,
                HTML_ATTRIBUTE,
                HTML_ENTITIES
            };

            template<typename _Ta>
            inline const char *attachStringAs(_Ta str, AttachStringAsType type) {
                switch (type) {
                case AttachStringAsType::STRING:
                    return attachString(str);
                case AttachStringAsType::HTML_ATTRIBUTE:
                    return encodeHtmlAttribute(str);
                case AttachStringAsType::HTML_ENTITIES:
                    return encodeHtmlEntities(str);
                }
                return attachString(emptyString);
            }

            inline void _addItem(const __FlashStringHelper *label) {
                _storage.push_back(Storage::Value::Label(encodeHtmlEntities(label)));
            }

            inline void _addItem(const Container::Label &label) {
                _storage.push_back(Storage::Value::Label(encodeHtmlEntities(label.getValue())));
            }

            inline void _addItem(const Container::RawLabel &label) {
                _storage.push_back(Storage::Value::LabelRaw(attachString(label.getValue())));
            }

            inline void _addItem(const Container::Suffix &suffix) {
                _storage.push_back(Storage::Value::SuffixText(encodeHtmlEntities(suffix.getValue())));
            }

            void _addItem(const Container::CheckboxButtonSuffix &suffix);

            inline void _addItem(const Container::IntMinMax &minMax) {
                _storage.push_back(Storage::Value::AttributeMinMax(minMax._minValue, minMax._maxValue));
            }

            bool _isDisabledAttribute(const char *key, const char *value) {
                auto disabled = PSTR("disabled");
                // with PROGMEM the linker should have deduplicated the strings already and we get the same pointers
                return ((key == disabled || strcmp_P_P(key, disabled) == 0) && (value == disabled || strcmp_P_P(value, disabled) == 0));
            }

            void _checkDisableAttribute(const char *key, const char *value);

            void _addItem(const Container::StringAttribute &attribute);
            void _addItem(const Container::FPStringAttribute &attribute);

            inline void _addItem(const Container::List &items) {
                _setItems(items);
                _type = Type::SELECT;
            }

            inline void _addItem(const Container::BoolItems &boolItems) {
                _storage.push_back(Storage::Value::OptionNumKey(0, encodeHtmlEntities(boolItems._false)));
                _storage.push_back(Storage::Value::OptionNumKey(1, encodeHtmlEntities(boolItems._true)));
                _type = Type::SELECT;
            }

            template<typename _Ta>
            void _addItem(const Container::Conditional<_Ta> &conditional) {
                if (conditional._condition) {
                    _addItem(conditional._value);
                }
            }

            void _addAll() {
            }

            template <typename _Ta>
            void _addall(const _Ta &t) {
                _addItem(t);
            }

            template <typename _Ta, typename... Args>
            void _addAll(const _Ta &t, Args &&... args) {
                _addItem(t);
                _addAll(args...);
            }

    #if 0
            template <typename _Ta>
            void _addall(_Ta &&t) {
                _addItem(std::move(t));
            }

            template <typename _Ta, typename... Args>
            void _addAll(_Ta &&t, Args &&... args) {
                _addItem(std::move(t));
                _addAll(args...);
            }
    #endif

        private:
            friend Form::BaseForm;

            const char *_attachMixedContainer(const Container::Mixed &container, AttachStringAsType type) {
                if (container.isFPStr()) {
                    return attachStringAs((const char *)container.getFPString(), type);
                }
                else if (container.isStringPtr()) {
                    return attachStringAs(*container.getStringPtr(), type);
                }
                else if (container.isCStr()) {
                    return attachStringAs(container.getCStr(), type);
                }
                return attachStringAs(container.getString(), type);
            }

            bool _isSelected(int32_t value) const;
            bool _compareValue(const char *value) const;
            void _setItems(const Container::List &items);

            bool _hasLabel() const;
            bool _hasSuffix() const;
            bool _hasAttributes() const;

            void _printAttributeTo(PrintInterface &output) const;
            void _printSuffixTo(PrintInterface &output) const;
            void _printLabelTo(PrintInterface &output, const char *forLabel) const;
            void _printOptionsTo(PrintInterface &output) const;

        private:
            friend Storage::Vector;

            StringDeduplicator &strings();

        private:
            Field::BaseField *_parent;
            Type _type;
            Storage::Vector _storage;
        };

    }

};

