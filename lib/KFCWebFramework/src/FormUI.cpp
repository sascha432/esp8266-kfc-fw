/**
* Author: sascha_lammers@gmx.de
*/

#include "FormUI.h"
#include "FormField.h"
#include <PrintHtmlEntities.h>
#include <misc.h>

namespace FormUI {

    UI *UI::setBoolItems()
    {
        return setBoolItems(FSPGM(Enabled), FSPGM(Disabled));
    }

    UI *UI::setBoolItems(const String &enabled, const String &disabled)
    {
        _items.clear();
        _items.reserve(2);
        _items.emplace_back(std::move(String(0)), disabled);
        _items.emplace_back(std::move(String(1)), enabled);
        return this;
    }

    UI *UI::addItems(const String &value, const String &label)
    {
        _items.push_back(value, label);
        return this;
    }

    UI *UI::addItems(const ItemsList &items)
    {
        _items = items;
        return this;
    }

    UI *UI::setSuffix(const String &suffix)
    {
        _suffix = suffix;
        return this;
    }

    UI *UI::setPlaceholder(const String &placeholder)
    {
        addAttribute(FSPGM(placeholder), placeholder);
        return this;
    }

    UI *UI::setMinMax(const String &min, const String &max)
    {
        addAttribute(FSPGM(min), min);
        addAttribute(FSPGM(max), max);
        return this;
    }

    UI *UI::addAttribute(const String &name, const String &value)
    {
        _attributes += ' ';
        _attributes += name;
        if (value.length()) {
            _attributes += '=';
            _attributes += '"';
            // append translated value to _attributes
            if (!PrintHtmlEntities::translateTo(value.c_str(), _attributes, true)) {
                _attributes += value; // no translation required, just append value
            }
            _attributes += '"';
        }
        return this;
    }

    UI *UI::addAttribute(const __FlashStringHelper *name, const String &value)
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

    UI *UI::addConditionalAttribute(bool cond, const String &name, const String &value)
    {
        if (cond) {
            return addAttribute(name, value);
        }
        return this;
    }

    UI *UI::setReadOnly()
    {
        return addAttribute(FSPGM(readonly), String());
    }

    const char *UI::_encodeHtmlEntities(const char *cStr, bool attribute, PrintInterface &output)
    {
        if (pgm_read_byte(cStr) == 0xff) {
            return ++cStr;
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

    void UI::html(PrintInterface &output)
    {
        const char *name = nullptr;
        switch (_type) {
        case Type::GROUP_START_HR:
        case Type::GROUP_START_DIV:
        case Type::GROUP_END_HR:
        case Type::GROUP_END_DIV:
        case Type::GROUP_END:
            break;
        case Type::GROUP_START_CARD:
        default:
            name = _encodeHtmlEntities(_parent->getName(), true, output);
            break;
        }

        switch (_type) {
        // ---------------------------------------------------------------
        // Card Group
        case Type::GROUP_START_CARD: {
            auto &ui = _parent->getFormUIConfig();
            FormGroup &group = reinterpret_cast<FormGroup &>(*_parent);


            output.printf_P(PSTR("<div class=\"card\"><div class=\"card-header\" id=\"heading-%s\"><h2 class=\"mb-0\"><button class=\"btn btn-link btn-lg collapsed\" type=\"button\" data-toggle=\"collapse\" data-target=\"#collapse-%s\" aria-expanded=\"false\" aria-controls=\"collapse-%s\">%s</button></h2></div>" FORMUI_CRLF),
                name,
                name,
                name,
                _encodeHtmlEntities(_label, true, output)
            );

            output.printf_P(PSTR("<div id=\"collapse-%s\" class=\"collapse%s\" aria-labelledby=\"heading-%s\" data-parent=\"#%s\"><div class=\"card-body\">" FORMUI_CRLF),
                name,
                (group.isExpanded() ? PSTR(" show") : emptyString.c_str()),
                name,
                ui.getContainerId().c_str()
            );
        }
        break;
        case Type::GROUP_END_CARD: {
            output.printf_P(PSTR("</div></div></div>" FORMUI_CRLF));
        }
        break;

        // ---------------------------------------------------------------
        // HR Group
        case Type::GROUP_START_HR: {
            if (_label.length()) {
                output.printf_P(PSTR("<div class=\"form-row%s%s\"><div class=\"col-lg-12\"><h5>%s</h5></div><div class=\"col-lg-12\"><hr class=\"mt-0\"></div></div>" FORMUI_CRLF "<div class=\"form-group\">" FORMUI_CRLF),
                    _parent->getNameType(),
                    _parent->getNameForType(),
                    _encodeHtmlEntities(_label, true, output)
                );
            }
            else {
                output.printf_P(PSTR("<div class=\"form-row%s%s\"><div class=\"col-lg-12 mt-3\"><hr></div></div><div class=\"form-group\">" FORMUI_CRLF),
                    _parent->getNameType(),
                    _parent->getNameForType()
                );
            }
        } break;

        // ---------------------------------------------------------------
        // Div Group
        case Type::GROUP_START_DIV: {
            if (_label.length()) {
                output.printf_P(PSTR("<div class=\"form-dependency-group%s%s\" data-action=\"%s\">" FORMUI_CRLF),
                    _parent->getNameType(),
                    _parent->getNameForType(),
                    _encodeHtmlEntities(_label, true, output)
                );
            }
            else {
                output.printf_P(PSTR("<div class=\"form-dependency-group%s%s\">" FORMUI_CRLF),
                    _parent->getNameType(),
                    _parent->getNameForType()
                );
            }
        } break;
        case Type::GROUP_END_HR:
        case Type::GROUP_END_DIV: {
            output.printf_P(PSTR("</div>" FORMUI_CRLF));
        } break;

        // ---------------------------------------------------------------
        // Default Group
        case Type::GROUP_START: {
            FormGroup &group = reinterpret_cast<FormGroup &>(*_parent);
            output.printf_P(PSTR("<div class=\"form-group\"><button class=\"btn btn-secondary btn-block\""));
            output.printf_P(PSTR(" type=\"button\" data-toggle=\"collapse\" data-target=\"#%s\" aria-expanded=\"false\" aria-controls=\"%s\">"),
                name,
                name
            );
            output.printf_P(PSTR("%s</button></div><div class=\"collapse%s\" id=\"%s\"><div class=\"card card-body mb-3\">" FORMUI_CRLF),
                _encodeHtmlEntities(_label, false, output),
                group.isExpanded() ? F(" show") : FPSTR(emptyString.c_str()),
                name
            );
        } break;
        case Type::GROUP_END: {
            output.printf_P(PSTR("</div></div>" FORMUI_CRLF));
        } break;

        case Type::HIDDEN: {
            output.printf_P(PSTR("<input type=\"hidden\" name=\"%s\" id=\"%s\" value=\"%s\"%s>" FORMUI_CRLF),
                name,
                name,
                _encodeHtmlEntities(_parent->getValue(), true, output),
                _attributes.c_str()
            );
        } break;

        default: {
            output.printf_P(PSTR("<div class=\"form-group\"><label for=\"%s\">%s</label>" FORMUI_CRLF),
                name,
                _encodeHtmlEntities(_label, false, output)
            );

            if (_suffix.length()) {
                output.printf_P(PSTR("<div class=\"input-group\">" FORMUI_CRLF));
            }

            switch (_type) {
            case Type::SELECT:
                output.printf_P(PSTR("<select class=\"form-control\" name=\"%s\" id=\"%s\"%s>" FORMUI_CRLF), name, name, _attributes.c_str());
                for (auto &item : _items) {
                    // we cannot translate item.first when adding it or we cannot compare its value anymore
                    PGM_P selected = _compareValue(item.first) ? PSTR(" selected") : emptyString.c_str();
                    output.printf_P(PSTR("<option value=\"%s\"%s>%s</option>" FORMUI_CRLF),
                        _encodeHtmlEntities(item.first, true, output),
                        selected,
                        _encodeHtmlEntities(item.second, false, output)
                    );
                }
                output.printf_P(PSTR("</select>" FORMUI_CRLF));
                break;
            case Type::TEXT:
            case Type::NUMBER:
            case Type::INTEGER:
            case Type::FLOAT:
                output.printf_P(PSTR("<input type=\"text\" class=\"form-control\" name=\"%s\" id=\"%s\" value=\"%s\"%s>" FORMUI_CRLF),
                    name,
                    name,
                    _encodeHtmlEntities(_parent->getValue(), true, output),
                    _attributes.c_str()
                );
                break;
            case Type::PASSWORD:
                output.printf_P(PSTR("<input type=\"password\" class=\"form-control visible-password\" name=\"%s\" id=\"%s\" autocomplete=\"current-password\" spellcheck=\"false\"%s>" FORMUI_CRLF),
                    name,
                    name,
                    _attributes.c_str()
                );
                break;
            case Type::NEW_PASSWORD:
                output.printf_P(PSTR("<input type=\"password\" class=\"form-control visible-password\" name=\"%s\" id=\"%s\" autocomplete=\"new-password\" spellcheck=\"false\"%s>" FORMUI_CRLF),
                    name,
                    name,
                    _attributes.c_str()
                );
                break;
            case Type::RANGE:
                output.printf_P(PSTR("<input type=\"range\" class=\"custom-range\" value=\"%s\" name=\"%s\" id=\"%s\"%s>" FORMUI_CRLF),
                    _encodeHtmlEntities(_parent->getValue(), true, output),
                    name,
                    name,
                    _attributes.c_str()
                );
                break;
            case Type::RANGE_SLIDER:
                output.printf_P(PSTR("<div class=\"form-enable-slider\"><input type=\"range\" value=\"%s\" name=\"%s\" id=\"%s\"%s></div>" FORMUI_CRLF),
                    _encodeHtmlEntities(_parent->getValue(), true, output),
                    name,
                    name,
                    _attributes.c_str()
                );
                break;
            default:
                break;
            }

            if (_suffix.length()) {
                if (_suffix.charAt(0) == '<') {
                    output.printf_P(PSTR("<div class=\"input-group-append\">%s</div></div>" FORMUI_CRLF),
                        _encodeHtmlEntities(_suffix, false, output)
                    );
                }
                else {
                    output.printf_P(PSTR("<div class=\"input-group-append\"><span class=\"input-group-text\">%s</span></div></div>" FORMUI_CRLF),
                        _encodeHtmlEntities(_suffix, false, output)
                    );
                }
            }

            output.printf_P(PSTR("</div>" FORMUI_CRLF));

        } break;
        }
    }

    void UI::setParent(FormField *field)
    {
        _parent = field;
    }

    bool UI::_compareValue(const String &value) const
    {
        if (_parent->getType() == FormField::Type::TEXT) {
            return value.equals(_parent->getValue());
        }
        else {
            return value.toInt() == _parent->getValue().toInt();
        }
    }

}
