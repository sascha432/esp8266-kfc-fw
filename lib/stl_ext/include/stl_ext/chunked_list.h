/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <stdint.h>

#ifndef XTRA_CONTAINERS_CHUNKED_LIST_ASSERT
#define XTRA_CONTAINERS_CHUNKED_LIST_ASSERT                     0
#endif

#if XTRA_CONTAINERS_CHUNKED_LIST_ASSERT
#include <assert.h>
#endif

// frees all objects in reverse order
#ifndef XTRA_CONTAINERS_CHUNKED_LIST_DESTROY_RECURSIVE
#define XTRA_CONTAINERS_CHUNKED_LIST_DESTROY_RECURSIVE          0
#endif

#ifndef XTRA_CONTAINERS_CHUNKED_LIST_ENABLE_ERASE
#define XTRA_CONTAINERS_CHUNKED_LIST_ENABLE_ERASE               0
#endif

#pragma push_macro("new")
#undef new

namespace STL_STD_EXT_NAMESPACE {

    // chunked_list is a memory efficient append only container
    // comparable to a forward_list that stores multiple elements in chunks and a pointer to the next chunk
    //
    // Elements in sequence containers are ordered in a strict linear sequence. Individual elements are accessed by their position in this sequence.
    // Each chunk keeps information on how to locate the next chunk, allowing constant time insert at the end, but no direct random access.
    //
    // iterators do not change
    // pointers to data do not change
    //
    // 32bit aligned:
    // chunked_list = 8 byte
    // chunk_size = sizeof(value_type) * elements + sizeof(size_type)
    //
    // if erase is enabled, it adds 4 byte per 32 elements to it (chunked_list_helpers::deleted_chunks::storage_type = uint32_t)
    //
    // NOTE: erase is not fully implemented yet
    // erase marks the item as removed, destructor is called, size() does not count the item anymore, iterator skips erased elements
    // TODO
    // - empty chunks are not removed
    // - deleted elements are not removed when the list is copied
    // - empty chunks are not removed when the list is copied
    // - reusing empty slots would change the order, but an empty chunk could be moved to the end
    // - begin can point to an erase element
    // - check if the compiler removes extra code (if constexpr is not available)
    // - add += operator for iterator
    //
    // methods:
    //
    // push_back()
    // emplace_back()
    //
    // begin()
    // end()
    // cbegin()
    // cend()
    //
    // size()
    // empty()
    // capacity() - of the current chunk
    //
    // clear()
    // swap()
    //
    // iterators:
    //
    // iterator
    // const_iterator
    //
    // works with std::swap, std:find, std::find_if, std::distance
    //

#if XTRA_CONTAINERS_CHUNKED_LIST_ENABLE_ERASE
    namespace chunked_list_helpers {

        template<typename _Tb, _Tb N>
        class deleted_chunks {
        public:
            using storage_type = uint32_t;
            static constexpr size_t storage_bits = sizeof(storage_type) * 8;
            static constexpr size_t BITS = (N + (storage_bits - 1)) / storage_bits;
            static constexpr bool value = true;

            deleted_chunks() : _bits{} {}

            bool _erased(size_t n) const {
                return (_bits[n / storage_bits] & _mask(n)) != 0;
            }
            bool _exists(size_t n) const {
                return (_bits[n / storage_bits] & _mask(n)) == 0;
            }
            void _erase(size_t n) {
                Serial.printf("ERASE %u\n",n);
                _bits[n / storage_bits] |= _mask(n);
            }
            void _reset(size_t n) {
                _bits[n / storage_bits] &= ~_mask(n);
            }
            bool _all_exist() const {
                size_t count = BITS - 1;
                do {
                    if (_bits[count]) {
                        return false;
                    }
                } while (count--);
                return true;
            }

        private:
            uint8_t _mask(size_t n) const {
                return 1 << (n % storage_bits);
            }
            storage_type _bits[BITS];
        };

        template<>
        struct deleted_chunks<int, 0> {
            static constexpr size_t BITS = 0;
            static constexpr bool value = false;

            bool _erased(size_t n) const {
                return false;
            }
            bool _exists(size_t n) const {
                return true;
            }
            bool _all_exist() const {
                return true;
            }
            void _erase(size_t n) {
            }
            void _reset(size_t n) {
            }
        };

    }
#endif

    template<class _Ta, size_t numElements, bool enableErase = false>
    class chunked_list {
    private:
        class chunk;

    public:
        using reference = _Ta &;
        using const_reference = const _Ta &;
        using pointer = _Ta *;
        using const_pointer = const _Ta *;
        using value_type = _Ta;
        using size_type = size_t;
        using difference_type = ptrdiff_t;
        using chunk_pointer = chunk *;
        using iterator_category = std::forward_iterator_tag;

#if XTRA_CONTAINERS_CHUNKED_LIST_ENABLE_ERASE
        static constexpr size_type erased_elements_size = enableErase * numElements;
#endif

    private:

        struct alignas(alignof(value_type)) value_storage_type {
        };

        class chunk_value_ptr {
        public:
            chunk_value_ptr() : _ptr(nullptr) {
            }

            chunk_value_ptr(pointer ptr) : _ptr(reinterpret_cast<void *>(ptr)) {
            }

            inline chunk_pointer chunk(chunk_pointer chunk) {
                return reinterpret_cast<chunk_pointer>(_ptr = reinterpret_cast<void *>(chunk));
            }

            inline chunk_pointer chunk() const {
                return reinterpret_cast<chunk_pointer>(_ptr);
            }

            inline pointer forward() {
                return reinterpret_cast<pointer &>(_ptr)++;
            }

            inline pointer value() const {
                return reinterpret_cast<pointer>(_ptr);
            }

        private:
            void *_ptr;
        };

#if XTRA_CONTAINERS_CHUNKED_LIST_ENABLE_ERASE
        using deleted_chunk_type = chunked_list_helpers::deleted_chunks<int, erased_elements_size>;

        class chunk : private deleted_chunk_type {
#else
        class chunk {
#endif
        public:

            chunk() : _ptr(_begin()) {
            }

            chunk(chunk_pointer source, chunk_pointer &last) : _ptr(_begin()) {
                if (source) {
                    // copy all objects
                    for (auto iterator = source->_begin(); iterator != source->_lastElement(); ++iterator) {
                        ::new(static_cast<void *>(_ptr.forward())) value_type(const_cast<const_reference>(*iterator));
                    }
                    if (source->_hasChildren()) {
                        // create a new chunk and copy all objects from children
                        _ptr.chunk(::new chunk(source->_ptr.chunk(), last));
                    }
                    else {
                        // set last chunk
                        last = this;
                    }
                }
            }

            ~chunk() {
                _destroy();
            }

            size_type size() const {
#if XTRA_CONTAINERS_CHUNKED_LIST_ENABLE_ERASE
                if (deleted_chunk_type::value && deleted_chunk_type::_all_exist() == false) {
                    size_type count = 0;
                    size_type n = _size() - 1;
                    for (auto data = _lastElement() - 1; data >= _cbegin(); --data, --n) {
                        if (deleted_chunk_type::_exists(n)) {
                            count++;
                        }
                    }
                    return count;
                }
#endif
                return _size();
            }

            constexpr size_type capacity() {
                return numElements;
            }

        private:
            inline size_type _size() const {
                return _lastElement() - _cbegin();
            }

            inline void _destroy() {
#if XTRA_CONTAINERS_CHUNKED_LIST_ENABLE_ERASE
                if (deleted_chunk_type::value && deleted_chunk_type::_all_exist() == false) {
                    size_type n = _size() - 1;
                    for (auto data = _lastElement() - 1; data >= _begin(); --data, --n) {
                        if (deleted_chunk_type::_exists(n)) {
                            data->~value_type();
                        }
                    }
                }
                else 
#endif
                {
                    for (auto data = _lastElement() - 1; data >= _begin(); --data) {
                        data->~value_type();
                    }
                }
            }

#if XTRA_CONTAINERS_CHUNKED_LIST_DESTROY_RECURSIVE
            void _destroyRecursive() {
                if (_hasChildren()) {
                    _ptr.chunk()._destroyRecursive();
                    delete _ptr.chunk();
                }
                _destroy();
            }
#endif

        private:
            inline bool _hasChildren() const {
                return !_isDataPtr();
            }

            chunk_pointer _getNextChunk() const {
                return _hasChildren() ? _ptr.chunk() : nullptr;
            }

            bool _isDataPtr(pointer ptr) const {
                return (ptr >= _cbegin()) && (ptr <= _cend());
            }

            inline bool _isDataPtr() const {
                return _isDataPtr(_ptr.value());
            }

            inline pointer _begin() {
                return const_cast<pointer>(_cbegin());
            }

            inline pointer _end() {
                return const_cast<pointer>(_cend());
            }

            inline const_pointer _cbegin() const {
                return static_cast<const_pointer>(reinterpret_cast<const void *>(&_data[0]));
            }

            inline const_pointer _cend() const {
                return static_cast<const_pointer>(reinterpret_cast<const void *>(&_data[0])) + numElements;
            }

            inline pointer _lastElement() {
                return _isDataPtr() ? _ptr.value() : _end();
            }

            inline const_pointer _lastElement() const {
                return _isDataPtr() ? _ptr.value() : _cend();
            }

        private:
            friend chunked_list;

            chunk_value_ptr _ptr;
            uint8_t _data[numElements * sizeof(value_type)];
        };

        class iterator_base : public non_std::iterator<iterator_category, value_type, difference_type, pointer, reference> {
        public:
            iterator_base() = default;

            iterator_base(chunk_pointer chunk, pointer next) : _chunk(chunk), _next(next) {
            }

            iterator_base &operator++() {
#if XTRA_CONTAINERS_CHUNKED_LIST_ASSERT
                assert(_chunk);
                assert(_chunk->_isDataPtr(_next));
#endif
#if XTRA_CONTAINERS_CHUNKED_LIST_ENABLE_ERASE
                if (deleted_chunk_type::value) {
                    while(true) {
                        __next();
                        size_type n = _next - _chunk->_cbegin();
                        Serial.printf("TEST %d next=%p exists=%u\n",n,_next, _next?_chunk->_exists(n):-1);
                        if (_next == nullptr || _chunk->_exists(n)) {
                            break;
                        }
                    }
                }
                else 
#endif
                {
                    __next();
                }
                return *this;
            }

            iterator_base operator+=(int step) {
#if XTRA_CONTAINERS_CHUNKED_LIST_ASSERT
                assert(step >= 0);
#endif
                while (step > 0) {
                    _chunk->_isDataPtr();
                }
            }

            bool operator !=(const iterator_base &iterator) const {
                return _chunk != iterator._chunk || _next != iterator._next;
            }

            bool operator ==(const iterator_base &iterator) const {
                return _chunk == iterator._chunk && _next == iterator._next;
            }

            const_pointer operator->() const {
                return get();
            }

            const_reference operator*() const {
                return *get();
            }

            const_pointer get() const {
                return _get();
            }

        protected:
            friend chunked_list;

            void __next() {
                // check if the last element has been reached
                if (++_next == _chunk->_lastElement()) {
                    // proceed with next chunk
                    _chunk = _chunk->_getNextChunk();
                    _next = _chunk ? _chunk->_begin() : nullptr;
                }
            }

#if XTRA_CONTAINERS_CHUNKED_LIST_ENABLE_ERASE
            void erase() const {
                size_type n = _next - _chunk->_cbegin();
                _chunk->_erase(n);
                get()->~value_type();
            }
#endif

        protected:
            inline pointer _get() const {
#if XTRA_CONTAINERS_CHUNKED_LIST_ASSERT
                assert(_chunk);
                assert(_chunk->_isDataPtr(_next));
#endif
                return _next;
            }

        private:
            chunk_pointer _chunk;
            pointer _next;
        };

    public:
        using const_iterator = iterator_base;

        class iterator : public iterator_base {
        public:
            using iterator_base::iterator_base;
            using iterator_base::_get;

            pointer operator->() {
                return get();
            }

            reference operator*() {
                return *get();
            }

            pointer get() {
                return _get();
            }
        };

        static constexpr size_t chunk_size = sizeof(chunk);

        chunked_list() :
            _firstChunk(nullptr),
            _lastChunk(nullptr)
            //_size(0)
        {
        }
        ~chunked_list() {
            _destroy();
        }

        chunked_list(chunked_list &&list) noexcept :
            _firstChunk(std::exchange(list._firstChunk, nullptr)),
            _lastChunk(std::exchange(list._lastChunk, nullptr))
            //_size(std::exchange(list._size, 0))
        {
        }

        chunked_list(const chunked_list &list) : chunked_list()
        {
            *this = list;
        }

        chunked_list &operator=(const chunked_list &list) {
            clear();
            if (list._firstChunk) {
                _firstChunk = ::new chunk(list._firstChunk, _lastChunk);
            }
            //_size = list._size;
            return *this;
        }

        chunked_list &operator=(chunked_list &&list) noexcept {
            clear();
            ::new(static_cast<void *>(this)) chunked_list(std::move(list));
            return *this;
        }

        inline void swap(chunked_list &list) {
            std::swap(*this, list);
        }

        inline size_type size() const {
            size_t count = 0;
            auto chunk = _firstChunk;
            while (chunk) {
                count += chunk->size();
                chunk = chunk->_getNextChunk();
            }
            return count;
        }

        inline bool empty() const {
            return _firstChunk == nullptr;
        }

        inline size_type capacity() const {
            size_t count = 0;
            auto chunk = _firstChunk;
            while (chunk) {
                count += numElements;
                chunk = chunk->_getNextChunk();
            }
            return count;
        }

        inline reference front() {
#if XTRA_CONTAINERS_CHUNKED_LIST_ASSERT
            assert(_firstChunk);
#endif
            return *_firstChunk->_begin();
        }

        inline reference back() {
#if XTRA_CONTAINERS_CHUNKED_LIST_ASSERT
            assert(_lastChunk);
#endif
            return *(_lastChunk->_lastElement() - 1);
        }

        inline void push_back(const _Ta &data) {
            emplace_back(data);
        }

        inline void push_back(_Ta &&data) {
            emplace_back(std::move(data));
        }

        template<typename... Args>
        void emplace_back(Args &&... args) {
            if (!_firstChunk) {
                // create first chunk
                _firstChunk = ::new chunk();
                _lastChunk = _firstChunk;
            }
            else {
                // chunk is full, create a new chunk
#if XTRA_CONTAINERS_CHUNKED_LIST_ASSERT
                assert(_lastChunk->_ptr.value() == _lastChunk->_lastElement());
#endif
                if (_lastChunk->_ptr.value() == _lastChunk->_end()) {
                    _lastChunk = _lastChunk->_ptr.chunk(::new chunk());
                }
            }
            // emplace data
            ::new(static_cast<void *>(_lastChunk->_ptr.forward())) value_type(std::forward<Args>(args)...);
            //_size++;
        }

        void clear() {
            if (_firstChunk) {
                _destroy();
                _firstChunk = nullptr;
                _lastChunk = nullptr;
                //_size = 0;
            }
        }

        iterator begin() {
            return iterator(_firstChunk, _first());
        }
        iterator end() {
            return iterator();
        }

        const_iterator begin() const {
            return const_iterator(_firstChunk, _first());
        }
        const_iterator end() const {
            return const_iterator();
        }

        const_iterator cbegin() const {
            return begin();
        }
        const_iterator cend() const {
            return end();
        }

#if XTRA_CONTAINERS_CHUNKED_LIST_ENABLE_ERASE
        const_iterator erase(const_iterator element) {
            auto next = element;
            ++next;
            element.erase();
            return next;
        }

        const_iterator erase(const_iterator first, const_iterator last) {
            const_iterator iterator;
            for (iterator = first; iterator != last; iterator = erase(iterator)) {
            }
            return iterator;
        }
#endif

    private:
        void _destroy() {
#if XTRA_CONTAINERS_CHUNKED_LIST_DESTROY_RECURSIVE
            if (_firstChunk) {
                _firstChunk->_destroyRecursive();
            }
#else
            auto chunk = _firstChunk;
            while (chunk) {
                auto nextChunk = chunk->_getNextChunk();
                delete chunk;
                chunk = nextChunk;
            }
#endif
        }

        pointer _first() const {
            return _firstChunk ? _firstChunk->_begin() : nullptr;
        }
    private:
        chunk_pointer _firstChunk;
        chunk_pointer _lastChunk;
        //size_type _size;
    };

    template<class _Ta>
    class chunk_list_size {
    public:
        static size_t getSize(uint16_t numElements) {
            return sizeof(void *) + sizeof(_Ta) * numElements;
        }

        static void list(Print &output, uint16_t from, uint16_t to, uint16_t alignment) {
            for (int i = from; i <= to; i++) {
                auto n = chunk_list_size<_Ta>::getSize(i);
                if (!alignment || (alignment && (n % alignment == 0))) {
                    output.printf("elements=%u per_chunk=%u alignment=%u\n", i, n, alignment ? (n % alignment) : n % 16);
                }
            }
        }
    };
};

#pragma pop_macro("new")
