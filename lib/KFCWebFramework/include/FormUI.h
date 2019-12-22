/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <vector>

class FormField;

class FormUI {
public:
	typedef enum {
		SELECT,
		TEXT,
		PASSWORD,
		NEW_PASSWORD,
	} TypeEnum_t;

	typedef std::pair<String, String> ItemPair;
	typedef std::vector<ItemPair> ItemsList;

	FormUI(TypeEnum_t type);
	FormUI(TypeEnum_t type, const String &label);

	FormUI *setLabel(const String &label, bool raw = true); // raw=false automatically adds ":" if the label doesn't have a trailing colon and isn't empty

	FormUI *setBoolItems(const String &yes, const String &no);
	FormUI *addItems(const String& value, const String& label);
	FormUI *addItems(const ItemsList &items);
	FormUI *setSuffix(const String &suffix);

	void html(Print& output);

	void setParent(FormField* field);

private:
	bool _compareValue(const String& value) const;

private:
	FormField *_parent;
	TypeEnum_t _type;
	String _label;
	String _suffix;
	ItemsList _items;
};
