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

	typedef std::pair<String, String> ItemPair;
	typedef std::vector<ItemPair> ItemsList;

	FormUI(TypeEnum_t type);
	FormUI(TypeEnum_t type, const String &label);

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
	TypeEnum_t getType() const {
		return _type;
	}


private:
	bool _compareValue(const String &value) const;

private:
	friend Form;

	FormField *_parent;
	TypeEnum_t _type;
	String _label;
	String _suffix;
	String _attributes;
	ItemsList _items;
};
