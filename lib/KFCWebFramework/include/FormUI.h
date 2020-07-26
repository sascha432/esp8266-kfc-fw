/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <vector>
#include <PrintArgs.h>
#include "../src/generated/FlashStringGeneratorAuto.h"

#if _MSC_VER
#define FORMUI_CRLF "\n"
#else
#define FORMUI_CRLF ""
#endif

class Form;
class FormField;

class FormUI {
public:
	using PrintInterface = PrintArgs::PrintInterface;

	typedef enum {
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
		GROUP_START,
		GROUP_END,
		GROUP_START_DIV,
		GROUP_END_DIV,
		GROUP_START_HR,
		GROUP_END_HR,
		HIDDEN,
	} TypeEnum_t;
	using Type = TypeEnum_t;

	typedef std::pair<String, String> ItemPair;
	typedef std::vector<ItemPair> ItemsList;

	FormUI(Type type);
	FormUI(Type type, const String &label);

	FormUI *setLabel(const String &label, bool raw = true); // raw=false automatically adds ":" if the label doesn't have a trailing colon and isn't empty

	// defaults Enabled/Disabled
	FormUI *setBoolItems();
	// custom text
	FormUI *setBoolItems(const String &enabled, const String &disabled);
	FormUI *addItems(const String &value, const String &label);
	FormUI *addItems(const ItemsList &items);
	// add input-append-group text or html if the string starts with <
	FormUI *setSuffix(const String &suffix);
	FormUI *setPlaceholder(const String &placeholder);
	// add min and max attribute
	FormUI *setMinMax(const String &min, const String &max);
	FormUI *addAttribute(const String &name, const String &value);
	FormUI *addConditionalAttribute(bool cond, const String &name, const String &value);
	FormUI *setReadOnly();

	void html(PrintInterface &output);

	void setParent(FormField *field);
	Type getType() const {
		return _type;
	}


private:
	bool _compareValue(const String &value) const;

	// creates an encoded string that is attached to output
	static const char *_encodeHtmlEntities(const char *str, bool attribute, PrintInterface &output);
	static const char *_encodeHtmlEntities(const String &str, bool attribute, PrintInterface &output) {
		return _encodeHtmlEntities(str.c_str(), attribute, output);
	}
	// // encodes str
	// static void _encodeHtmlEntitiesString(String &str);
	// copies an encoded version into target
	// static void _encodeHtmlEntitiesToString(const String &from, bool attribute, String &target);

private:
	friend Form;

	FormField *_parent;
	Type _type;
	String _label;
	String _suffix;
	String _attributes;
	ItemsList _items;
};
