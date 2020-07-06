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
    addItems(String('0'), no);
    addItems(String('1'), yes);
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
    if (value.length()) {
        _attributes += '=';
        _attributes += '"';
        _attributes += value; // TODO encode
        _attributes += '"';
    }
    return this;
}

FormUI *FormUI::addConditionalAttribute(bool cond, const String &name, const String &value)
{
    if (cond) {
        return addAttribute(name, value);
    }
    return this;
}

FormUI *FormUI::setReadOnly()
{
    return addAttribute(F("readonly"), String());
}

void FormUI::html(PrintInterface &output)
{
    if (_type == GROUP_START) {
        FormGroup &group = reinterpret_cast<FormGroup &>(*_parent);
        auto id = _parent->getName().c_str();
        output.printf_P(PSTR("<div class=\"form-group\"><button class=\"btn btn-secondary btn-block\""));
        output.printf_P(PSTR(" type=\"button\" data-toggle=\"collapse\" data-target=\"#%s\" aria-expanded=\"false\" aria-controls=\"%s\">"), id, id);
        output.printf_P(PSTR("%s</button></div><div class=\"collapse%s\" id=\"%s\"><div class=\"card card-body mb-3\">" FORMUI_CRLF), _label.c_str(), group.isExpanded() ? F(".show") : F(""), id);
    }
    else if (_type == GROUP_END) {
        output.printf_P(PSTR("</div></div>" FORMUI_CRLF));
    }
    else if (_type == HIDDEN) {
        // TODO check html entities encoding
        auto name = _parent->getName().c_str();
        output.printf_P(PSTR("<input type=\"hidden\" name=\"%s\" id=\"%s\" value=\"%s\"%s>" FORMUI_CRLF), name, name, _parent->getValue().c_str(), _attributes.c_str());
    }
    else {

        // TODO check html entities encoding
        auto name = _parent->getName().c_str();
        output.printf_P(PSTR("<div class=\"form-group\"><label for=\"%s\">%s</label>" FORMUI_CRLF), name, _label.c_str());

        if (_suffix.length()) {
            output.printf_P(PSTR("<div class=\"input-group\">" FORMUI_CRLF));
        }

        switch (_type) {
        case SELECT:
            output.printf_P(PSTR("<select class=\"form-control\" name=\"%s\" id=\"%s\"%s>" FORMUI_CRLF), name, name, _attributes.c_str());
            for (auto &item : _items) {
                PGM_P selected = _compareValue(item.first) ? PSTR(" selected") : PSTR("");
                output.printf_P(PSTR("<option value=\"%s\"%s>%s</option>" FORMUI_CRLF), item.first.c_str(), selected, item.second.c_str());
            }
            output.printf_P(PSTR("</select>" FORMUI_CRLF));
            break;
        case TEXT:
            output.printf_P(PSTR("<input type=\"text\" class=\"form-control\" name=\"%s\" id=\"%s\" value=\"%s\"%s>" FORMUI_CRLF), name, name, _parent->getValue().c_str(), _attributes.c_str());
            break;
        case PASSWORD:
            output.printf_P(PSTR("<input type=\"password\" class=\"form-control\" name=\"%s\" id=\"%s\" spellcheck=\"off\"%s>" FORMUI_CRLF), name, name, _attributes.c_str());
            break;
        case NEW_PASSWORD:
            output.printf_P(PSTR("<input type=\"password\" class=\"form-control\" name=\"%s\" id=\"%s\" autocomplete=\"new-password\" data-always-visible=\"false\" data-protected=\"true\"%s>" FORMUI_CRLF), name, name, _attributes.c_str());
            break;
        case GROUP_START:
        case GROUP_END:
        case HIDDEN:
            break;
        }

        if (_suffix.length()) {
            output.printf_P(PSTR("<div class=\"input-group-append\"><span class=\"input-group-text\">%s</span></div></div>" FORMUI_CRLF), _suffix.c_str());
        }

        output.printf_P(PSTR("</div>" FORMUI_CRLF));
    }
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
