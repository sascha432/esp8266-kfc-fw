/**
* Author: sascha_lammers@gmx.de
*/

#include "FormUI.h"
#include "FormField.h"
#include <PrintHtmlEntities.h>
#include <misc.h>

FormUI::FormUI(Type type) : _parent(nullptr), _type(type)
{
}

FormUI::FormUI(Type type, const String &label) : FormUI(type)
{
    setLabel(label, false);
}

FormUI *FormUI::setLabel(const String &label, bool raw)
{
    _label = label;
    if (!raw && !String_endsWith(label, ':')) {
        _label += ':';
    }
    return this;
}

FormUI *FormUI::setBoolItems()
{
    return setBoolItems(FSPGM(Enabled), FSPGM(Disabled));
}

FormUI *FormUI::setBoolItems(const String &enabled, const String &disabled)
{
    addItems(String('0'), disabled);
    addItems(String('1'), enabled);
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
    addAttribute(FSPGM(placeholder), placeholder);
    return this;
}

FormUI *FormUI::setMinMax(const String &min, const String &max)
{
    addAttribute(FSPGM(min), min);
    addAttribute(FSPGM(max), max);
    return this;
}

FormUI *FormUI::addAttribute(const String &name, const String &value)
{
    _attributes += ' ';
    _attributes += name;
    if (value.length()) {
        _attributes += '=';
        _attributes += '"';
         if (!PrintHtmlEntities::translateTo(value.c_str(), _attributes, true)) {
             _attributes += value;
         }
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
    return addAttribute(FSPGM(readonly), String());
}

const char *FormUI::_encodeHtmlEntities(const char *cStr, bool attribute, PrintInterface &output)
{
    if (pgm_read_byte(cStr) == 0xff) { // FORM_UI_RAW
        return cStr + 1;
    }
    const char *attachedStr;
    if ((attachedStr = output.getAttachedString(cStr)) != nullptr) {
        return attachedStr;
    }
    int size;
    if ((size = PrintHtmlEntities::getTranslatedSize(cStr, attribute)) == -1) {
        return cStr;
    }
    String target;
    if (PrintHtmlEntities::translateTo(cStr, target, size)) {
        if ((attachedStr = output.getAttachedString(target.c_str())) != nullptr) { // check if we have this string already
            return attachedStr;
        }
        return output.attachString(std::move(target));
    }
    return cStr;
}

// void FormUI::_encodeHtmlEntitiesString(String &str)
// {
//     int size = PrintHtmlEntities::getTranslatedSize(str.c_str());
//     if (size != PrintHtmlEntities::kNoTranslationRequired) {
//         String tmp;
//         if (PrintHtmlEntities::translateTo(str.c_str(), tmp, size)) {
//             str = std::move(tmp);
//         }
//     }
// }

// void FormUI::_encodeHtmlEntitiesToString(const String &from, bool attribute, String &target)
// {
//     int size = PrintHtmlEntities::getTranslatedSize(from.c_str(), attribute);
//     if (size != PrintHtmlEntities::kNoTranslationRequired) {
//         target = String();
//         if (PrintHtmlEntities::translateTo(from.c_str(), target, attribute, size)) {
//             return;
//         }
//     }
//     target = from;
// }

void FormUI::html(PrintInterface &output)
{
    const char *name = nullptr;
    switch(_type) {
        case Type::GROUP_START_HR:
        case Type::GROUP_START_DIV:
        case Type::GROUP_END_HR:
        case Type::GROUP_END_DIV:
        case Type::GROUP_END:
            break;
        default:
            name = _encodeHtmlEntities(_parent->getName(), true, output);
            break;
    }
    switch(_type) {
        case Type::GROUP_START_HR: {
            if (_label.length()) {
                output.printf_P(PSTR("<div class=\"form-row%s%s\"><div class=\"col-lg-12\"><h5>%s</h5></div><div class=\"col-lg-12\"><hr class=\"mt-0\"></div></div><div class=\"form-group\">" FORMUI_CRLF), _parent->getNameType(), _parent->getNameForType(), _encodeHtmlEntities(_label, true, output));
            } else {
                output.printf_P(PSTR("<div class=\"form-row%s%s\"><div class=\"col-lg-12 mt-3\"><hr></div></div><div class=\"form-group\">" FORMUI_CRLF), _parent->getNameType(), _parent->getNameForType());
            }
        } break;
        case Type::GROUP_START_DIV: {
            if (_label.length()) {
                output.printf_P(PSTR("<div class=\"form-dependency-group%s%s\" data-action=\"%s\">" FORMUI_CRLF), _parent->getNameType(), _parent->getNameForType(), _encodeHtmlEntities(_label, true, output));
            } else {
                output.printf_P(PSTR("<div class=\"form-dependency-group%s%s\">" FORMUI_CRLF), _parent->getNameType(), _parent->getNameForType());
            }
        } break;
        case Type::GROUP_END_HR:
        case Type::GROUP_END_DIV: {
            output.printf_P(PSTR("</div>" FORMUI_CRLF));
        } break;
        case Type::GROUP_START: {
            FormGroup &group = reinterpret_cast<FormGroup &>(*_parent);
            output.printf_P(PSTR("<div class=\"form-group\"><button class=\"btn btn-secondary btn-block\""));
            output.printf_P(PSTR(" type=\"button\" data-toggle=\"collapse\" data-target=\"#%s\" aria-expanded=\"false\" aria-controls=\"%s\">"), name, name);
            output.printf_P(PSTR("%s</button></div><div class=\"collapse%s\" id=\"%s\"><div class=\"card card-body mb-3\">" FORMUI_CRLF), _encodeHtmlEntities(_label, false, output), group.isExpanded() ? F(".show") : FPSTR(emptyString.c_str()), name);
        } break;
        case Type::GROUP_END: {
            output.printf_P(PSTR("</div></div>" FORMUI_CRLF));
        } break;
        case Type::HIDDEN: {
            output.printf_P(PSTR("<input type=\"hidden\" name=\"%s\" id=\"%s\" value=\"%s\"%s>" FORMUI_CRLF), name, name, _encodeHtmlEntities(_parent->getValue(), true, output), _attributes.c_str());
        } break;
        default: {
            output.printf_P(PSTR("<div class=\"form-group\"><label for=\"%s\">%s</label>" FORMUI_CRLF), name, _encodeHtmlEntities(_label, false, output));

            if (_suffix.length()) {
                output.printf_P(PSTR("<div class=\"input-group\">" FORMUI_CRLF));
            }

            switch (_type) {
            case Type::SELECT:
                output.printf_P(PSTR("<select class=\"form-control\" name=\"%s\" id=\"%s\"%s>" FORMUI_CRLF), name, name, _attributes.c_str());
                for (auto &item : _items) {
                    // we cannot translate item.first when adding it or we cannot compare its value anymore
                    PGM_P selected = _compareValue(item.first) ? PSTR(" selected") : emptyString.c_str();
                    output.printf_P(PSTR("<option value=\"%s\"%s>%s</option>" FORMUI_CRLF), _encodeHtmlEntities(item.first, true, output), selected, _encodeHtmlEntities(item.second, false, output));
                }
                output.printf_P(PSTR("</select>" FORMUI_CRLF));
                break;
            case Type::TEXT:
            case Type::NUMBER:
            case Type::INTEGER:
            case Type::FLOAT:
                output.printf_P(PSTR("<input type=\"text\" class=\"form-control\" name=\"%s\" id=\"%s\" value=\"%s\"%s>" FORMUI_CRLF), name, name, _encodeHtmlEntities(_parent->getValue(), true, output), _attributes.c_str());
                break;
            case Type::PASSWORD:
                output.printf_P(PSTR("<input type=\"password\" class=\"form-control visible-password\" name=\"%s\" id=\"%s\" autocomplete=\"current-password\" spellcheck=\"false\"%s>" FORMUI_CRLF), name, name, _attributes.c_str());
                break;
            case Type::NEW_PASSWORD:
                output.printf_P(PSTR("<input type=\"password\" class=\"form-control visible-password\" name=\"%s\" id=\"%s\" autocomplete=\"new-password\" spellcheck=\"false\"%s>" FORMUI_CRLF), name, name, _attributes.c_str());
                break;
            case Type::RANGE:
                output.printf_P(PSTR("<input type=\"range\" class=\"custom-range\" value=\"%s\" name=\"%s\" id=\"%s\"%s>"), _encodeHtmlEntities(_parent->getValue(), true, output), name, name, _attributes.c_str());
                break;
            case Type::RANGE_SLIDER:
                output.printf_P(PSTR("<div class=\"form-enable-slider\"><input type=\"range\" value=\"%s\" name=\"%s\" id=\"%s\"%s></div>"), _encodeHtmlEntities(_parent->getValue(), true, output), name, name, _attributes.c_str());
                break;
            default:
                break;
            }

            if (_suffix.length()) {
                if (_suffix.charAt(0) == '<') {
                    output.printf_P(PSTR("<div class=\"input-group-append\">%s</div></div>" FORMUI_CRLF), _encodeHtmlEntities(_suffix, false, output));
                } else {
                    output.printf_P(PSTR("<div class=\"input-group-append\"><span class=\"input-group-text\">%s</span></div></div>" FORMUI_CRLF), _encodeHtmlEntities(_suffix, false, output));
                }
            }

            output.printf_P(PSTR("</div>" FORMUI_CRLF));

        } break;
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
