/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#if !_MSC_VER
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#include <Arduino_compat.h>
#include <vector>
#include <PrintArgs.h>
#include "FormUIConfig.h"

#if _MSC_VER
#define FORMUI_CRLF "\n"
#else
#define FORMUI_CRLF ""
#endif

#include "FormUIStorage.h"

namespace FormUI {

	class UI {
	public:
		using Type = ::FormUI::Type;

		template <typename... Args>
		UI(FormField *parent, Args &&... args) : _parent(parent), _type(Type::TEXT)
		{
			_addAll(args...);
		}

		UI(UI &&ui) noexcept : _parent(ui._parent), _type(std::exchange(ui._type, Type::NONE)), _storage(std::move(ui._storage))
		{
		}

		UI &operator=(UI &&ui) noexcept {
			_parent = ui._parent;
			_type = std::exchange(ui._type, Type::NONE);
			_storage = std::move(ui._storage);
			return *this;
		}

		const __FlashStringHelper *kIconsNone = FPSTR(emptyString.c_str());
		static constexpr __FlashStringHelper *kIconsDefault = nullptr;

		// DEPRECATED METHODS

		UI &addItems(const ItemsList &items)  __attribute__((deprecated))
		{
			_setItems(items);
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

		inline const char *attachString(const char *str) {
			return __FormFieldAttachString(_parent, str);
		}

		inline const char *attachString(const __FlashStringHelper *fpstr) {
			return attachString((PGM_P)fpstr);
		}

		inline const char *attachString(const String &str) {
			return attachString(str.c_str());
		}

		inline const char *encodeHtmlEntities(const char *str) {
			return __FormFieldEncodeHtmlEntities(_parent, str);
		}

		inline const char *encodeHtmlEntities(const __FlashStringHelper *fpstr) {
			return encodeHtmlEntities((PGM_P)fpstr);
		}

		inline const char *encodeHtmlEntities(const String &str) {
			return encodeHtmlEntities(str.c_str());
		}

		inline const char *encodeHtmlAttribute(const char *str) {
			return __FormFieldEncodeHtmlAttribute(_parent, str);
		}

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
			_storage.push_back(ItemsStorage::Label(encodeHtmlEntities(label)));
		}

		inline void _addItem(const Label &label) {
			_storage.push_back(ItemsStorage::Label(encodeHtmlEntities(label.getValue())));
		}

		inline void _addItem(const RawLabel &label) {
			_storage.push_back(ItemsStorage::LabelRaw(attachString(label.getValue())));
		}

		inline void _addItem(const Suffix &suffix) {
			_storage.push_back(ItemsStorage::SuffixText(encodeHtmlEntities(suffix.getValue())));
		}

		void _addItem(const CheckboxButtonSuffix &suffix);

		inline void _addItem(const IntMinMax &minMax) {
			_storage.push_back(ItemsStorage::AttributeMinMax(minMax._min, minMax._max));
		}

		bool __isDisabledAttribute(const char *key, const char *value) {
			auto disabled = PSTR("disabled");
			// with PROGMEM the linker should have deduplicated the strings already and we get the same pointers
			return ((key == disabled || strcmp_P_P(key, disabled) == 0) && (value == disabled || strcmp_P_P(value, disabled) == 0));
		}

		void _addItem(const StringAttribute &attribute);
		void _addItem(const FPStringAttribute &attribute);

		inline void _addItem(const ItemsList &items) {
			_setItems(items);
			_type = Type::SELECT;
		}

		inline void _addItem(const BoolItems &boolItems) {
			_storage.push_back(ItemsStorage::OptionNumKey(0, encodeHtmlEntities(boolItems._false)));
			_storage.push_back(ItemsStorage::OptionNumKey(1, encodeHtmlEntities(boolItems._true)));
			_type = Type::SELECT;
		}

		template<typename T>
		void _addItem(const Conditional<T> &conditional) {
			if (conditional._condition) {
				_addItem(conditional._value);
			}
		}

		void _addAll() {
		}

		template <typename T>
		void _addall(const T &t) {
			_addItem(t);
		}

		template <typename T, typename... Args>
		void _addAll(const T &t, Args &&... args) {
			_addItem(t);
			_addAll(args...);
		}

#if 0
		template <typename T>
		void _addall(T &&t) {
			_addItem(std::move(t));
		}

		template <typename T, typename... Args>
		void _addAll(T &&t, Args &&... args) {
			_addItem(std::move(t));
			_addAll(args...);
		}
#endif

	private:
		friend Form;

		const char *_attachMixedContainer(const MixedContainer &container, AttachStringAsType type) {
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
		void _setItems(const ItemsList &items);

        bool _hasLabel() const;
		bool _hasSuffix() const;
        bool _hasAttributes() const;

		void _printAttributeTo(PrintInterface &output) const;
        void _printSuffixTo(PrintInterface &output) const;
        void _printLabelTo(PrintInterface &output, const char *forLabel) const;
        void _printOptionsTo(PrintInterface &output) const;

	public:
		template<typename _Ta>
		void for_each(_Ta func) {
			for (auto iterator = _storage.begin(); iterator != _storage.end(); iterator = ItemsStorage::PrintValue::advance(iterator)) {
				func(iterator);
			}
		}

	private:
		friend ItemsStorage;

		StringDeduplicator &strings();

	private:
		FormField *_parent;
		Type _type;
		ItemsStorage _storage;
	};

};

#if !_MSC_VER
#pragma GCC diagnostic pop
#endif

