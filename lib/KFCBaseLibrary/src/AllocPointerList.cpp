/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include <DumpBinary.h>
#include "AllocPointerList.h"

#if _MSC_VER || 1
static const uint32_t kHeapStartAddress = 0;
static constexpr uint8_t kHeapAlignment = 1;
static const uint32_t kProgmemStartAddress = 0;
static constexpr uint8_t kProgmemAlignment = 1;
#else
extern char _irom0_text_start[];
extern char _heap_start[];
static const uint32_t kHeapStartAddress = (uint32_t)&_heap_start[0];
static constexpr uint8_t kHeapAlignment = 1; // sizeof(uint32_t);
static const uint32_t kProgmemStartAddress = (uint32_t)&_irom0_text_start[0];
static constexpr uint8_t kProgmemAlignment = sizeof(uint32_t);
#endif




AllocPointerInfo::AllocPointerInfo() :
    _size(0),
    _type(0),
    _line(0),
    _ptr(kNullPointer),
    _file(kNullPointer)
{
}

AllocPointerInfo::AllocPointerInfo(const void *ptr, size_t size, const char *file, uint32_t line) :
    _size((uint16_t)size),
    _type(0),
    _line(line),
    _ptr(compress(ptr)),
    _file(compress_P(file))
{
}

AllocPointerInfo::PointerType AllocPointerInfo::compress(const void *ptr)
{
    return _compress(ptr, kHeapStartAddress, kHeapAlignment);
}

void *AllocPointerInfo::uncompress(PointerType ptr)
{
    return _uncompress(ptr, kHeapStartAddress, kHeapAlignment);
}

AllocPointerInfo::PointerType AllocPointerInfo::compress_P(const void *ptr)
{
    return _compress(ptr, kProgmemStartAddress, kProgmemAlignment);
}

void *AllocPointerInfo::uncompress_P(PointerType ptr)
{
    return _uncompress(ptr, kProgmemStartAddress, kProgmemAlignment);
}

AllocPointerInfo::PointerType AllocPointerInfo::_compress(const void *ptr, size_t startAddress, uint8_t alignment)
{
    if (ptr == nullptr) {
        return kNullPointer;
    }
    return ((uint32_t)ptr - startAddress) / alignment;
}

void *AllocPointerInfo::_uncompress(PointerType ptr, size_t startAddress, uint8_t alignment)
{
    if (ptr == kNullPointer) {
        return nullptr;
    }
    return (void *)((ptr * alignment) + startAddress);
}

void *AllocPointerInfo::getPointer() const
{
    return uncompress(_ptr);
}

void AllocPointerInfo::setPointer(const void *ptr)
{
    _ptr = compress(ptr);
}

size_t AllocPointerInfo::getSize() const
{
    return _size;
}

const char *AllocPointerInfo::getFile() const
{
    return (const char *)uncompress_P(_file);
}

uint32_t AllocPointerInfo::getLine() const
{
    return _line;
}

void AllocPointerInfo::printTo(Print &output) const
{
    output.printf_P(PSTR("%08x:%04x(% 5.5u) %02x"), uncompress(_ptr), _size, _size, _type);
}

void AllocPointerInfo::printLocationTo(Print &output) const
{
    output.printf_P(PSTR("%s:%u"), uncompress_P(_file), _line);
}

void AllocPointerInfo::dump(Print &output) const
{
    printTo(output);
    output.print(' ');
    printLocationTo(output);
    output.println();
}

void AllocPointerInfo::setNoLeak(bool state)
{
    _type = state ? 1 : 0;
}



AllocPointerList::AllocPointerList()
{
}

AllocPointerList::Iterator AllocPointerList::find(const void *ptr)
{
    return std::find(_list.begin(), _list.end(), ptr);
}

AllocPointerInfo *AllocPointerList::add(const void *ptr, size_t size, const char *file, uint32_t line)
{
    // find free spot
    auto iterator = std::find(_list.begin(), _list.end(), nullptr);
    if (iterator != _list.end()) {
        auto &tmp = *iterator;
        tmp = std::move(AllocPointerInfo(ptr, size, file, line));
        return &tmp;
    }
    _list.emplace_back(ptr, size, file, line);
    return &_list.back();
}

bool AllocPointerList::remove(const void *ptr)
{
    auto iterator = find(ptr);
    if (isValid(iterator)) {
        iterator->setPointer(nullptr);
        return true;
    }
    return false;
}

void AllocPointerList::dump(Print &output, size_t dumpBinarySize, AllocType type)
{
    DumpBinary dump(output);
    dump.disablePerLine();

    output.println(F("---"));
    output.printf_P(PSTR("count=%u capacity=%u size=%u\n"), _list.size(), _list.capacity(), sizeof(_list[0]) * _list.capacity());
    for (const auto &info : _list) {
        if (info == type) {
            info.printTo(output);
            output.print(' ');
            dump.dump(info.getPointer(), std::min(info.getSize(), dumpBinarySize));
            output.printf_P(PSTR(" (%s:%u)\n"), info.getFile(), info.getLine());
        }
    }
}

void AllocPointerList::markAllNoLeak(bool state)
{
    for (auto &info : _list) {
        info.setNoLeak(state);
    }
}

void AllocPointerList::dump(Print &output, AllocType type)
{
    output.println(F("---"));
    for (const auto &info : _list) {
        if (info == type) {
            info.printTo(output);
            output.printf_P(PSTR(" (%s:%u)\n"), info.getFile(), info.getLine());
        }
    }
}

