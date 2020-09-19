/**
* Author: sascha_lammers@gmx.de
*/

#include <PrintArgs.h>
#include "WebUI/BaseUI.h"
#include "WebUI/Storage.h"
#include "Field/BaseField.h"
#include "Form/Form.hpp"

#include "Utility/Debug.h"

using namespace FormUI;

void Storage::Vector::validate(size_t offset, Field::BaseField *parent) const
{
    //Serial.printf_P(PSTR("VALIDATE name=%s offset=%u\n"), parent->getName().c_str(), offset);
    auto iterator = begin();
    std::advance(iterator, offset);
    while (iterator != end()) {
        //Serial.printf_P(PSTR("%p: "), this);
        TypeByte tb(iterator);
        //Serial.printf_P(PSTR("type=%s[cnt=%u] ofs=%u:%u\n"), tb.name(), tb.count(), std::distance(begin(), iterator), tb.size());
        iterator = tb.next(iterator);
    }
    //Serial.printf_P(PSTR("vector: size=%u capacity=%u\n"), size(), capacity());
}

void Storage::Vector::dump(size_t offset, Print &output) const
{
    auto iterator = begin();
    std::advance(iterator, offset);
    while (iterator != end()) {
        output.printf_P(PSTR("%p: "), this);
        TypeByte tb(iterator);
        output.printf_P(PSTR("type=%s[cnt=%u] ofs=%u:%u "), tb.name(), tb.count(), std::distance(begin(), iterator), tb.size());
        iterator = tb.next(iterator);
    }
    output.printf_P(PSTR(" vector: size=%u capacity=%u\n"), size(), capacity());
}

// -----------------------------------------------------------------------
// FormUI::CheckboxButtonSuffix
// -----------------------------------------------------------------------

void Container::CheckboxButtonSuffix::initButton(FormField &hiddenField, const __FlashStringHelper *onIcons, const __FlashStringHelper *offIcons)
{
    if (onIcons && offIcons) {
        _items.emplace_back(F("<button type=\"button\" class=\"button-checkbox btn btn-default\" data-on-icon=\"%s\" data-off-icon=\"%s\" id=\"_%s\">%s</button>"));
        _items.emplace_back(onIcons);
        _items.emplace_back(offIcons);
        _items.emplace_back(hiddenField.getName());
    }
    else {
        _items.emplace_back(F("<button type=\"button\" class=\"button-checkbox btn btn-default\" id=\"_%s\">%s</button>"));
        _items.emplace_back(hiddenField.getName());
    }
}
