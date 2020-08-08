/**
* Author: sascha_lammers@gmx.de
*/

#include "FormUI.h"
#include "FormField.h"
#include <PrintHtmlEntities.h>
#include <misc.h>

namespace FormUI {

    UI &UI::addInputGroupAppendCheckBoxButton(FormField &hiddenField, const String &label, const __FlashStringHelper *onIcons, const __FlashStringHelper *offIcons)
    {
        _suffix += F("<span class=\"button-checkbox\"");
        if (onIcons && pgm_read_byte(RFPSTR(onIcons))) {
            _suffix += F(" data-on-icon=\"");
            _suffix += onIcons;
            _suffix += '"';
        }
        if (offIcons && pgm_read_byte(RFPSTR(offIcons))) {
            _suffix += F(" data-off-icon=\"");
            _suffix += offIcons;
            _suffix += '"';
        }
        _suffix += F("><button type=\"button\" class=\"btn btn-default\" id=\"_");
        _suffix += hiddenField.getName();
        _suffix += '"';
        _suffix += '>';
        _suffix += PrintHtmlEntities::getTranslatedTo(label, false);
        _suffix += F("</button></span>");

        return *this;
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

            if (_label.length()) {

                output.printf_P(PSTR("<div class=\"card\"><div class=\"card-header p-1\" id=\"heading-%s\"><h2 class=\"mb-1\"><button class=\"btn btn-link btn-lg collapsed\" type=\"button\" data-toggle=\"collapse\" data-target=\"#collapse-%s\" aria-expanded=\"false\" aria-controls=\"collapse-%s\"><strong>%s</strong></button></h2></div>" FORMUI_CRLF),
                    name,
                    name,
                    name,
                    _encodeHtmlEntities(_label, true, output)
                );

                output.printf_P(PSTR("<div id=\"collapse-%s\" class=\"collapse%s\" aria-labelledby=\"heading-%s\" data-cookie=\"#%s\"><div class=\"card-body\">" FORMUI_CRLF),
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
                _encodeHtmlEntities(_parent->getValue(), true, output),
                _attributes.c_str()
            );
        } break;

        default: {
            output.printf_P(PSTR("<div class=\"form-group\">" FORMUI_CRLF));
            if (_label.length()) {
                // append colon if label is not marked as raw output
                if (_label.charAt(0) != 0xff && !String_endsWith(_label, ':')) {
                    _label += ':';
                }
                output.printf_P(PSTR("<label for=\"%s\">%s</label>" FORMUI_CRLF),
                    name,
                    _encodeHtmlEntities(_label, false, output)
                );

            }
            if (_suffix.length()) {
                output.printf_P(PSTR("<div class=\"input-group\">" FORMUI_CRLF));
            }

            switch (_type) {
            case Type::SELECT:
                output.printf_P(PSTR("<select class=\"form-control\" name=\"%s\" id=\"%s\"%s>" FORMUI_CRLF), name, name, _attributes.c_str());
                if (_items) {
                    for (auto &item : *_items) {
                        // we cannot translate item.first when adding it or we cannot compare its value anymore
                        PGM_P selected = _compareValue(item.first) ? SPGM(_selected) : emptyString.c_str();

                        auto val = _encodeHtmlEntities(item.second, false, output);
                        auto key = _encodeHtmlEntities(item.first, true, output);

                        output.printf_P(PSTR("<option value=\"%s\"%s>%s</option>" FORMUI_CRLF),
                            key,
                            selected,
                            val
                        );
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
                    _encodeHtmlEntities(_parent->getValue(), true, output),
                    _attributes.c_str()
                );
                break;
            case Type::PASSWORD:
                output.printf_P(PSTR("<input type=\"password\" class=\"form-control visible-password\" name=\"%s\" id=\"%s\" value=\"%s\" autocomplete=\"current-password\" spellcheck=\"false\"%s>" FORMUI_CRLF),
                    name,
                    name,
                    _encodeHtmlEntities(_parent->getValue(), true, output),
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
                        _suffix.c_str()
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

}
