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
		SELECT,
		TEXT,
		PASSWORD,
		NEW_PASSWORD,
		GROUP_START,
		GROUP_END,
		GROUP_START_DIV,
		GROUP_END_DIV,
		HIDDEN,
	} TypeEnum_t;

	typedef std::pair<String, String> ItemPair;
	typedef std::vector<ItemPair> ItemsList;

	FormUI(TypeEnum_t type);
	FormUI(TypeEnum_t type, const String &label);

	FormUI *setLabel(const String &label, bool raw = true); // raw=false automatically adds ":" if the label doesn't have a trailing colon and isn't empty

	FormUI *setBoolItems();
	FormUI *setBoolItems(const String &enabled, const String &disabled);
	FormUI *addItems(const String &value, const String &label);
	FormUI *addItems(const ItemsList &items);
	FormUI *setSuffix(const String &suffix);
	FormUI *setPlaceholder(const String &placeholder);
	FormUI *addAttribute(const String &name, const String &value);
	FormUI *addConditionalAttribute(bool cond, const String &name, const String &value);
	FormUI *setReadOnly();

	void html(PrintInterface &output);

	void setParent(FormField *field);

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
