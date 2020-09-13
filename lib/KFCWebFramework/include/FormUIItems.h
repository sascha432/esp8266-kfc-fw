/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <vector>
#include <PrintString.h>
#include "FormUIConfig.h"

class Form;
class FormField;
class PrintInterface;
class PrintHtmlEntitiesString;

namespace FormUI {

	const char *__FormFieldGetName(FormField &field);

	const char *__FormFieldAttachString(FormField *parent, const char *str);

	inline const char *__FormFieldAttachString(FormField *parent, const String &str) {
		return __FormFieldAttachString(parent, str.c_str());
	}

	inline const char *__FormFieldAttachString(FormField *parent, const __FlashStringHelper *fpstr) {
		return __FormFieldAttachString(parent, (PGM_P)fpstr);
	}

	const char *__FormFieldEncodeHtmlEntities(FormField *parent, const char *str);

	inline const char *__FormFieldEncodeHtmlEntities(FormField *parent, const String &str) {
		return __FormFieldEncodeHtmlEntities(parent, str.c_str());
	}

	inline const char *__FormFieldEncodeHtmlEntities(FormField *parent, const __FlashStringHelper *fpstr) {
		return __FormFieldEncodeHtmlEntities(parent, (PGM_P)fpstr);
	}

	const char *__FormFieldEncodeHtmlAttribute(FormField *parent, const char *str);

	inline const char *__FormFieldEncodeHtmlAttribute(FormField *parent, const String &str) {
		return __FormFieldEncodeHtmlAttribute(parent, str.c_str());
	}

	static inline const char *__FormFieldEncodeHtmlAttribute(FormField *parent, const __FlashStringHelper *fpstr) {
		return __FormFieldEncodeHtmlAttribute(parent, (PGM_P)fpstr);
	}

	class MixedStringContainer : public String
	{
	public:
		MixedStringContainer(const String &label) :
			String(label),
			_fpstr(nullptr)
		{
		}

		MixedStringContainer(String &&label) :
			String(std::move(label)),
			_fpstr(nullptr)
		{
		}

		MixedStringContainer(const __FlashStringHelper *label) :
			String(),
			_fpstr(label)
		{
		}

		const char *getValue() const {
			if (_fpstr) {
				return (const char *)_fpstr;
			}
			else {
				return c_str();
			}
		}

	private:
		const __FlashStringHelper *_fpstr;
	};

	#include <push_pack.h>

	class MixedContainer {
	public:
		enum class Type : uint8_t {
			NONE,
			CSTR,
			FPSTR,
			STRING,
			INT32,
			UINT32,
			DOUBLE,
			MAX = 15,
			ENCODE_HTML_ATTRIBUTE		= 0x40,
			ENCODE_HTML_ENTITIES		= 0x80
		};

		MixedContainer(const MixedContainer &copy) : _type(copy._type), _ptr(copy._ptr) {
			_copy(copy);
		}
		MixedContainer &operator=(const MixedContainer &copy) {
			_release();
			_type = copy._type;
			_ptr = copy._ptr;
			_copy(copy);
		}

		MixedContainer(MixedContainer &&move) noexcept : _type(std::exchange(move._type, Type::NONE)), _ptr(std::exchange(move._ptr, nullptr)) {}
		MixedContainer &operator=(MixedContainer &&move) noexcept
		{
			_release();
			_type = std::exchange(move._type, Type::NONE);
			_ptr = std::exchange(move._ptr, nullptr);
		}

		MixedContainer(const String &value) : _type(Type::STRING), _str(new String(value)) {}
		MixedContainer(String &&value) : _type(Type::STRING), _str(new String(std::move(value))) {}
		MixedContainer(double value) : _type(Type::DOUBLE), _double((float)value) {}
		MixedContainer(int32_t value) : _type(Type::INT32), _int(value) {}
		MixedContainer(uint32_t value) : _type(Type::UINT32), _uint(value) {}
		~MixedContainer() {
			_release();
		}

		inline void clear() {
			_release();
			_type = Type::NONE;
		}

		Type getType() const {
			return _mask(_type);
		}

		bool isInt() const {
			return getType() == Type::INT32 || getType() == Type::UINT32;
		}

		bool isFPStr() const {
			return getType() == Type::FPSTR;
		}

		bool isStringPtr() const {
			return getType() == Type::STRING;
		}

		bool isCStr() const {
			return getType() == Type::CSTR;
		}

		int32_t getInt() const {
			return _int;
		}

		uint32_t getUInt() const {
			return _uint;
		}

		const __FlashStringHelper *getFPString() const {
			return _fpStr;
		}

		String *getStringPtr() const {
			return _str;
		}

		const char *getCStr() const {
			return _cStr;
		}

		String getString() const {
			switch (getType()) {
			case Type::CSTR:
				return String(_cStr);
			case Type::STRING:
				return *_str;
			case Type::DOUBLE:
				return String(_double);
			case Type::INT32:
				return String(_int);
			case Type::UINT32:
				return String(_uint);
			case Type::FPSTR:
				return String(_fpStr);
			}
			return String();
		}

#if 0
		void dump(Print &output) const {
			if (_ptr == nullptr && getType() <= Type::STRING) {
				output.print(F("<NULL>"));
				return;
			}
			switch (getType()) {
			case Type::CSTR:
				output.print(_cStr);
				break;
			case Type::FPSTR:
				output.print(_fpStr);
				break;
			case Type::STRING:
				output.print(*_str);
				break;
			case Type::INT32:
				output.print(_int);
				break;
			case Type::UINT32:
				output.print(_uint);
				break;
			case Type::DOUBLE:
				output.print(_double);
				break;
			}
		}
#endif

	private:
		inline void _copy(const MixedContainer &copy) {
			if (copy._ptr) {
				switch (getType()) {
				case Type::CSTR:
					_cStr = strdup(copy._cStr);
					break;
				case Type::STRING:
					_str = new String(*copy._str);
					break;
				}
			}
		}

		inline void _release() {
			if (_ptr) {
				if (_isType(Type::CSTR)) {
					free(_ptr);
				}
				else if (_isType(Type::STRING)) {
					delete _str;
				}
			}
		}

		inline Type _mask(Type type) const {
			// not used
			//return static_cast<Type>(static_cast<uint8_t>(type) & 0x0f);
			return type;
		}
		inline bool _isType(Type type) const {
			return _mask(_type) == type;
		}

		union {
			float _double;
			int32_t _int;
			uint32_t _uint;
			const char *_cStr;
			const __FlashStringHelper *_fpStr;
			String *_str;
			void *_ptr;
		};
		Type _type;
	};

	using MixedContainerPair = std::pair<MixedContainer, MixedContainer>;

	static constexpr size_t MixedContainerSize = sizeof(MixedContainer);
	static constexpr size_t MixedContainerPairSize = sizeof(MixedContainerPair);

	#include <pop_pack.h>

	using ItemsListVector = std::vector<MixedContainerPair>;

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
		// supports: String, const FlashStringHelper *, int, unsigned, float, enum class with underlying type int/unsigned, int16_t, int8_t etc..
		template <typename... Args>
		ItemsList(Args &&... args) : ItemsListVector() {
			static_assert(sizeof ...(args) % 2 == 0, "invalid number of pairs");
			reserve((sizeof ...(args)) / 2);
			_addAll(args...);
		}

		template <typename _Ta, typename _Tb>
		void emplace_back(_Ta &&key, _Tb &&val) {
			ItemsListVector::emplace_back(std::move(_create(std::move(key))), std::move(_create(std::move(val))));
		}

		template <typename Ta, typename Tb>
		void push_back(const Ta &key, const Tb &val) {
			ItemsListVector::emplace_back(_passthrough(key), _passthrough(val));
		}

	private:
		template <typename _Ta, typename _Tb = std::relaxed_underlying_type_t<_Ta>, typename std::enable_if<std::is_enum<_Ta>::value, int>::type = 0>
		inline _Tb _passthrough(_Ta t) {
			return static_cast<_Tb>(t);
		}

		template <typename _Ta, typename std::enable_if<!std::is_enum<_Ta>::value, int>::type = 0>
		inline const _Ta &_passthrough(const _Ta &t) {
			return t;
		}

		template <typename _Ta, typename _Tb = std::relaxed_underlying_type_t<_Ta>, typename std::enable_if<std::is_enum<_Ta>::value, int>::type = 0>
		inline MixedContainer _create(_Ta t) {
			return MixedContainer(static_cast<_Tb>(t));
		}

		template <typename _Ta, typename std::enable_if<!std::is_enum<_Ta>::value, int>::type = 0>
		inline MixedContainer _create(const _Ta &t) {
			return MixedContainer(t);
		}

		template <typename _Ta, typename std::enable_if<!std::is_enum<_Ta>::value, int>::type = 0>
		inline MixedContainer _create(_Ta &&t) {
			return MixedContainer(std::move(t));
		}

#if 0
		void dump(Print &output) const {
			for (const auto &item : *this) {
				item.first.dump(output);
				output.print('=');
				item.second.dump(output);
				output.println();
			}
		}
#endif

		void _addAll() {
		}

		template <typename Ta, typename Tb, typename... Args>
		void _push(Ta &&key, Tb &&val, Args &&... args) {
			ItemsListVector::emplace_back(std::move(_create(std::move(key))), std::move(_create(std::move(val))));
			_addAll(args...);
		}

		template <typename T, typename... Args>
		void _addAll(T &&t, Args &&... args) {
			_push(std::move(t), args...);
		}

	};

	class UI;

	class Label : public MixedStringContainer
	{
	public:
		using MixedStringContainer::MixedStringContainer;
		using MixedStringContainer::getValue;
	};

	class RawLabel : public MixedStringContainer
	{
	public:
		using MixedStringContainer::MixedStringContainer;
		using MixedStringContainer::getValue;
	};

	class Suffix : public MixedStringContainer {
	public:
		using MixedStringContainer::MixedStringContainer;
		using MixedStringContainer::getValue;
	};

		inline Suffix ZeroconfSuffix() {
		return F("<button type=\"button\" class=\"btn btn-default resolve-zerconf-button\" data-color=\"primary\">Resolve Zeroconf</button>");
	}

	class CheckboxButtonSuffix {
	public:
		CheckboxButtonSuffix(FormField &hiddenField, const char *label, const __FlashStringHelper *onIcons = nullptr, const __FlashStringHelper *offIcons = nullptr);
		//{
		//	if (onIcons && offIcons) {
		//		_items.push_back(F("<button type=\"button\" class=\"btn btn-default button-checkbox\" data-on-icon=\"%s\" data-off-icon=\"%s\" id=\"_%s\">%s</button>"));
		//		_items.push_back(onIcons);
		//		_items.push_back(offIcons);
		//		_items.push_back(__FormFieldGetName(hiddenField));
		//		_items.push_back(label);
		//	}
		//	else {
		//		_items.push_back(F("<button type=\"button\" class=\"btn btn-default button-checkbox\" id=\"_%s\">%s</button>"));
		//		_items.push_back(onIcons);
		//		_items.push_back(offIcons);
		//		_items.push_back(__FormFieldGetName(hiddenField));
		//		_items.push_back(label);
		//	}
		//}
		CheckboxButtonSuffix(FormField &hiddenField, const String &label, const __FlashStringHelper *onIcons = nullptr, const __FlashStringHelper *offIcons = nullptr) :
			CheckboxButtonSuffix(hiddenField, label.c_str(), onIcons, offIcons)
		{
		}
		CheckboxButtonSuffix(FormField &hiddenField, const __FlashStringHelper *label, const __FlashStringHelper *onIcons = nullptr, const __FlashStringHelper *offIcons = nullptr) :
			CheckboxButtonSuffix(hiddenField, (const char *)label, onIcons, offIcons)
		{
		}

	private:
		friend UI;

		StringVector _items;
	};

	class IntMinMax
	{
	public:
		IntMinMax(int32_t __min, int32_t __max) : _min(__min), _max(__max) {}

	private:
		friend UI;

		int32_t _min;
		int32_t _max;
	};

	inline IntMinMax MinMax(int32_t aMin, int32_t aMax) {
		return IntMinMax(aMin, aMax);
	}

	class StringAttribute
	{
	public:
		StringAttribute(const __FlashStringHelper *key, const String &value) : _key(key), _value(value) {}
		StringAttribute(const __FlashStringHelper *key, String &&value) : _key(key), _value(std::move(value)) {}

	private:
		friend UI;

		const __FlashStringHelper *_key;
		String _value;
	};

	class FPStringAttribute
	{
	public:
		FPStringAttribute(const __FlashStringHelper *key, const __FlashStringHelper *value) : _key(key), _value(value) {}

	private:
		friend UI;

		const __FlashStringHelper *_key;
		const __FlashStringHelper *_value;
	};

	inline FPStringAttribute Attribute(const __FlashStringHelper *key, const __FlashStringHelper *value) {
		return FPStringAttribute(key, value);
	}

	inline StringAttribute Attribute(const __FlashStringHelper *key, const String &value) {
		return StringAttribute(key, value);
	}

	inline StringAttribute Attribute(const __FlashStringHelper *key, String &&value) {
		return StringAttribute(key, std::move(value));
	}


	inline StringAttribute PlaceHolder(int32_t placeholder) {
		return StringAttribute(F("placeholder"), String(placeholder));
	}

	inline StringAttribute PlaceHolder(uint32_t placeholder) {
		return StringAttribute(F("placeholder"), String(placeholder));
	}

	inline StringAttribute PlaceHolder(double placeholder, uint8_t digits) {
		return StringAttribute(F("placeholder"), String(placeholder, digits));
	}

	inline FPStringAttribute PlaceHolder(const __FlashStringHelper *placeholder) {
		return FPStringAttribute(F("placeholder"), placeholder);
	}

	inline StringAttribute PlaceHolder(const String &placeholder) {
		return StringAttribute(F("placeholder"), placeholder);
	}

	inline StringAttribute PlaceHolder(String &&placeholder) {
		return StringAttribute(F("placeholder"), std::move(placeholder));
	}

	class ReadOnly : public StringAttribute
	{
	public:
		ReadOnly() : StringAttribute(FSPGM(readonly), emptyString) {}
	};

	class BoolItems
	{
	public:
		BoolItems() : _false(FSPGM(Disabled)), _true(FSPGM(Enabled)) {}
		BoolItems(const String &pTrue, const String &pFalse) : _false(pFalse), _true(pTrue) {}
		BoolItems(String &&pTrue, String &&pFalse) : _false(std::move(pFalse)), _true(std::move(pTrue)) {}

	private:
		friend UI;
		String _false;
		String _true;
	};

	template<class T>
	class Conditional {
	public:
		Conditional(bool condition, const T &value) : _value(value), _condition(condition) {}
		Conditional(bool condition, T &&value) : _value(std::move(value)), _condition(condition) {}

	private:
		friend UI;

		T _value;
		bool _condition: 1;
	};

	class ConditionalAttribute : public Conditional<StringAttribute> {
	public:
		ConditionalAttribute(bool condition, const __FlashStringHelper *key, const String &value) : Conditional(condition, Attribute(key, value)) {}
		ConditionalAttribute(bool condition, const __FlashStringHelper *key, String &&value) : Conditional(condition, Attribute(key, std::move(value))) {}
	};

}
