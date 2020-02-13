/**
* Author: sascha_lammers@gmx.de
*/

#include "FormUI.h"
#include "FormField.h"

FormUI::FormUI(TypeEnum_t type) : _parent(nullptr), _type(type)
{
}

FormUI::FormUI(TypeEnum_t type, const String &label) : FormUI(type)
{
    setLabel(label, false);
}

FormUI *FormUI::setLabel(const String &label, bool raw)
{
    _label = label;
    if (!raw && _label.length() && _label.charAt(_label.length() - 1) != ':') {
        _label += ':';
    }
    return this;
}

FormUI *FormUI::setBoolItems(const String &yes, const String &no)
{
    addItems(F("0"), no);
    addItems(F("1"), yes);
    return this;
}

FormUI *FormUI::addItems(const String &value, const String &label)
{
    _items.push_back(std::make_pair(value, label));
    return this;
}

FormUI *FormUI::addItems(const ItemsList &items)
{
    _items = items;
    return this;
}

FormUI *FormUI::setSuffix(const String &suffix)
{
    _suffix = suffix;
    return this;
}

FormUI *FormUI::setPlaceholder(const String &placeholder)
{
    addAttribute(F("placeholder"), placeholder);
    return this;
}

FormUI *FormUI::addAttribute(const String &name, const String &value)
{
    _attributes += ' ';
    _attributes += name;
    _attributes += '=';
    _attributes += '"';
    _attributes += value; // TODO encode
    _attributes += '"';
    return this;
}

void FormUI::html(PrintInterface &output)
{
    // TODO check html entities encoding
    auto name = _parent->getName().c_str();

    output.printf_P(PSTR("<div class=\"form-group\"><label for=\"%s\">%s</label>"), name, _label.c_str());

    if (_suffix.length()) {
        output.printf_P(PSTR("<div class=\"input-group\">"));
    }

    switch (_type) {
    case SELECT:
        output.printf_P(PSTR("<select class=\"form-control\" name=\"%s\" id=\"%s\"%s>"), name, name, _attributes.c_str());
        for (auto &item : _items) {
            PGM_P selected = _compareValue(item.first) ? PSTR(" selected") : PSTR("");
            output.printf_P(PSTR("<option value=\"%s\"%s>%s</option>"), item.first.c_str(), selected, item.second.c_str());
        }
        output.printf_P(PSTR("</select>"));
        break;
    case TEXT:
        output.printf_P(PSTR("<input type=\"text\" class=\"form-control\" name=\"%s\" id=\"%s\" value=\"%s\"%s>"), name, name, _parent->getValue().c_str(), _attributes.c_str());
        break;
    case PASSWORD:
        output.printf_P(PSTR("<input type=\"password\" class=\"form-control\" name=\"%s\" id=\"%s\" spellcheck=\"off\"%s>"), name, name, _attributes.c_str());
        break;
    case NEW_PASSWORD:
        output.printf_P(PSTR("<input type=\"password\" class=\"form-control\" name=\"%s\" id=\"%s\" autocomplete=\"new-password\" data-always-visible=\"false\" data-protected=\"true\"%s>"), name, name, _attributes.c_str());
        break;
    }

    if (_suffix.length()) {
        output.printf_P(PSTR("<div class=\"input-group-append\"><span class=\"input-group-text\">%s</span></div></div>"), _suffix.c_str());
    }

    output.printf_P(PSTR("</div>"));
}

void FormUI::setParent(FormField *field)
{
    _parent = field;
}

bool FormUI::_compareValue(const String &value) const
{
    if (_parent->getType() == FormField::InputFieldType::TEXT) {
        return value.equals(_parent->getValue());
    }
    else {
        return value.toInt() == _parent->getValue().toInt();
    }
}
