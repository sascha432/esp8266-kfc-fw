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

class Form;
class FormField;

namespace FormUI {

	using ItemPair = std::pair<String, String>;
	using ItemsListVector = std::vector<ItemPair>;

	class ItemsList : public ItemsListVector {
	public:
		ItemsList() : ItemsListVector() {}

		ItemsList &operator=(const ItemsList &items) {
			clear();
			reserve(items.size());
			for (const auto &item : items) {
				ItemsListVector::push_back(item);
			}
			return *this;
		}

		ItemsList &operator=(ItemsList &&items) noexcept {
			ItemsListVector::swap(items);
			return *this;
		}

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

		void emplace_back_pair(const String &key, const String &val) {
			ItemsListVector::emplace_back(key, val);
		}

		void emplace_back_pair(int key, int val) {
			ItemsListVector::emplace_back(std::move(String(key)), std::move(String(val)));
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

	using StringPairListVector = std::vector<std::pair<const char *, const char *>>;

	class StringPairList : public StringPairListVector
	{
	public:
		using StringPairListVector::StringPairListVector;

		StringPairList(Config &ui, const ItemsList &items);
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
			if (!raw && String_endsWith(*this, ':')) {
				__DBG_printf("Label '%s' ends with ':'", label.c_str());
			}
		}

	};

	class Suffix : public String {
	public:
		Suffix(char suffix) : String(suffix) {}
		Suffix(const String &suffix) : String(suffix) {}
		Suffix(String &&suffix) : String(std::move(suffix)) {}
		Suffix(const __FlashStringHelper *suffix) : String(suffix) {}
	};

	class ZeroconfSuffix : public Suffix {
	public:
		ZeroconfSuffix() : Suffix(F("<button type=\"button\" class=\"btn btn-default resolve-zerconf-button\" data-color=\"primary\">Resolve Zeroconf</button>")) {}
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
		Attribute(const __FlashStringHelper *key, const String &value) : _key(key), _value(value) {}

	private:
		friend UI;

		const __FlashStringHelper *_key;
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
		BoolItems() : _false(FSPGM(Disabled)), _true(FSPGM(Enabled)) {}
		BoolItems(const String &pTrue, const String &pFalse) : _false(pFalse), _true(pTrue) {}

	private:
		friend UI;
		String _false;
		String _true;
	};

	template<class T>
	class Conditional {
	public:
		Conditional(bool condition, const T &value) : _value(value), _condition(condition) {}

	private:
		friend UI;

		FormField *_parent;
		Type _type;
		String _label;
		String _suffix;
		String _attributes;
		ItemsList *_items;
		T _value;
		bool _condition: 1;
	};

	class ConditionalAttribute : public Conditional<Attribute> {
	public:
		ConditionalAttribute(bool condition, const __FlashStringHelper *key, const String &value) : Conditional<Attribute>(condition, Attribute(key, value)) {}
	};

	class UI {
	public:
		using PrintInterface = PrintArgs::PrintInterface;
		using Type = ::FormUI::Type;
		// using InputGroupAppendVector = std::vector<InputGroupAppend>;

		// arguments see Form::addFormUI()

		template <typename... Args>
		UI(FormField *parent, Args &&... args) : _parent(parent), _type(Type::TEXT), _label(nullptr), _suffix(nullptr), _items(nullptr) {
			_addAll(args...);
		}
		UI(UI &&ui) noexcept {
			*this = std::move(ui);
		}
		~UI() {
			if (_items) {
				delete _items;
			}
		}

		UI &operator=(UI &&ui) noexcept {
			_parent = ui._parent;
			_type = ui._type;
			_label = ui._label;
			_suffix = ui._suffix;
			_attributes = std::move(ui._attributes);
			_items = ui._items;
			ui._items = nullptr;
			return *this;
		}

		//// move vector instead of copying
		//UI &emplaceItems(ItemsList &&items) {
		//	_setItems(std::move(items));
		//	_type = Type::SELECT;
		//	return *this;
		//}

		const __FlashStringHelper *kIconsNone = FPSTR(emptyString.c_str());
		static constexpr __FlashStringHelper *kIconsDefault = nullptr;

		static Suffix createCheckBoxButton(FormField &hiddenField, const String &label, const __FlashStringHelper *onIcons = nullptr, const __FlashStringHelper *offIcons = nullptr);

		// DEPRECATED METHODS

		//UI *setLabel(const String &label, bool raw = true) __attribute__ ((deprecated)) {
		//	return setLabel(Label(label, raw));
		//}
		//UI *setLabel(const Label &label) __attribute__ ((deprecated)) {
		//	_label = label;
		//	return this;
		//}
		// defaults Enabled/Disabled
		UI *setBoolItems()  __attribute__ ((deprecated)) ;
		// custom text
		UI *setBoolItems(const String &enabled, const String &disabled)  __attribute__ ((deprecated)) ;
		//UI *addItems(const String &value, const String &label)  __attribute__ ((deprecated)) ;
		UI *addItems(const ItemsList &items)  __attribute__ ((deprecated)) ;
		// add input-append-group text or html if the string starts with <
		UI *setSuffix(const String &suffix)  __attribute__ ((deprecated)) ;
		UI *setPlaceholder(const String &placeholder)  __attribute__ ((deprecated)) ;
		// add min and max attribute
		UI *setMinMax(const String &min, const String &max)  __attribute__ ((deprecated)) ;
		UI *addAttribute(const __FlashStringHelper *name, const String &value);
		UI *addConditionalAttribute(bool cond, const __FlashStringHelper *name, const String &value)  __attribute__ ((deprecated)) ;
		UI *setReadOnly()  __attribute__ ((deprecated)) ;

		void html(PrintInterface &output);

		//void setParent(FormField *field);
		Type getType() const {
			return _type;
		}

	private:

		void _addItem(Type type);
		void _addItem(const __FlashStringHelper *label);
		void _addItem(const Label &label);
		void _addItem(const Suffix &suffix);
		void _addItem(const PlaceHolder &placeholder);
		void _addItem(const MinMax &minMax);
		void _addItem(const Attribute &attribute);
		void _addItem(const ItemsList &items);
		void _addItem(const BoolItems &boolItems);

		template<typename T>
		void _addItem(const Conditional<T> &conditional) {
			__DBG_printf("condition %u", conditional._condition);
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
		void _setItems(const ItemsList &items);
		//	if (_items) {
		//		delete _items;
		//	}
		//	auto &x = _parent->getFormUIConfig().strings();
		//	_items = new StringPairList(x, items);
		//	//if (!_items) {
		//	//	_items = new ItemsList(_strings, items);
		//	//}
		//	//*_items = items;
		//}
		//void _setItems(ItemsList &&items) {
		//	if (!_items) {
		//		_items = new ItemsList();
		//	}
		//	*_items = std::move(items);
		//}

	private:
		FormField *_parent;
		Type _type;
		const char *_label;
		const char *_suffix;
		String _attributes;
		StringPairList *_items;
		//ItemsList *_items;
	};


};

#include "FormUIConfig.h"

#if !_MSC_VER
#pragma GCC diagnostic pop
#endif

