/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include "AllocPointerList.h"

class KFCMemoryDebugging : public AllocPointerList
{
public:
    template<class T>
    static T _new(const DebugContext &p, size_t s, T t) {
        _newCount++;
        return _alloc(p, s, t);
    }

    template<class T>
    static T _delete(const DebugContext &p, T t) {
        _deleteCount++;
        return _free(p, t);
    }

    template<class T>
    static T _alloc(DebugContext p, size_t s, T t) {
        _add(p, t, s);
        return t;
    }

    static char *_alloc(const DebugContext &p, char *str) {
        _add(p, str, strlen_P(str));
        return str;
    }

    template<class T>
    static T _realloc(const DebugContext &p, size_t s, T r, T t) {
        _reallocCount++;
        _free(p, r);
        return _alloc(p, s, t);
    }

    template<class T>
    static T _free(const DebugContext &p, T t) {
        _remove(p, t);
        return t;
    }

    static void _add(const DebugContext &p, const void *ptr, size_t size);
    static void _remove(const DebugContext &p, const void *ptr);

    void __add(const DebugContext &p, const void *ptr, size_t size);
    void __remove(const DebugContext &p, const void *ptr);
    void __dump(Print &output, size_t dumpBinaryMaxSize, AllocType type);
    // mark all allocated blocks as no leak. dump(..., AllocType::LEAK) will show only show
    // blocks that have been allocated after the call
    void __markAllNoLeak();

    static void reset();
    static void dump(Print &output, AllocType type);
    static void markAllNoLeak();

    static KFCMemoryDebugging &getInstance();

private:
    static uint32_t _newCount;
    static uint32_t _deleteCount;

    // include new and delete count
    static uint32_t _allocCount;       // includes _reallocCount
    static uint32_t _reallocCount;
    static uint32_t _freeCount;

    static uint32_t _failCount;
    static uint32_t _freeNullCount;

    static KFCMemoryDebugging _instance;
};


