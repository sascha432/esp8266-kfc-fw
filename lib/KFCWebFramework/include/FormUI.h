/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <vector>
#include <PrintArgs.h>

#if _MSC_VER
#define FORMUI_CRLF "\n"
#else
#define FORMUI_CRLF ""
#endif

#undef DEFAULT
#undef min
#undef max
#undef _min
#undef _max

class Form;
class FormField;

namespace FormUI {

	using ItemPair = std::pair<String, String>;
	using ItemsListVector = std::vector<ItemPair>;

	enum class Type : uint8_t {
		NONE,
		SELECT,
		TEXT,
		NUMBER,
		INTEGER,
		FLOAT,
		RANGE,
		RANGE_SLIDER,
		PASSWORD,
		NEW_PASSWORD,
		// VISIBLE_PASSWORD,
		GROUP_START,
		GROUP_END,
		GROUP_START_DIV,
		GROUP_END_DIV,
		GROUP_START_HR,
		GROUP_END_HR,
		GROUP_START_CARD,
		GROUP_END_CARD,
		HIDDEN,
	};

    enum class StyleType : uint8_t {
        DEFAULT = 0,
        ACCORDION,
        MAX
    };

	class ItemsList : public ItemsListVector {
	public:
		ItemsList() : ItemsListVector() {}

		// pass key value pairs as arguments
		// everything that can be converted to String() including enum class
		template <typename... Args>
		ItemsList(Args &&... args) : ItemsListVector() {
			static_assert(sizeof ... (args) % 2 == 0, "invalid number of pairs");
			reserve((sizeof ... (args)) / 2);
			_addAll(args...);
		}

		template <typename Ta, typename Tb>
		void push_back(const Ta &key, const Tb &val) {
			ItemsListVector::emplace_back(_enumConverter(key), _enumConverter(val));
		}

		template <typename Ta, typename Tb>
		void emplace_back(Ta &&key, Tb &&val) {
			ItemsListVector::emplace_back(std::move(_enumConverter(key)), std::move(_enumConverter(val)));
		}

		void dump(Print &output) {
			for (const auto &item : *this) {
				output.print(item.first);
				output.print('=');
				output.println(item.second);
			}
		}

	private:
		void _addAll() {
		}

		template <typename T, typename std::enable_if<std::is_enum<T>::value, int>::type = 0>
		String _enumConverter(const T &t) {
			return String(static_cast<typename std::underlying_type<T>::type>(t));
		}

		template <typename T, typename std::enable_if<!std::is_enum<T>::value, int>::type = 0>
		String _enumConverter(const T &t) {
			return String(t);
		}

		template <typename T>
		T &&_enumConverter(T &&t) {
			return t;
		}

		template <typename Ta, typename Tb, typename... Args>
		void _push(const Ta &key, const Tb &val, Args &&... args) {
			push_back(key, val);
			_addAll(args...);
		}

		template <typename T, typename... Args>
		void _addAll(const T &t, Args &&... args) {
			_push(t, args...);
		}

	};


	class UI;

	class Label : public String
	{
	public:
		Label() : String() {}

		// adds ":" at the end if not found and encodes html entities
		Label(const String &label) : Label(label, false) {}

		Label(const __FlashStringHelper *label, bool raw = false) : Label(String(label), raw) {}

		// if raw is set to true, the output is not modified
		Label(const String &label, bool raw) : String(raw ? String('\xff') : label) {
			if (raw) {
				*this += label;
			}
			if (String_endsWith(*this, ':')) {
				__DBG_printf("Label '%s' ends with ':'", label.c_str());
			}
		}

	};

	class Suffix : public String {
	public:
		Suffix(char suffix) : String(suffix) {}
		Suffix(const String &suffix) : String(suffix) {}
	};

	class PlaceHolder : public String {
	public:
		PlaceHolder(int placeholder) : String(placeholder) {}
		PlaceHolder(double placeholder, uint8_t digits) : String(placeholder, digits) {}
		PlaceHolder(const String &placeholder) : String(placeholder) {}
	};

	// class InputGroupAppend {
	// public:
	// 	enum class Type {
	// 		BUTTON_CHECKBOX,
	// 	};

	// 	InputGroupAppend(Type type, const String &onIcon, const String off) : _type(type) {
	// 	}

	// private:
	// 	Type _type;
	// };

	class MinMax
	{
	public:
		MinMax(int min, int max) : _min(min), _max(max) {}
		MinMax(const String &min, const String &max) : _min(min), _max(max) {}

	private:
		friend UI;

		String _min;
		String _max;
	};

	class Attribute
	{
	public:
		Attribute(const String &key, const String &value) : _key(key), _value(value) {}

	private:
		friend UI;

		String _key;
		String _value;
	};

	class ReadOnly : public Attribute
	{
	public:
		ReadOnly() : Attribute(FSPGM(readonly), emptyString) {}
	};

	class BoolItems
	{
	public:
		BoolItems() : _items(0, FSPGM(Disabled), 1, FSPGM(Enabled)) {}
		BoolItems(const String &_true, const String &_false) : _items(0, _false, 1, _true) {}

	private:
		friend UI;
		ItemsList _items;
	};

	template<class T>
	class Conditional {
	public:
		Conditional(bool condition, const T &value) : _value(value), _condition(condition) {}

	private:
		friend UI;

		T _value;
		bool _condition: 1;
	};

	class ConditionalAttribute : public Conditional<Attribute> {
	public:
		ConditionalAttribute(bool condition, const String &key, const String &value) : Conditional<Attribute>(condition, Attribute(key, value)) {}
	};

	class UI {
	public:
		using PrintInterface = PrintArgs::PrintInterface;
		using Type = ::FormUI::Type;
		// using InputGroupAppendVector = std::vector<InputGroupAppend>;

		// arguments see Form::addFormUI()

		template <typename... Args>
		UI(Args &&... args) : _parent(nullptr), _type(Type::TEXT) {
			_addAll(args...);
		}

		// DEPRECATED METHODS

		UI *setLabel(const String &label, bool raw = true) __attribute__ ((deprecated)) {
			return setLabel(Label(label, raw));
		}
		UI *setLabel(const Label &label) __attribute__ ((deprecated)) {
			_label = label;
			return this;
		}
		// defaults Enabled/Disabled
		UI *setBoolItems();
		// custom text
		UI *setBoolItems(const String &enabled, const String &disabled);
		UI *addItems(const String &value, const String &label);
		UI *addItems(const ItemsList &items);
		// add input-append-group text or html if the string starts with <
		UI *setSuffix(const String &suffix);
		UI *setPlaceholder(const String &placeholder);
		// add min and max attribute
		UI *setMinMax(const String &min, const String &max);
		UI *addAttribute(const String &name, const String &value);
		UI *addAttribute(const __FlashStringHelper *name, const String &value);
		UI *addConditionalAttribute(bool cond, const String &name, const String &value);
		UI *setReadOnly();

		void html(PrintInterface &output);

		void setParent(FormField *field);
		Type getType() const {
			return _type;
		}

	private:

		void _addItem(Type type) {
			_type = type;
		}

		void _addItem(const __FlashStringHelper *label) {
			_label = Label(label);
		}

		void _addItem(const Label &label) {
			_label = label;
		}

		void _addItem(const Suffix &suffix) {
			_suffix = suffix;
		}

		void _addItem(const PlaceHolder &placeholder) {
			addAttribute(FSPGM(placeholder), placeholder);
		}

		void _addItem(const MinMax &minMax) {
			addAttribute(FSPGM(min), minMax._min);
			addAttribute(FSPGM(max), minMax._max);
		}

		void _addItem(const Attribute &attribute) {
			addAttribute(attribute._key, attribute._value);
		}

		void _addItem(const ItemsList &items) {
			_items = items;
			_type = Type::SELECT;
		}

		void _addItem(const BoolItems &boolItems) {
			_items = boolItems._items;
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
		void _addall(T &&t) {
			_addItem(t);
		}

		template <typename T, typename... Args>
		void _addAll(T &&t, Args &&... args) {
			_addItem(t);
			_addAll(args...);
		}

	private:
		bool _compareValue(const String &value) const;

		friend Form;

		// creates an encoded string that is attached to output
		static const char *_encodeHtmlEntities(const char *str, bool attribute, PrintInterface &output);
		static const char *_encodeHtmlEntities(const String &str, bool attribute, PrintInterface &output) {
			return _encodeHtmlEntities(str.c_str(), attribute, output);
		}

	private:
		FormField *_parent;
		Type _type;
		String _label;
		String _suffix;
		String _attributes;
		ItemsList _items;
		// InputGroupAppendVector _inputGroupAppend;
	};


};

#include "FormUIConfig.h"
