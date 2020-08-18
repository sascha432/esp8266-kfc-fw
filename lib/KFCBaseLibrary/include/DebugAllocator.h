/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if HAVE_MEM_DEBUG

template<class T>
class __DebugAllocator : public std::allocator<T>
{
public:
    using AllocatorBase = std::allocator<T>;
    using AllocatorBase::AllocatorBase;
    using pointer = typename AllocatorBase::pointer;
    using size_type = typename AllocatorBase::size_type;

    template<typename R>
    struct rebind
    {
        typedef __DebugAllocator<R> other;
    };

    pointer allocate(size_type __n, const void * = nullptr)
    {
        return static_cast<T *>(__DBG_operator_new(__n * sizeof(T)));
    }

    void deallocate(pointer __p, size_type)
    {
        __DBG_operator_delete(__p);
    }

};

template<class T>
class __TrackAllocator : public std::allocator<T>
{
public:
    using AllocatorBase = std::allocator<T>;
    using AllocatorBase::AllocatorBase;
    using pointer = typename AllocatorBase::pointer;
    using size_type = typename AllocatorBase::size_type;

    template<typename R>
    struct rebind
    {
        typedef __TrackAllocator<R> other;
    };

    pointer allocate(size_type __n, const void * = nullptr)
    {
        KFCMemoryDebugging::_track_new(__n * sizeof(T));
        return static_cast<T *>(__NDBG_operator_new(__n * sizeof(T)));
    }

    void deallocate(pointer __p, size_type __n)
    {
        KFCMemoryDebugging::_track_delete(__p, __n * sizeof(T));
        __NDBG_operator_delete(__p);
    }

};

#endif
