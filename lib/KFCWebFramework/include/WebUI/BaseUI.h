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

#include "Utility/Debug.h"

namespace FormUI {

    namespace WebUI {

        class BaseUI {
        public:
            using Type = WebUI::Type;

            template <typename... Args>
            BaseUI(Field::BaseField *parent, Args &&... args) :
                _parent(parent),
                _type(Type::TEXT)
            {
#if DEBUG_STORAGE_VECTOR && 0
                size_t from = _storage.size();
                _addAll(args...);
                _storage.validate(from, parent);
#else
                _addAll(args...);
#endif
            }

            BaseUI(BaseUI &&ui) noexcept :
                _parent(ui._parent),
                _storage(std::move(ui._storage)),
                _type(std::exchange(ui._type, Type::NONE))
            {
            }

            BaseUI &operator=(BaseUI &&ui) noexcept {
                _parent = ui._parent;
                _storage = std::move(ui._storage);
                _type = std::exchange(ui._type, Type::NONE);
                return *this;
            }

            void html(PrintInterface &output);
            void renderInputField(Type type, PrintInterface &output, const char *name, const String &value);

            inline Type getType() const {
                return _type;
            }

            // if type is set to FormUI::Type::HIDDEN, addItems() won't change the value anymore
            // use setType() instead
            inline void setType(Type type) {
                _type = type;
            }

            void dump(size_t offset, Print &output) {
                _storage.dump(offset, output);
            }

            template<typename _Ta>
            void addItem(const _Ta &item) {
                _addItem(item);
            }

            template <typename... Args>
            void addItems(Args &&... args) {
                _addAll(args...);
            }

        public:
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

        private:
            inline void _addItem(Type type) {
                if (_type != Type::HIDDEN) {
                    _type = type;
                }
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

            inline void _addItem(const Container::SuffixHtml &suffix) {
                _storage.push_back(Storage::Value::SuffixHtml(attachString(suffix.getValue())));
            }

            void _addItem(const Container::CheckboxButtonSuffix &suffix);
            void _addItem(const Container::SelectSuffix &suffix);

            inline void _addItem(const Container::IntMinMax &minMax) {
                _storage.push_back(minMax.create());
            }

            template<typename _Ta>
            inline void _addItem(const Container::AttributeTmpl<_Ta> &attribute) {
                attribute.push_back(_storage, *this);
            }

            void _addItem(const Container::DisabledAttribute &attribute);

            inline void _addItem(const Container::List &items) {
                _setItems(items);
                _addItem(Type::SELECT);
            }

            template<typename _Ta, typename _Tb>
            void _addItem(const Container::Conditional<_Ta, _Tb> &conditional) {
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
            bool _compareTextValue(const char *value) const;
            bool _compareValue(const char *value) const;
            void _setItems(const Container::List &items);

            bool _hasLabel() const;
            bool _hasSuffix() const;
            bool _hasAttributes() const;

            const char *_getLabel() const;
            void _printAttributeTo(PrintInterface &output) const;
            void _printSuffixTo(PrintInterface &output) const;
            void _printLabelTo(PrintInterface &output, const char *forLabel) const;
            void _printOptionsTo(PrintInterface &output) const;

        private:
            friend Storage::Vector;

            StringDeduplicator &strings();

        private:
            Field::BaseField *_parent;
            Storage::Vector _storage;
            Type _type;
        };

    }

};

#include <debug_helper_disable.h>
