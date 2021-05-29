/**
* Author: sascha_lammers@gmx.de
*/

#include "WebUI/BaseUI.h"
#include "WebUI/Config.h"
#include "Field/BaseField.h"
#include "Form/BaseForm.h"
#include "WebUI/Containers.h"
#include <PrintHtmlEntitiesString.h>
#include <misc.h>
#include "Form/Form.hpp"

#include "Utility/Debug.h"

using namespace FormUI;

// -----------------------------------------------------------------------
// Form
// -----------------------------------------------------------------------

void Form::BaseForm::createHtml(PrintInterface &output)
{
#if DEBUG_KFC_FORMS
    MicrosTimer duration;
    duration.start();
#endif
    auto &ui = getWebUIConfig();
    __LDBG_printf("style=%u", ui.getStyle());

    switch(ui.getStyle()) {
        case WebUI::StyleType::ACCORDION: {
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
        case WebUI::StyleType::ACCORDION: {
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
                output.printf_P(PSTR("<button type=\"submit\" class=\"btn btn-primary\">%s...</button>"), ui.getButtonLabel(), false);
            }
        }
        break;
    }
#if DEBUG_KFC_FORMS
    __DBG_printf("render=form_html time=%.3fms", duration.getTime() / 1000.0);
#endif
}

// -----------------------------------------------------------------------
// FormUI::UI
// -----------------------------------------------------------------------

const char *WebUI::BaseUI::_getLabel() const
{
#if DEBUG_PRINT_ARGS && 1
    const char *value = emptyString.c_str();
    auto iterator = _storage.find_if(_storage.begin(), _storage.end(), Storage::Vector::isLabel);
    if (iterator != _storage.end()) {
        // safe version with validation
        Storage::TypeByte tb(iterator);
        __LDBG_assert_printf(tb.count() == 1, "%s count=%u != 1", tb.name(), tb.count());
        ++iterator;
        switch (tb.type()) {
        case Storage::Value::Label::type:
            value = Storage::Value::Label::pop_front<Storage::Value::Label>(iterator).getValue();
            break;
        case Storage::Type::LABEL_RAW:
            value = Storage::Value::LabelRaw::pop_front<Storage::Value::LabelRaw>(iterator).getValue();
            break;
        default:
            __LDBG_assert_printf(false, "invalid type %u: %s", tb.type(), tb.name());
            break;
        }
        __LDBG_assert_printf(_storage.find_if(iterator, _storage.end(), Storage::Vector::isLabel) == _storage.end(), "multiple labels found");
    }
    return value;
#else
    // fast version with static assert
    static_assert(sizeof(Storage::Value::String) == sizeof(Storage::Value::Label) && sizeof(Storage::Value::String) == sizeof(Storage::Value::LabelRaw), "size of objects does not match");

    auto iterator = _storage.find_if(_storage.begin(), _storage.end(), Storage::Vector::isLabel);
    if (iterator != _storage.end()) {
        return Storage::Value::String::pop_front<Storage::Value::String>(++iterator).getValue();
    }
    return emptyString.c_str();
#endif
}

void WebUI::BaseUI::_printLabelTo(PrintInterface &output, const char *forLabel) const
{
    auto label = _getLabel();
    if (pgm_read_byte(label)) {
        if (forLabel) {
            output.printf_P(str_endswith_P(label, ':') ? PrintArgs::FormatType::HTML_LABEL_FOR : PrintArgs::FormatType::HTML_LABEL_FOR_COLON, forLabel, label);
        }
        else {
            output.print(label);
        }
    }
}

void WebUI::BaseUI::_printAttributeTo(PrintInterface &output) const
{
    _storage.for_each_if(_storage.begin(), Storage::Vector::isAttribute, [&](Storage::ConstIterator &iterator, Storage::TypeByte tb) {
        switch (tb.type()) {
            case Storage::Value::Attribute::type:
                while (--tb) {
                    auto attribute = Storage::Value::Attribute::pop_front<Storage::Value::Attribute>(iterator);
                    output.printf_P(PrintArgs::FormatType::HTML_ATTRIBUTE_STR_STR, attribute.getKey(), attribute.getValue());
                }
                break;
            case Storage::Value::AttributeInt::type:
                while (--tb) {
                    auto attribute = Storage::Value::AttributeInt::pop_front<Storage::Value::AttributeInt>(iterator);
                    output.printf_P(PrintArgs::FormatType::HTML_ATTRIBUTE_STR_INT, attribute.getKey(), attribute.getValue());
                }
                break;
            case Storage::Value::AttributeMinMax::type:
                if (true) {
                    auto attribute = Storage::Value::AttributeMinMax::pop_front<Storage::Value::AttributeMinMax>(iterator);
                    output.printf_P(PSTR(" min=\"%d\" max=\"%d\""), attribute.getMin(), attribute.getMax());
                    __LDBG_assert_printf(tb.count() == 1, "AttributeMinMax count=%u != 1", tb.count());
                }
                break;
            default:
                __LDBG_assert_printf(false, "invalid type %u: %s", tb.type(), tb.name());
                break;
        }
    });
    output.printf_P(PrintArgs::FormatType::HTML_CLOSE_TAG);
}

void WebUI::BaseUI::_printSuffixTo(PrintInterface &output) const
{
    _storage.for_each_if(_storage.begin(), Storage::Vector::isSuffix, [&](Storage::ConstIterator &iterator, Storage::TypeByte tb) {
        switch (tb.type()) {
            case Storage::Value::SuffixText::type:
                output.printf_P(PrintArgs::FormatType::HTML_OPEN_DIV_INPUT_GROUP_TEXT);
                Storage::SingleValueArgs<Storage::Value::SuffixText>(iterator, tb).print(output);
                output.printf_P(PrintArgs::FormatType::HTML_CLOSE_DIV_INPUT_GROUP_TEXT);
                break;
            case Storage::Value::SuffixHtml::type:
                Storage::SingleValueArgs<Storage::Value::SuffixHtml>(iterator, tb).print(output);
                break;
            case Storage::Value::SuffixOption::type:
                while (--tb) {
                    auto option = Storage::Value::SuffixOption::pop_front<Storage::Value::SuffixOption>(iterator);
                    output.printf_P(PrintArgs::FormatType::HTML_OPTION_STR_KEY, option.getKey(), option.getValue());
                }
                break;
            case Storage::Value::SuffixOptionNumKey::type:
                while (--tb) {
                    auto option = Storage::Value::SuffixOptionNumKey::pop_front<Storage::Value::SuffixOptionNumKey>(iterator);
                    output.printf_P(PrintArgs::FormatType::HTML_OPTION_NUM_KEY, option.getKey(), option.getValue());
                }
                break;
            default:
                __LDBG_assert_printf(false, "invalid type %u: %s", tb.type(), tb.name());
                break;
        }
    });
}

void WebUI::BaseUI::_printOptionsTo(PrintInterface &output) const
{
    _storage.for_each_if(_storage.begin(), Storage::Vector::isOption, [&](Storage::ConstIterator &iterator, Storage::TypeByte tb) {
        switch (tb.type()) {
            case Storage::Value::OptionNumKey::type:
                while (--tb) {
                    auto option = Storage::Value::OptionNumKey::pop_front<Storage::Value::OptionNumKey>(iterator);
                    auto format = _isSelected(option.getKey()) ? PrintArgs::FormatType::HTML_OPTION_NUM_KEY_SELECTED : PrintArgs::FormatType::HTML_OPTION_NUM_KEY;
                    output.printf_P(format, option.getKey(), option.getValue());
                }
                break;
            case Storage::Value::Option::type:
                while (--tb) {
                    auto option = Storage::Value::Option::pop_front<Storage::Value::Option>(iterator);
                    auto format = _compareValue(option.getKey()) ? PrintArgs::FormatType::HTML_OPTION_STR_KEY_SELECTED : PrintArgs::FormatType::HTML_OPTION_STR_KEY;
                    output.printf_P(format, option.getKey(), option.getValue());
                }
                break;
            default:
                __LDBG_assert_printf(false, "invalid type %u: %s", tb.type(), tb.name());
                break;
        }
    });
}

void WebUI::BaseUI::renderInputField(Type type, PrintInterface &output, const char *name, const String &value)
{
    // __DBG_printf("type=%u name=%s", _type, __S(name));
    switch (type) {
        // ---------------------------------------------------------------
        // Select field
        // ---------------------------------------------------------------
        case Type::SELECT:
        case Type::HIDDEN_SELECT:
            if (type == Type::SELECT) {
                output.printf_P(PrintArgs::FormatType::HTML_OPEN_SELECT, name, name);
            }
            else {
                output.printf_P(PrintArgs::FormatType::HTML_OPEN_HIDDEN_SELECT, name);
            }
            _printAttributeTo(output);
            _printOptionsTo(output);
            output.printf_P(PrintArgs::FormatType::HTML_CLOSE_SELECT);
            break;

        // ---------------------------------------------------------------
        // Input field
        // ---------------------------------------------------------------
        case Type::TEXT:
            output.printf_P(PrintArgs::FormatType::HTML_OPEN_TEXT_INPUT, name, name, encodeHtmlEntities(value));
            _printAttributeTo(output);
            break;

        case Type::TEXTAREA:
            output.printf_P(PrintArgs::FormatType::HTML_OPEN_TEXTAREA, name, name);
            _printAttributeTo(output);
            output.printf_P(PrintArgs::FormatType::HTML_CLOSE_TEXTAREA, encodeHtmlEntities(value));
            break;

        case Type::NUMBER:
            output.printf_P(PrintArgs::FormatType::HTML_OPEN_NUMBER_INPUT, name, name, encodeHtmlEntities(value));
            _printAttributeTo(output);
            break;

        case Type::NUMBER_RANGE:
            output.printf_P(PSTR("<div class=\"form-control input-text-range\"><input type=\"text\" name=\"%s\" id=\"%s\" value=\"%s\""), name, name, encodeHtmlEntities(value));
            _printAttributeTo(output);
            output.printf_P(PSTR("<input type=\"range\" class=\"custom-range\"></div>"));
            break;

        // ---------------------------------------------------------------
        // Password input fields
        // ---------------------------------------------------------------
        case Type::PASSWORD:
            output.printf_P(PSTR("<input type=\"password\" class=\"form-control visible-password\" name=\"%s\" id=\"%s\" value=\"%s\" autocomplete=\"current-password\" spellcheck=\"false\""),
                name,
                name,
                encodeHtmlEntities(value)
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
                encodeHtmlEntities(value),
                name,
                name
            );
            _printAttributeTo(output);
            break;
        case Type::RANGE_SLIDER:
            output.printf_P(PSTR(
                    "<div class=\"form-enable-slider\"><input type=\"range\" value=\"%s\" name=\"%s\" id=\"%s\""
                ),
                encodeHtmlEntities(value),
                name,
                name
            );
            _printAttributeTo(output);
            output.printf_P(PrintArgs::FormatType::HTML_CLOSE_DIV_RANGE_SLIDER);
            break;

        case Type::CHECKBOX:
            __LDBG_assert_printf(F("not implemented") == nullptr, "not implemented");
            //  <input type="checkbox" aria-label="Checkbox for following text input">
            break;

        case Type::HIDDEN:
            output.printf_P(PSTR("<input type=\"hidden\" name=\"%s\" id=\"%s\" value=\"%s\""), name, name, encodeHtmlEntities(value));
            _printAttributeTo(output);
        default:
            break;
    }
}

void WebUI::BaseUI::html(PrintInterface &output)
{
    auto &ui = _parent->getWebUIConfig();
    //__LDBG_printf("type=%u name=%s", _type, __S(_parent->getName()));

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
        name = encodeHtmlEntities(_parent->getName());
        break;
    }

    switch (_type) {
        // ---------------------------------------------------------------
        // Card Group
        // ---------------------------------------------------------------
        case Type::GROUP_START_CARD: {
                Group &group = reinterpret_cast<Group &>(*_parent);
                if (_hasLabel()) {
                    output.printf_P(PrintArgs::FormatType::HTML_OPEN_GROUP_START_CARD, name, name, name, _getLabel(), name, name, ui.getContainerId());
                    output.printf_P(group.isExpanded() ? PrintArgs::FormatType::HTML_OPEN_GROUP_START_CARD_COLLAPSE_SHOW : PrintArgs::FormatType::HTML_OPEN_GROUP_START_CARD_COLLAPSE_HIDE);
                }
                else {
                    output.printf_P(PrintArgs::FormatType::HTML_OPEN_DIV_CARD_DIV_DIV_CARD_BODY);
                }
            }
            break;
        case Type::GROUP_END_CARD: {
                output.printf_P(PrintArgs::FormatType::HTML_CLOSE_DIV_CARD_BODY_DIV_COLLAPSE_DIV_CARD);
            }
            break;

        // ---------------------------------------------------------------
        // HR Group
        // ---------------------------------------------------------------
        case Type::GROUP_START_HR: {
                if (_hasLabel()) {

                    // 2x open div, 1x open h5
                    output.printf_P(PSTR("<div class=\"form-row%s%s\"><div class=\"col-lg-12\"><h5>"), _parent->getNameType(), _parent->getNameForType());
                    _printLabelTo(output, nullptr);
                    // 1xclose h5, 1x close div , 1x open div, 2x close div, 1x open div form-group
                    output.printf_P(PrintArgs::FormatType::HTML_CLOSE_GROUP_START_HR);
                    // 1x open div form group-left
                }
                else {
                    // 2x open div, 2x close div, 1x open div form-group
                    output.printf_P(PSTR("<div class=\"form-row%s%s\"><div class=\"col-lg-12 mt-3\"><hr></div></div><div class=\"form-group\">"),
                        _parent->getNameType(),
                        _parent->getNameForType()
                    );
                    // 1x open div form group-left
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
                // 1x open div form-dependency-group
            } break;
        case Type::GROUP_END_HR:
        case Type::GROUP_END_DIV: {
                output.printf_P(PrintArgs::FormatType::HTML_CLOSE_DIV_FORM_DEPENDENCY_GROUP);
            } break;

        // ---------------------------------------------------------------
        // Default Group
        // ---------------------------------------------------------------
        case Type::GROUP_START: {
            Group &group = reinterpret_cast<Group &>(*_parent);
                // 1x open div
                output.printf_P(PSTR("<div class=\"form-group\"><button class=\"btn btn-secondary btn-block\" type=\"button\" data-toggle=\"collapse\" data-target=\"#%s\" aria-expanded=\"false\" aria-controls=\"%s\">"), name, name);
                _printLabelTo(output, nullptr);
                // 1x close div, 1x open div collapse, 1x open div card-body
                output.printf_P(PSTR("</button></div><div class=\"collapse%s\" id=\"%s\"><div class=\"card card-body mb-3\">"), group.isExpanded() ? PSTR(" show") : emptyString.c_str(), name);
            } break;
        case Type::GROUP_END: {
                output.printf_P(PrintArgs::FormatType::HTML_CLOSE_DIV_CARD_BODY_DIV_COLLAPSE);
            } break;

        // ---------------------------------------------------------------
        // Hidden input
        // ---------------------------------------------------------------
        case Type::HIDDEN:
            renderInputField(_type, output, name, _parent->getValue());
            break;

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

            renderInputField(_type, output, name, _parent->getValue());

            if (hasSuffix) {
                output.printf_P(PrintArgs::FormatType::HTML_OPEN_DIV_INPUT_GROUP_APPEND);
                _printSuffixTo(output);
                output.printf_P(PrintArgs::FormatType::HTML_CLOSE_DIV_INPUT_GROUP_APPEND_DIV_INPUT_GROUP_DIV_FORM_GROUP);
            }
            else {
                output.printf_P(PrintArgs::FormatType::HTML_CLOSE_DIV_FORM_GROUP);
            }

        } break;
    }
}
