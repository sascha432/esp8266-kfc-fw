/**
* Author: sascha_lammers@gmx.de
*/

#include "FormUI.h"
#include "FormField.h"
#include "Form.h"
#include <PrintHtmlEntitiesString.h>
#include <misc.h>

#if DEBUG_KFC_FORMS
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

const char *FormUI::__FormFieldGetName(FormField &field)
{
    return field.getName().c_str();
}

const char *FormUI::__FormFieldAttachString(FormField *parent, const char *str)
{
    return parent->getFormUIConfig().attachString(str);
}

const char *FormUI::__FormFieldEncodeHtmlEntities(FormField *parent, const char *str)
{
    return parent->getFormUIConfig().encodeHtmlEntities(str);
}

const char *FormUI::__FormFieldEncodeHtmlAttribute(FormField *parent, const char *str)
{
    return parent->getFormUIConfig().encodeHtmlAttribute(str);
}

void Form::createHtml(PrintInterface &output)
{
    auto &ui = getFormUIConfig();
    __LDBG_printf("style=%u", ui.getStyle());

    switch(ui.getStyle()) {
        case FormUI::StyleType::ACCORDION: {
            output.printf_P(PSTR("<div class=\"accordion pt-3\" id=\"%s\"><div class=\"card bg-primary text-white mb-0\"><div class=\"card-header\" id=\"main-header\"><h3 class=\"mb-0 p-1\">%s</h3></div></div>"),
                ui.getContainerId(),
                ui.getTitle()
            );
        }
        break;
        default: {
            if (ui.hasTitle()) {
                output.printf_P(PSTR("<h1>%s</h1>"), ui.getTitle());
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
                output.printf_P(PSTR("<div class=\"card\"><div class=\"card-body\"><button type=\"submit\" class=\"btn btn-primary\">%s</button></div></div></div>"),
                    ui.getButtonLabel()
                );
            }
            else {
                output.printf_P(PrintArgs::FormatType::HTML_CLOSE_DIV);
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

    using StorageType = ItemsStorage::StorageType;
    using PrintValue = ItemsStorage::PrintValue;

    void UI::_printLabelTo(PrintInterface &output, const char *forLabel) const
    {
        auto iterator = _storage.find(_storage.begin(), _storage.end(), ItemsStorage::isLabel);
        if (iterator != _storage.end()) {
            if (forLabel == nullptr) {
                PrintValue::print(iterator, output);
            }
            else {
                auto isRaw = ItemsStorage::getType(iterator) == StorageType::LABEL_RAW;
                auto label = ItemsStorage::getData<ItemsStorage::Label>(iterator).getValue();

                output.printf_P(PSTR("<label for=\"%s\">%s"), forLabel, label);
                PrintArgs::FormatType endLabel = (isRaw || str_endswith_P(label, ':')) ? PrintArgs::FormatType::HTML_CLOSE_LABEL : PrintArgs::FormatType::HTML_CLOSE_LABEL_WITH_COLON;
                //PrintValue::print(iterator, output);
                output.printf_P(endLabel);
            }
        }
    }

    void UI::_printAttributeTo(PrintInterface &output) const
    {
        for (auto iterator = _storage.find(_storage.begin(), _storage.end(), ItemsStorage::isAttribute); iterator != _storage.end(); iterator = ItemsStorage::advanceIterator(iterator)) {
            switch(ItemsStorage::getType(iterator)) {
                case ItemsStorage::StorageType::ATTRIBUTE: {
                    auto attr = ItemsStorage::getData<ItemsStorage::Attribute>(iterator);
                    output.printf_P(PSTR(" %s=\"%s\""), attr.getKey(), attr.getValue());
                } break;
                case ItemsStorage::StorageType::ATTRIBUTE_MIN_MAX: {
                    auto attr = ItemsStorage::getData<ItemsStorage::AttributeMinMax>(iterator);
                    output.printf_P(PSTR(" min=\"%d\" max=\"%d\""), attr.getMin(), attr.getMax());
                } break;
                default:
                    break;
            }
        }
        output.printf_P(PrintArgs::FormatType::HTML_CLOSE_TAG);
    }

    void UI::_printSuffixTo(PrintInterface &output) const
    {
        output.printf_P(PrintArgs::FormatType::HTML_OPEN_DIV_INPUT_GROUP_APPEND);
        for (auto iterator = _storage.find(_storage.begin(), _storage.end(), ItemsStorage::isSuffix); iterator != _storage.end(); ) {
            output.printf_P(PrintArgs::FormatType::HTML_OPEN_SPAN_INPUT_GROUP_TEXT);
            ItemsStorage::PrintValue::print(iterator, output);
            output.printf_P(PrintArgs::FormatType::HTML_CLOSE_SPAN);
            bool isEnd = iterator == _storage.end();
            int k = 0;
        }
        output.printf_P(PrintArgs::FormatType::HTML_CLOSE_DIV_2X);
    }

    void UI::_printOptionsTo(PrintInterface &output) const
    {
        for (auto iterator = _storage.find(_storage.begin(), _storage.end(), ItemsStorage::isOption); iterator != _storage.end(); iterator = ItemsStorage::advanceIterator(iterator)) {
            switch (ItemsStorage::getType(iterator)) {
                case ItemsStorage::StorageType::OPTION_NUM_KEY: {
                    auto option = ItemsStorage::getData<ItemsStorage::OptionNumKey>(iterator);
                    if (_isSelected(option.getKey())) {
                        output.printf_P(PSTR("<option value=\"%d\" selected>%s</option>"), option.getKey(), option.getValue());
                    }
                    else {
                        output.printf_P(PSTR("<option value=\"%d\">%s</option>"), option.getKey(), option.getValue());
                    }
                } break;
                case ItemsStorage::StorageType::OPTION: {
                    auto option = ItemsStorage::getData<ItemsStorage::Option>(iterator);
                    if (_compareValue(option.getKey())) {
                        output.printf_P(PSTR("<option value=\"%s\" selected>%s</option>"), option.getKey(), option.getValue());
                    }
                    else {
                        output.printf_P(PSTR("<option value=\"%s\">%s</option>"), option.getKey(), option.getValue());
                    }
                } break;
                default:
                    break;
            }
        }
    }

    StringDeduplicator &UI::strings()
    {
        return _parent->getFormUIConfig().strings();
    }

    void UI::html(PrintInterface &output)
    {
        auto &ui = _parent->getFormUIConfig();
        __LDBG_printf("type=%u name=%s", _type, __S(_parent->getName()));

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
            name = ui.encodeHtmlEntities(_parent->getName(), true);
            break;
        }

        switch (_type) {
            // ---------------------------------------------------------------
            // Card Group
            // ---------------------------------------------------------------
            case Type::GROUP_START_CARD: {
                    FormGroup &group = reinterpret_cast<FormGroup &>(*_parent);
                    if (_hasLabel()) {
                        output.printf_P(PSTR("<div class=\"card\"><div class=\"card-header p-1\" id=\"heading-%s\"><h2 class=\"mb-1\"><button class=\"btn btn-link btn-lg collapsed\" type=\"button\" data-toggle=\"collapse\" data-target=\"#collapse-%s\" aria-expanded=\"false\" aria-controls=\"collapse-%s\"><strong>%s"), 
                            name, 
                            name, 
                            name
                        );
                        _printLabelTo(output, nullptr);
                        output.printf_P(PSTR("</strong></button></h2></div><div id=\"collapse-%s\" class=\"collapse%s\" aria-labelledby=\"heading-%s\" data-cookie=\"#%s\"><div class=\"card-body\">"),
                            name,
                            (group.isExpanded() ? PSTR(" show") : emptyString.c_str()),
                            name,
                            ui.getContainerId()
                        );
                    }
                    else {
                        output.printf_P(PrintArgs::FormatType::HTML_OPEN_DIV_CARD_DIV_DIV_CARD_BODY);
                    }
                }
                break;
            case Type::GROUP_END_CARD: {
                    output.printf_P(PrintArgs::FormatType::HTML_CLOSE_DIV_3X);
                }
                break;

            // ---------------------------------------------------------------
            // HR Group
            // ---------------------------------------------------------------
            case Type::GROUP_START_HR: {
                    if (_hasLabel()) {
                        output.printf_P(PSTR("<div class=\"form-row%s%s\"><div class=\"col-lg-12\"><h5>"), _parent->getNameType(), _parent->getNameForType());
                        _printLabelTo(output, nullptr);
                        output.printf_P(PrintArgs::FormatType::HTML_CLOSE_GROUP_START_HR);
                    }
                    else {
                        output.printf_P(PSTR("<div class=\"form-row%s%s\"><div class=\"col-lg-12 mt-3\"><hr></div></div><div class=\"form-group\">"),
                            _parent->getNameType(),
                            _parent->getNameForType()
                        );
                    }
                } break;

            // ---------------------------------------------------------------
            // Div Group
            // ---------------------------------------------------------------
            case Type::GROUP_START_DIV: {
                    if (_hasLabel()) {
                        output.printf_P(PSTR("<div class=\"form-dependency-group%s%s\" data-action=\""), _parent->getNameType(), _parent->getNameForType());
                        _printLabelTo(output, nullptr);
                        output.printf_P(PrintArgs::FormatType::HTML_CLOSE_QUOTE_CLOSE_TAG);
                    }
                    else {
                        output.printf_P(PSTR("<div class=\"form-dependency-group%s%s\">"),
                            _parent->getNameType(),
                            _parent->getNameForType()
                        );
                    }
                } break;
            case Type::GROUP_END_HR:
            case Type::GROUP_END_DIV: {
                    output.printf_P(PrintArgs::FormatType::HTML_CLOSE_DIV);
                } break;

            // ---------------------------------------------------------------
            // Default Group
            // ---------------------------------------------------------------
            case Type::GROUP_START: {
                    FormGroup &group = reinterpret_cast<FormGroup &>(*_parent);
                    output.printf_P(PSTR("<div class=\"form-group\"><button class=\"btn btn-secondary btn-block\" type=\"button\" data-toggle=\"collapse\" data-target=\"#%s\" aria-expanded=\"false\" aria-controls=\"%s\">"), name, name);
                    _printLabelTo(output, nullptr);
                    output.printf_P(PSTR("</button></div><div class=\"collapse%s\" id=\"%s\"><div class=\"card card-body mb-3\">"), group.isExpanded() ? PSTR(" show") : emptyString.c_str(), name);
                } break;
            case Type::GROUP_END: {
                    output.printf_P(PrintArgs::FormatType::HTML_CLOSE_DIV_2X);
                } break;

            // ---------------------------------------------------------------
            // Hidden input
            // ---------------------------------------------------------------
            case Type::HIDDEN: {
                    output.printf_P(PSTR("<input type=\"hidden\" name=\"%s\" id=\"%s\" value=\"%s\""), name, name, ui.encodeHtmlEntities(_parent->getValue(), true));
                    _printAttributeTo(output);
                } break;

            // ---------------------------------------------------------------
            // Form group
            // ---------------------------------------------------------------
            default: {
                output.printf_P(PrintArgs::FormatType::HTML_OPEN_DIV_FORM_GROUP);
                _printLabelTo(output, name);
                auto hasSuffix = _hasSuffix();
                if (hasSuffix) {
                    output.printf_P(PrintArgs::FormatType::HTML_OPEN_DIV_INPUT_GROUP);
                }

                __LDBG_printf("type=%u", _type);
                switch (_type) {
                    // ---------------------------------------------------------------
                    // Select field
                    // ---------------------------------------------------------------
                    case Type::SELECT:
                        output.printf_P(PSTR("<select class=\"form-control\" name=\"%s\" id=\"%s\""), name, name);
                        _printAttributeTo(output);
                        _printOptionsTo(output);
                        output.printf_P(PrintArgs::FormatType::HTML_CLOSE_SELECT);
                        break;

                    // ---------------------------------------------------------------
                    // Input field
                    // ---------------------------------------------------------------
                    case Type::TEXT:
                    case Type::NUMBER:
                    case Type::INTEGER:
                    case Type::FLOAT:
                        output.printf_P(PSTR("<input type=\"text\" class=\"form-control\" name=\"%s\" id=\"%s\" value=\"%s\""),
                            name,
                            name,
                            ui.encodeHtmlEntities(_parent->getValue(), true)
                        );
                        _printAttributeTo(output);
                        break;

                    // ---------------------------------------------------------------
                    // Password input fields
                    // ---------------------------------------------------------------
                    case Type::PASSWORD:
                        output.printf_P(PSTR("<input type=\"password\" class=\"form-control visible-password\" name=\"%s\" id=\"%s\" value=\"%s\" autocomplete=\"current-password\" spellcheck=\"false\""),
                            name,
                            name,
                            ui.encodeHtmlEntities(_parent->getValue(), true)
                        );
                        _printAttributeTo(output);
                        break;
                    case Type::NEW_PASSWORD:
                        output.printf_P(PSTR("<input type=\"password\" class=\"form-control visible-password\" name=\"%s\" id=\"%s\" autocomplete=\"new-password\" spellcheck=\"false\""),                    
                            name,
                            name
                        );
                        _printAttributeTo(output);
                        break;

                    // ---------------------------------------------------------------
                    // Input range fields
                    // ---------------------------------------------------------------
                    case Type::RANGE:
                        output.printf_P(PSTR("<input type=\"range\" class=\"custom-range\" value=\"%s\" name=\"%s\" id=\"%s\""),
                            ui.encodeHtmlEntities(_parent->getValue(), true),
                            name,
                            name
                        );
                        _printAttributeTo(output);
                        break;
                    case Type::RANGE_SLIDER:
                        output.printf_P(PSTR(
                                "<div class=\"form-enable-slider\"><input type=\"range\" value=\"%s\" name=\"%s\" id=\"%s\""
                            ),
                            ui.encodeHtmlEntities(_parent->getValue(), true),
                            name,
                            name
                        );
                        _printAttributeTo(output);
                        output.printf_P(PrintArgs::FormatType::HTML_CLOSE_DIV);
                        break;
                    default:
                        break;
                }

                if (_hasSuffix()) {
                    _printSuffixTo(output);
                }
                else {
                    output.printf_P(PrintArgs::FormatType::HTML_CLOSE_DIV);
                }

            } break;
        }
    }

    void UI::_addItem(const StringAttribute &attribute)
    {
        const char *value;
        _storage.push_back(ItemsStorage::Attribute(attachString(attribute._key), value = encodeHtmlEntities(attribute._value)));
        if (__isDisabledAttribute((const char *)attribute._key, value)) {
            _parent->setDisabled(true);
        }
    }

    void UI::_addItem(const FPStringAttribute &attribute)
    {
        const char *value;
        _storage.push_back(ItemsStorage::Attribute(attachString(attribute._key), value = encodeHtmlEntities(attribute._value)));
        if (__isDisabledAttribute((const char *)attribute._key, value)) {
            _parent->setDisabled(true);
        }
    }

}
