/**
* Author: sascha_lammers@gmx.de
*/

#include "WebUI/BaseUI.h"
#include "WebUI/Storage.h"
#include "Field/BaseField.h"
#include <PrintArgs.h>

#if DEBUG_KFC_FORMS
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using namespace FormUI;

//// -----------------------------------------------------------------------
//// FormUI::ItemsStorage::PrintValue
//// -----------------------------------------------------------------------
//
//#if DEBUG_STORAGE_VECTOR
//
//void Storage::Value::PrintHelper::verify_iterator(const DebugContext &ctx, const Vector &storage, ConstIterator iterator, size_t size, Type type, uint8_t count)
//{
//#if _MSC_VER
//    auto iterPtr = iterator._Ptr;
//#else
//    auto iterPtr = (uint8_t *)&(*(iterator));
//#endif
//    auto iterPtr2 = iterPtr + size;
//    auto beginSt = (uint8_t *)storage.data();
//    auto endSt = beginSt + storage.size();
//    TypeByte tb(type, count);
//
//    bool cond = (iterPtr <= endSt && beginSt <= iterPtr && iterPtr2 <= endSt && beginSt <= iterPtr2);
//    if (!cond) {
//        storage.dump(DEBUG_OUTPUT);
//        ctx.printStack();
//    }
//
//    __LDBG_assert_printf(cond,
//        "%sout of range: vector=%p %p-%p iterator=%p[%u<%u] + %u=%p[%u<%u] type=%u[%s] size=%u * %u=%u",
//        ctx.getPrefix().c_str(), &storage, beginSt, endSt,
//        iterPtr, iterPtr - beginSt, storage.size(),
//        size,
//        iterPtr2, iterPtr2 - beginSt, storage.size(),
//        tb.type(), tb.name(), tb.item_size(), tb.count(), tb.size()
//
//    );
//}
//
//#endif
//
//void Storage::Value::PrintHelper::print(IF_DEBUG_STORAGE_VECTOR(const Storage::Vector &vector, ) ConstIterator &iterator, PrintInterface &output)
//{
//    TypeByte tb(iterator);
//    uint8_t count = tb.count();
//
//    DEBUG_STORAGE_RANGE_CHECK(vector, iterator, 0, tb.type(), count);
//    __DBG_assert_printf(count < Storage::kTypeMaxCount, "count=%u not less than max=%u", count, Storage::kTypeMaxCount);
//    __DBG_assert_printf(tb.item_size() == sizeof(Storage::Value::String), "size=%u does not match Storage::Value::String=%u", tb.item_size(), sizeof(Storage::Value::String));
//    DEBUG_STORAGE_RANGE_CHECK(vector, iterator, tb.size(), tb.type(), count);
//
//    auto str = Vector::pop_front<Storage::Value::String>(iterator);
//    if (count == 1) {
//        output.printf_P(PrintArgs::FormatType::SINGLE_STRING, str.getValue());
//    }
//    else {
//        const char *args[Storage::kTypeMaxCount] = { str.getValue() };
//        auto argsPtr = &args[1];
//        *argsPtr++ = str.getValue();
//
//        while (count--) {
//            str = Storage::Value::String::pop_front<Storage::Value::String>(iterator);
//        }
//
//        output.vprintf_P((uintptr_t **)(args), argsPtr - args);
//    }
//}

void FormUI::Storage::Vector::dump(Print &output) const
{
    auto iterator = begin();
    while (iterator != end()) {
        output.printf_P(PSTR("%p: "), this);
        TypeByte tb(iterator);
        output.printf_P(PSTR("type=%s[cnt=%u] ofs=%u:%u "), tb.name(), tb.count(), std::distance(begin(), iterator), tb.size());
        iterator = tb + iterator;
    }
    output.printf(" vector: size=%u capacity=%u\n", size(), capacity());
}

//Storage::ConstIterator Storage::Vector::find(ConstIterator iterator, ConstIterator end, Pred pred) const
//{
//    if (iterator != Vector::end()) {
//        String str;
//        bool match = false;
//        for (auto iter = begin(); iter != Vector::end(); ) {
//            TypeByte tb(iter);
//            int ofs = std::distance(begin(), iter);
//            str += String(ofs);
//            str += ',';
//            if (ofs == std::distance(begin(), iterator)) {
//                match = true;
//                break;
//            }
//            iter = tb + iter;
//        }
//        str += String(size());
//        __DBG_assert_printf(match == true, "%p: iterator has invalid offset %u (%s)", this, std::distance(begin(), iterator), str.c_str());
//    }
//
//    while (iterator != end) {
//        TypeByte tb(iterator);
//        if (pred(tb.type())) {
//            return iterator;
//        }
//        DEBUG_STORAGE_RANGE_CHECK(*this, iterator, (iterator != end) ? tb.size() : 0, tb.type(), tb.count());
//        iterator = tb + iterator;
//    }
//    return iterator;
//}

// -----------------------------------------------------------------------
// FormUI::CheckboxButtonSuffix
// -----------------------------------------------------------------------

Container::CheckboxButtonSuffix::CheckboxButtonSuffix(Field::BaseField &hiddenField, const char *label, const __FlashStringHelper *onIcons, const __FlashStringHelper *offIcons)
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
