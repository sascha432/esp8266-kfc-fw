/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <push_pack.h>

class AllocPointerInfo {
public:
#if _MSC_VER || 1
    using PointerType = uintptr_t;
    static constexpr PointerType kNullPointer = (PointerType)nullptr;
#else
    using PointerType = uint16_t;
    static constexpr PointerType kNullPointer = ~0;
#endif

    enum class AllocType {
        ALL = 0,
        LEAK = 1,
        FREED = 2,
    };

public:
    AllocPointerInfo();
    AllocPointerInfo(const void *ptr, size_t size, const char *file, uint32_t line);

    static PointerType compress(const void *ptr);
    static void *uncompress(PointerType ptr);

    static PointerType compress_P(const void *ptr);
    static void *uncompress_P(PointerType ptr);

    static PointerType _compress(const void *ptr, size_t startAddress, uint8_t alignment);
    static void *_uncompress(PointerType ptr, size_t startAddress, uint8_t alignment);

    operator const void *() const {
        return uncompress(_ptr);
    }
    operator void *() {
        return uncompress(_ptr);
    }

    bool operator==(AllocType type) const {
        if (type != AllocType::FREED && _ptr == kNullPointer) {
            return false;
        }
        if (type == AllocType::LEAK && _type == 1) {
            return false;
        }
        return true;
    }

    void *getPointer() const;
    void setPointer(const void *ptr);
    size_t getSize() const;
    const char *getFile() const;
    uint32_t getLine() const;
    void printTo(Print &output) const;
    void printLocationTo(Print &output) const;
    void dump(Print &output) const;
    void setNoLeak(bool state);

private:
    struct __attribute__packed__ {
        uint32_t _size: 15;
        uint32_t _type : 1;
        uint32_t _line: 16;
        PointerType _ptr;
        PointerType _file;
    };
};

class AllocPointerList {
public:
    using AllocPointerInfoVector = std::vector<AllocPointerInfo>;
    using Iterator = AllocPointerInfoVector::iterator;
    using AllocType = AllocPointerInfo::AllocType;

    AllocPointerList();

    inline bool has(const void *ptr) {
        return isValid(find(ptr));
    }
    inline bool isValid(Iterator iterator) const {
        return iterator != _list.end();
    }

    Iterator find(const void *ptr);
    AllocPointerInfo *add(const void *ptr, size_t size, const char *file, uint32_t line);
    bool remove(const void *ptr);
    void dump(Print &output, AllocType type);
    void dump(Print &output, size_t dumpBinarySize, AllocType type);
    void markAllNoLeak(bool state);

private:
    AllocPointerInfoVector _list;
};

#include <pop_pack.h>
