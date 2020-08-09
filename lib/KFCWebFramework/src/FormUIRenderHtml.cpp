/**
* Author: sascha_lammers@gmx.de
*/

#include "FormUI.h"
#include "FormField.h"
#include "Form.h"
#include <PrintHtmlEntitiesString.h>
#include <misc.h>

void Form::createHtml(PrintInterface &output)
{
    auto &ui = getFormUIConfig();

    switch(ui.getStyle()) {
        case FormUI::StyleType::ACCORDION: {
            output.printf_P(PSTR("<div class=\"accordion pt-3\" id=\"%s\"><div class=\"card bg-primary text-white mb-0\"><div class=\"card-header\" id=\"main-header\"><h3 class=\"mb-0 p-1\">%s</h3></div></div>" FORMUI_CRLF),
                ui.getContainerId().c_str(),
                ui.encodeHtmlEntities(ui.getTitle(), false)
            );
        }
        break;
        default: {
            if (ui.hasTitle()) {
                output.printf_P(PSTR("<h1>%s</h1>" FORMUI_CRLF), ui.encodeHtmlEntities(ui.getTitle(), false));
            }
        }
        break;
    }
    for (const auto &field : _fields) {
        field->html(output);
        if (field->_formUI) {
            delete field->_formUI;
            field->_formUI = nullptr;
        }
    }

    switch(ui.getStyle()) {
        case FormUI::StyleType::ACCORDION: {
            if (ui.hasButtonLabel()) {
                output.printf_P(PSTR("<div class=\"card\"><div class=\"card-body\"><button type=\"submit\" class=\"btn btn-primary\">%s</button></div></div></div>" FORMUI_CRLF),
                    ui.encodeHtmlEntities(ui.getButtonLabel(), false)
                );
            }
            else {
                output.printf_P(PSTR("</div>" FORMUI_CRLF));
            }
        }
        break;
        default: {
            if (ui.hasButtonLabel()) {
                output.printf_P(PSTR("<button type=\"submit\" class=\"btn btn-primary\">%s...</button>" FORMUI_CRLF), ui.encodeHtmlEntities(ui.getButtonLabel(), false));
            }
        }
        break;
    }
}
namespace FormUI {

    Suffix UI::createCheckBoxButton(FormField &hiddenField, const String &label, const __FlashStringHelper *onIcons, const __FlashStringHelper *offIcons)
    {
        PrintHtmlEntitiesString _suffix;
        _suffix.setMode(PrintHtmlEntities::Mode::RAW);
        _suffix += F("<span class=\"button-checkbox\"");
        if (onIcons && pgm_read_byte(RFPSTR(onIcons))) {
            _suffix += F(" data-on-icon=\"");
            _suffix.setMode(PrintHtmlEntities::Mode::ATTRIBUTE);
            _suffix += onIcons;
            _suffix.setMode(PrintHtmlEntities::Mode::RAW);
            _suffix += '"';
        }
        if (offIcons && pgm_read_byte(RFPSTR(offIcons))) {
            _suffix += F(" data-off-icon=\"");
            _suffix.setMode(PrintHtmlEntities::Mode::ATTRIBUTE);
            _suffix += offIcons;
            _suffix.setMode(PrintHtmlEntities::Mode::RAW);
            _suffix += '"';
        }
        _suffix += F("><button type=\"button\" class=\"btn btn-default\" id=\"_");
        _suffix.setMode(PrintHtmlEntities::Mode::ATTRIBUTE);
        _suffix += hiddenField.getName();
        _suffix.setMode(PrintHtmlEntities::Mode::RAW);
        _suffix += '"';
        _suffix += '>';
        _suffix.setMode(PrintHtmlEntities::Mode::HTML);
        _suffix += PrintHtmlEntities::getTranslatedTo(label, false);
        _suffix.setMode(PrintHtmlEntities::Mode::RAW);
        _suffix += F("</button></span>");

        return Suffix(std::move(_suffix));
    }


    void UI::html(PrintInterface &output)
    {
        auto &ui = _parent->getFormUIConfig();
        const char *name = nullptr;
        char ch;
        switch (_type) {
        case Type::GROUP_START_HR:
        case Type::GROUP_START_DIV:
        case Type::GROUP_END_HR:
        case Type::GROUP_END_DIV:
        case Type::GROUP_END:
            break;
        case Type::GROUP_START_CARD:
        default:
            name = ui.encodeHtmlEntities(_parent->getName(), true);
            break;
        }

        switch (_type) {
        // ---------------------------------------------------------------
        // Card Group
        case Type::GROUP_START_CARD: {
            FormGroup &group = reinterpret_cast<FormGroup &>(*_parent);

            if (_label && pgm_read_byte(_label)) {

                output.printf_P(PSTR(
                        "<div class=\"card\"><div class=\"card-header p-1\" id=\"heading-%s\"><h2 class=\"mb-1\"><button class=\"btn btn-link btn-lg collapsed\" type=\"button\" data-toggle=\"collapse\" data-target=\"#collapse-%s\" aria-expanded=\"false\" aria-controls=\"collapse-%s\"><strong>%s</strong></button></h2></div>" FORMUI_CRLF
                        "<div id=\"collapse-%s\" class=\"collapse%s\" aria-labelledby=\"heading-%s\" data-cookie=\"#%s\"><div class=\"card-body\">" FORMUI_CRLF
                    ),
                    name,
                    name,
                    name,
                    ui.encodeHtmlEntities(_label, true),

                    name,
                    (group.isExpanded() ? PSTR(" show") : emptyString.c_str()),
                    name,
                    ui.getContainerId().c_str()
                );

            }
            else {

                output.printf_P(PSTR("<div class=\"card\"><div><div class=\"card-body\">"));

            }

        }
        break;
        case Type::GROUP_END_CARD: {
            output.printf_P(PSTR("</div></div></div>" FORMUI_CRLF));
        }
        break;

        // ---------------------------------------------------------------
        // HR Group
        case Type::GROUP_START_HR: {
            if (_label && pgm_read_byte(_label)) {
                output.printf_P(PSTR("<div class=\"form-row%s%s\"><div class=\"col-lg-12\"><h5>%s</h5></div><div class=\"col-lg-12\"><hr class=\"mt-0\"></div></div>" FORMUI_CRLF "<div class=\"form-group\">" FORMUI_CRLF),
                    _parent->getNameType(),
                    _parent->getNameForType(),
                    ui.encodeHtmlEntities(_label, true)
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
            if (_label && pgm_read_byte(_label)) {
                output.printf_P(PSTR("<div class=\"form-dependency-group%s%s\" data-action=\"%s\">" FORMUI_CRLF),
                    _parent->getNameType(),
                    _parent->getNameForType(),
                    ui.encodeHtmlEntities(_label, true)
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
                ui.encodeHtmlEntities(_label, false),
                group.isExpanded() ? PSTR(" show") : emptyString.c_str(),
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
                ui.encodeHtmlEntities(_parent->getValue(), true),
                _attributes.c_str()
            );
        } break;

        default: {
            output.printf_P(PSTR("<div class=\"form-group\">" FORMUI_CRLF));
            bool addColon = false;
            if (_label && (ch = pgm_read_byte(_label)) != 0) {
                // append colon if label is not marked as raw output
                if (ch != 0xff) {
                    size_t len = strlen_P(_label);
                    if (len && pgm_read_byte(_label + len - 1) != ':') {
                        addColon = true;
                    }
                }
                if (addColon) {
                    output.printf_P(PSTR("<label for=\"%s\">%s:</label>" FORMUI_CRLF),
                        name,
                        ui.encodeHtmlEntities(_label, false)
                    );
                }
                else {
                    output.printf_P(PSTR("<label for=\"%s\">%s</label>" FORMUI_CRLF),
                        name,
                        ui.encodeHtmlEntities(_label, false)
                    );
                }
            }
            if (_suffix && pgm_read_byte(_suffix)) {
                output.printf_P(PSTR("<div class=\"input-group\">" FORMUI_CRLF));
            }

            switch (_type) {
            case Type::SELECT:
                output.printf_P(PSTR("<select class=\"form-control\" name=\"%s\" id=\"%s\"%s>" FORMUI_CRLF), name, name, _attributes.c_str());
                if (_items) {
                    for (auto &item : *_items) {
                        if (_compareValue(item.first)) {
                            output.printf_P(PSTR("<option value=\"%s\" selected>%s</option>" FORMUI_CRLF),
                                item.first,
                                item.second
                            );
                        }
                        else {
                            output.printf_P(PSTR("<option value=\"%s\">%s</option>" FORMUI_CRLF),
                                item.first,
                                item.second
                            );
                        }
                    }
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
                    ui.encodeHtmlEntities(_parent->getValue(), true),
                    _attributes.c_str()
                );
                break;
            case Type::PASSWORD:
                output.printf_P(PSTR("<input type=\"password\" class=\"form-control visible-password\" name=\"%s\" id=\"%s\" value=\"%s\" autocomplete=\"current-password\" spellcheck=\"false\"%s>" FORMUI_CRLF),
                    name,
                    name,
                    ui.encodeHtmlEntities(_parent->getValue(), true),
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
                    ui.encodeHtmlEntities(_parent->getValue(), true),
                    name,
                    name,
                    _attributes.c_str()
                );
                break;
            case Type::RANGE_SLIDER:
                output.printf_P(PSTR("<div class=\"form-enable-slider\"><input type=\"range\" value=\"%s\" name=\"%s\" id=\"%s\"%s></div>" FORMUI_CRLF),
                    ui.encodeHtmlEntities(_parent->getValue(), true),
                    name,
                    name,
                    _attributes.c_str()
                );
                break;
            default:
                break;
            }

            if (_suffix && (ch = pgm_read_byte(_suffix)) != 0) {
                if (ch == '<') {
                    output.printf_P(PSTR("<div class=\"input-group-append\">%s</div></div>" FORMUI_CRLF),
                        _suffix
                    );
                }
                else {
                    output.printf_P(PSTR("<div class=\"input-group-append\"><span class=\"input-group-text\">%s</span></div></div>" FORMUI_CRLF),
                        ui.encodeHtmlEntities(_suffix, false)
                    );
                }
            }

            output.printf_P(PSTR("</div>" FORMUI_CRLF));

        } break;
        }
    }

	void UI::_addItem(Type type)
    {
		_type = type;
	}

	void UI::_addItem(const __FlashStringHelper *label)
    {
		_label = _parent->getFormUIConfig().strings().attachString(label);
	}

	void UI::_addItem(const Label &label)
    {
		_label = _parent->getFormUIConfig().strings().attachString(label);;
	}

	void UI::_addItem(const Suffix &suffix)
    {
		_suffix = _parent->getFormUIConfig().strings().attachString(suffix);;
	}

	void UI::_addItem(const PlaceHolder &placeholder)
    {
		addAttribute(FSPGM(placeholder), placeholder);
	}

	void UI::_addItem(const MinMax &minMax)
    {
		addAttribute(FSPGM(min), minMax._min);
		addAttribute(FSPGM(max), minMax._max);
	}

	void UI::_addItem(const Attribute &attribute)
    {
		addAttribute(attribute._key, attribute._value);
	}

	void UI::_addItem(const ItemsList &items)
    {
		_setItems(items);
		_type = Type::SELECT;
	}

	void UI::_addItem(const BoolItems &boolItems)
    {
        if (_items) {
            delete _items;
        }
        _items = new StringPairList(_parent->getFormUIConfig(), ItemsList(0, boolItems._false, 1, boolItems._true));
		_type = Type::SELECT;
	}

}
