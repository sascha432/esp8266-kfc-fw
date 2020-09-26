/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <stdint.h>
#if defined(ARDUINO)
#include <Print.h>
#endif

#ifndef XTRA_CONTAINERS_CHUNKED_LIST_ASSERT
#define XTRA_CONTAINERS_CHUNKED_LIST_ASSERT                     0
#endif

#if XTRA_CONTAINERS_CHUNKED_LIST_ASSERT
#include <assert.h>
#endif

#ifndef XTRA_CONTAINERS_CHUNKED_LIST_ENABLE_ERASE
#define XTRA_CONTAINERS_CHUNKED_LIST_ENABLE_ERASE               0
#endif

#pragma push_macro("new")
#undef new

namespace STL_STD_EXT_NAMESPACE_EX {

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
    // capacity()
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

    template<class _Ty, size_t numElements/*, bool enableErase = false*/>
    class chunked_list {
    private:
        class chunk;

    public:
        using value_type = _Ty;
        using reference = _Ty &;
        using const_reference = const _Ty &;
        using pointer = _Ty *;
        using const_pointer = const _Ty *;
        using size_type = size_t;
        using difference_type = ptrdiff_t;
        using iterator_category = std::forward_iterator_tag;
        using chunk_pointer = chunk *;

#if XTRA_CONTAINERS_CHUNKED_LIST_ENABLE_ERASE
        static constexpr size_type erased_elements_size = enableErase * numElements;
#endif

    private:

        struct alignas(value_type) value_storage_type {
            union {
                uint8_t _storage[1];
                value_type _value;
            };
            value_storage_type() {}
            ~value_storage_type() {}
        };

        class chunk_value_ptr {
        public:
            chunk_value_ptr() : _ptr(nullptr) {
            }

            chunk_value_ptr(pointer ptr) : _ptr(reinterpret_cast<value_storage_type *>(ptr)) {
            }

            inline chunk_pointer chunk(chunk_pointer chunk) {
                return reinterpret_cast<chunk_pointer>(_ptr = reinterpret_cast<value_storage_type *>(chunk));
            }

            inline chunk_pointer chunk() const {
                return reinterpret_cast<chunk_pointer>(_ptr);
            }

            inline void *forward() {
                return reinterpret_cast<void *>(_ptr++);
            }

            inline pointer value() const {
                return reinterpret_cast<pointer>(_ptr);
            }

        private:
            value_storage_type *_ptr;
        };

#if XTRA_CONTAINERS_CHUNKED_LIST_ENABLE_ERASE
        using deleted_chunk_type = chunked_list_helpers::deleted_chunks<int, erased_elements_size>;

        class chunk : private deleted_chunk_type {
#else
        class chunk {
#endif
        public:

            chunk() : _ptr(begin()) {
            }

            ~chunk() {
                _destroy();
            }

            size_type size() const {
#if XTRA_CONTAINERS_CHUNKED_LIST_ENABLE_ERASE
                if (deleted_chunk_type::value && deleted_chunk_type::_all_exist() == false) {
                    size_type count = 0;
                    size_type n = _size() - 1;
                    for (auto data = _last() - 1; data >= _cbegin(); --data, --n) {
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
                return _last() - _cbegin();
            }

            inline void _destroy() {
#if XTRA_CONTAINERS_CHUNKED_LIST_ENABLE_ERASE
                if (deleted_chunk_type::value && deleted_chunk_type::_all_exist() == false) {
                    size_type n = _size() - 1;
                    for (auto data = _last() - 1; data >= begin(); --data, --n) {
                        if (deleted_chunk_type::_exists(n)) {
                            data->~value_type();
                        }
                    }
                }
                else
#endif
                for(auto &value: *this) {
                    value.~value_type();
                }
            }

            pointer begin() {
                return const_cast<pointer>(_cbegin());
            }

            pointer end() {
                return const_cast<pointer>(_last());
            }

            const_pointer cbegin() {
                return _cbegin();
            }

            const_pointer cend() {
                return _last();
            }

        private:
            inline bool has_children() const {
                return !is_pointer_to_data();
            }

            chunk_pointer next_chunk() const {
                return is_pointer_to_data() ? nullptr : _ptr.chunk();
            }

            bool is_pointer_to_data(pointer ptr) const {
                return (ptr >= _cbegin()) && (ptr <= _cend());
            }

            inline bool is_pointer_to_data() const {
                return is_pointer_to_data(_ptr.value());
            }


            inline pointer _end() {
                return const_cast<pointer>(_cend());
            }

            const_pointer _cbegin() const {
                return static_cast<const_pointer>(reinterpret_cast<const void *>(&_data[0]._value));
            }

            const_pointer _cend() const {
                return static_cast<const_pointer>(reinterpret_cast<const void *>(&_data[numElements]._value));
            }

            inline pointer _last() {
                return is_pointer_to_data() ? _ptr.value() : _end();
            }

            inline const_pointer _last() const {
                return is_pointer_to_data() ? _ptr.value() : _cend();
            }

        private:
            friend chunked_list;

            chunk_value_ptr _ptr;
            value_storage_type _data[numElements];
        };

        class iterator_base : public non_std::iterator<iterator_category, value_type, difference_type, pointer, reference> {
        public:
            constexpr iterator_base() : _chunk(nullptr), _next(nullptr) {}

            iterator_base(chunk_pointer chunk, pointer next) : _chunk(chunk), _next(next) {
            }

//            iterator_base &operator++() {
//#if XTRA_CONTAINERS_CHUNKED_LIST_ASSERT
//                assert(_chunk);
//                assert(_chunk->is_pointer_to_data(_next));
//#endif
//#if XTRA_CONTAINERS_CHUNKED_LIST_ENABLE_ERASE
//                if (deleted_chunk_type::value) {
//                    while(true) {
//                        __next();
//                        size_type n = _next - _chunk->_cbegin();
//                        Serial.printf("TEST %d next=%p exists=%u\n",n,_next, _next?_chunk->_exists(n):-1);
//                        if (_next == nullptr || _chunk->_exists(n)) {
//                            break;
//                        }
//                    }
//                }
//                else
//#endif
//                {
//                    __next();
//                }
//                return *this;
//            }

            iterator_base &operator++() {
                __next();
                return *this;
            }

            iterator_base operator++(const difference_type) {
                auto tmp = *this;
                __next();
                return tmp;
            }

            iterator_base &operator+=(const difference_type step) {
                _advance(step);
                return *this;
            }

            iterator_base operator+(const difference_type step) const {
                auto tmp = *this;
                tmp._advance(step);
                return tmp;
            }

            bool operator !=(const iterator_base &iterator) const {
                return _chunk != iterator._chunk || _next != iterator._next;
            }

            bool operator ==(const iterator_base &iterator) const {
                return _chunk == iterator._chunk && _next == iterator._next;
            }

            const_pointer operator->() const {
                return _get();
            }

            const_reference operator*() const {
                return *_get();
            }

            const_pointer get() const {
                return _get();
            }

        protected:
            friend chunked_list;


            void _advance(difference_type step) {
#if XTRA_CONTAINERS_CHUNKED_LIST_ASSERT
                assert(step >= 0);
#endif
                if (!_next || !_chunk || step <= 0) {
                    return;
                }
                while (step > 0) {
                    difference_type left = _chunk->_last() - _next;
                    if (step < left) {
                        _next += step;
                        return;
                    }
                    if (!(_chunk = _chunk->next_chunk())) {
                        _next = nullptr;
                        return;
                }
                    _next = _chunk->begin();
                    step -= left;
            }
        }

            void __next() {
                if (!_next || !_chunk) {
                    return;
                }
                // check if the last element has been reached
                if (++_next >= _chunk->_last()) {
                    // proceed with next chunk
                    if ((_chunk = _chunk->next_chunk())) {
                        _next = _chunk->begin();
                        return;
                    }
                    _next = nullptr;
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
                assert(_chunk->is_pointer_to_data(_next));
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

            constexpr iterator() : iterator_base() {}

            iterator(const const_iterator &) = delete;
            iterator(const_iterator &&) = delete;

            iterator &operator=(const const_iterator &) = delete;
            iterator &operator=(const_iterator &&) = delete;

            pointer operator->() {
                return _get();
            }

            reference operator*() {
                return *_get();
            }
        };

        static constexpr size_type chunk_size = sizeof(chunk);
        static constexpr size_type chunk_element_count = numElements;

        chunked_list() :
            _firstChunk(nullptr),
            _lastChunk(nullptr)
        {
        }
        ~chunked_list() {
            _destroy();
        }

        chunked_list(chunked_list &&list) noexcept :
            _firstChunk(std::exchange(list._firstChunk, nullptr)),
            _lastChunk(std::exchange(list._lastChunk, nullptr))
        {
        }

        chunked_list(const chunked_list &list) :
            _firstChunk(nullptr),
            _lastChunk(nullptr)

        {
            std::copy(list.cbegin(), list.cend(), std::back_inserter(*this));
        }

        chunked_list &operator=(const chunked_list &list) {
            _destroy();
            _firstChunk = nullptr;
            _lastChunk = nullptr;
            std::copy(list.cbegin(), list.cend(), std::back_inserter(*this));
            return *this;
        }

        chunked_list &operator=(chunked_list &&list) noexcept {
            _destroy();
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
                chunk = chunk->next_chunk();
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
                chunk = chunk->next_chunk();
            }
            return count;
        }

        inline reference front() {
#if XTRA_CONTAINERS_CHUNKED_LIST_ASSERT
            assert(_firstChunk);
#endif
            return *_firstChunk->begin();
        }

        inline reference back() {
#if XTRA_CONTAINERS_CHUNKED_LIST_ASSERT
            assert(_lastChunk);
#endif
            return *(_lastChunk->_last() - 1);
        }

        inline void push_back(const value_type &data) {
            emplace_back(data);
        }

        inline void push_back(value_type &&data) {
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
                assert(_lastChunk->_ptr.value() == _lastChunk->_last());
#endif
                if (_lastChunk->_ptr.value() == _lastChunk->_end()) {
                    _lastChunk = _lastChunk->_ptr.chunk(::new chunk());
                }
            }
            // emplace data
            ::new(_lastChunk->_ptr.forward()) value_type(std::forward<Args>(args)...);
        }

        void clear() {
            if (_firstChunk) {
                _destroy();
                _firstChunk = nullptr;
                _lastChunk = nullptr;
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
            auto chunk = _firstChunk;
            while (chunk) {
                auto nextChunk = chunk->next_chunk();
                delete chunk;
                chunk = nextChunk;
            }
        }

        pointer _first() const {
            return _firstChunk ? _firstChunk->begin() : nullptr;
        }
    private:
        chunk_pointer _firstChunk;
        chunk_pointer _lastChunk;
    };

#if defined(ARDUINO)
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
#endif
};

#pragma pop_macro("new")
