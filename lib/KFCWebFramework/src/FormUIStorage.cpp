/**
* Author: sascha_lammers@gmx.de
*/

#include "FormUI.h"
#include "FormUIStorage.h"
#include "FormField.h"
#include <PrintArgs.h>

#if DEBUG_KFC_FORMS
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using namespace FormUI;

void ItemsStorage::PrintValue::print(ConstIterator &iterator, PrintInterface &output)
{
    auto byte = *iterator;
    auto type = getType(byte);
    auto count = (byte >> 4);
    ++iterator;
    if (count == 0) {
        output.printf_P(PrintArgs::FormatType::SINGLE_STRING, *(const char **)(&(*(iterator))));
        iterator += getSizeOf(type);
    }
    else {
        __DBG_assert_printf(getSizeOf(type) == sizeof(uintptr_t), "size=%u does not match pointer size=%u", getSizeOf(type), sizeof(uintptr_t));
        output.vprintf_P((uintptr_t **)(&(*iterator)), count);//argsIterator - &args[1]);
        iterator += (count + 1) * getSizeOf(type);
    }
}

ItemsStorage::ConstIterator ItemsStorage::find(ConstIterator iterator, ConstIterator end, Pred pred) const 
{
    while (iterator != end) {
        auto byte = *iterator;
        auto count = (byte >> 4) + 1;
        auto type = static_cast<StorageType>(byte & 0xf);
        if (pred(type)) {
            return iterator;
        }
        iterator += (getSizeOf(type) * count) + 1;
    }
    return end;
}

CheckboxButtonSuffix::CheckboxButtonSuffix(FormField &hiddenField, const char *label, const __FlashStringHelper *onIcons, const __FlashStringHelper *offIcons)
{
    if (onIcons && offIcons) {
        _items.push_back(F("<button type=\"button\" class=\"btn btn-default button-checkbox\" data-on-icon=\"%s\" data-off-icon=\"%s\" id=\"_%s\">%s</button>"));
        _items.push_back(onIcons);
        _items.push_back(offIcons);
        _items.push_back(hiddenField.getName().c_str());
        _items.push_back(label);
    }
    else {
        _items.push_back(F("<button type=\"button\" class=\"btn btn-default button-checkbox\" id=\"_%s\">%s</button>"));
        _items.push_back(hiddenField.getName().c_str());
        _items.push_back(label);
    }
}
