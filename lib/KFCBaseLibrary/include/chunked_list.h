/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <stdint.h>

#ifndef XTRA_CONTAINERS_CHUNK_LIST_ASSERT
#define XTRA_CONTAINERS_CHUNK_LIST_ASSERT               1
#endif

#if XTRA_CONTAINERS_CHUNK_LIST_ASSERT
#include <assert.h>
#endif

// frees all objects in reverse order
#ifndef XTRA_CONTAINERS_CHUNK_LIST_DESTROY_RECURSIVE
#define XTRA_CONTAINERS_CHUNK_LIST_DESTROY_RECURSIVE    0
#endif


namespace xtra_containers {


    // chunked_list is a memory efficient append only container
    //
    // Elements in sequence containers are ordered in a strict linear sequence. Individual elements are accessed by their position in this sequence.
    // Each chunk keeps information on how to locate the next chunk, allowing constant time insert at the end, but no direct random access.
    //
    // iterators do not change
    // pointers to data do not change
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

    template<class T, size_t numElements>
    class chunked_list {
    private:
        class chunk;

    public:
        using reference = T &;
        using const_reference = const T &;
        using pointer = T *;
        using const_pointer = const T *;
        using value_type = T;
        using size_type = size_t;
        using difference_type = ptrdiff_t;
        using chunk_pointer = chunk *;
        using iterator_category = std::forward_iterator_tag;

    private:
        class chunk_value_ptr {
        public:
            chunk_value_ptr() = default;
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

        class chunk {
        public:
            chunk() : _ptr(_begin()) {
            }

            chunk(chunk_pointer source, chunk_pointer &last) : _ptr(_begin()) {
                if (source) {
                    // copy all objects
                    for (auto iterator = source->_begin(); iterator != source->_lastElement(); ++iterator) {
                        new(_ptr.forward()) value_type(const_cast<const_reference>(*iterator));
                    }
                    if (source->_hasChildren()) {
                        // create a new chunk and copy all objects from children
                        _ptr.chunk(new chunk(source->_ptr.chunk(), last));
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

            size_type capacity() const {
                return _cend() == _lastElement() ? numElements : _cend() - _lastElement();
            }
        private:
            void _destroy() {
                for (auto data = _lastElement() - 1; data >= _begin(); --data) {
                    data->~value_type();
                }
            }

#if XTRA_CONTAINERS_CHUNK_LIST_DESTROY_RECURSIVE
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
            pointer _lastElement() {
                return _isDataPtr() ? _ptr.value() : _end();
            }
            const_pointer _lastElement() const {
                return _isDataPtr() ? _ptr.value() : _cend();
            }
        private:
            friend chunked_list;

            chunk_value_ptr _ptr;
            uint8_t _data[numElements * sizeof(value_type)];
        };

        class iterator_base {
        public:
            using iterator_category = typename chunked_list::iterator_category;
            using difference_type = typename chunked_list::difference_type;
            using value_type = typename chunked_list::value_type;
            using pointer = typename chunked_list::pointer;
            using reference = typename chunked_list::reference;

            iterator_base() = default;
            iterator_base(chunk_pointer chunk, pointer next) : _chunk(chunk), _next(next) {
            }
            iterator_base &operator++() {
#if XTRA_CONTAINERS_CHUNK_LIST_ASSERT
                assert(_chunk);
                assert(_chunk->_isDataPtr(_next));
#endif
                // check if the last element has been reached
                if (++_next == _chunk->_lastElement()) {
                    // proceed with next chunk
                    _chunk = _chunk->_getNextChunk();
                    _next = _chunk ? _chunk->_begin() : nullptr;
                }
                return *this;
            }
            bool operator !=(const iterator_base &iterator) const {
                return _chunk != iterator._chunk || _next != iterator._next;
            }
            bool operator ==(const iterator_base &iterator) const {
                return _chunk == iterator._chunk && _next == iterator._next;
            }
        protected:
            pointer _get() const {
#if XTRA_CONTAINERS_CHUNK_LIST_ASSERT
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
        class iterator : public iterator_base
        {
        public:
            using iterator_base::iterator_base;
            using difference_type = chunked_list::difference_type;

            pointer operator->() {
                return get();
            }
            reference operator*() {
                return *get();
            }
            pointer get() const {
                return iterator_base::_get();
            }
        };

        class const_iterator : public iterator_base
        {
        public:
            using iterator_base::iterator_base;

            const_pointer operator->() const {
                return get();
            }
            const_reference operator*() const {
                return *get();
            }
            const_pointer get() const {
                return iterator_base::_get();
            }
        };

        chunked_list() : _firstChunk(nullptr), _lastChunk(nullptr), _size(0) {
        }
        ~chunked_list() {
            _destroy();
        }

        chunked_list(chunked_list &&list) {
            *this = std::move(list);
        }

        chunked_list(const chunked_list &list) {
            *this = list;
        }

        chunked_list &operator=(const chunked_list &list) {
            clear();
            if (list._firstChunk) {
                _firstChunk = new chunk(list._firstChunk, _lastChunk);
            }
            _size = list._size;
            return *this;
        }

        chunked_list &operator=(chunked_list &&list) {
            clear();
            _firstChunk = list._firstChunk;
            _lastChunk = list._lastChunk;
            _size = list._size;
            list._firstChunk = nullptr;
            list._lastChunk = nullptr;
            list._size = 0;
            return *this;
        }

        inline void swap(chunked_list &list) {
            std::swap(*this, list);
        }

        size_type size() const {
            return _size;
        }

        bool empty() const {
            return _size != 0;
        }

        size_type capacity() const {
            return _lastChunk ? _lastChunk->capacity() : numElements;
        }

        reference front() {
#if XTRA_CONTAINERS_CHUNK_LIST_ASSERT
            assert(_firstChunk);
#endif
            return *_firstChunk->_begin();
        }

        reference back() {
#if XTRA_CONTAINERS_CHUNK_LIST_ASSERT
            assert(_lastChunk);
#endif
            return *(_lastChunk->_lastElement() - 1);
        }

        void push_back(T &&data) {
            emplace_back(std::move(data));
        }

        template<typename... _Args>
        void emplace_back(_Args &&... __args) {
            if (!_firstChunk) {
                // create first chunk
                _firstChunk = new chunk();
                _lastChunk = _firstChunk;
            }
            else {
                // chunk is full, create a new chunk
#if XTRA_CONTAINERS_CHUNK_LIST_ASSERT
                assert(_lastChunk->_ptr.value() == _lastChunk->_lastElement());
#endif
                if (_lastChunk->_ptr.value() == _lastChunk->_end()) {
                    _lastChunk = _lastChunk->_ptr.chunk(new chunk());
                }
            }
            // emplace data
            new(_lastChunk->_ptr.forward()) value_type(std::forward<_Args>(__args)...);
            _size++;
        }

        void clear() {
            if (_firstChunk) {
                _destroy();
                _firstChunk = nullptr;
                _lastChunk = nullptr;
                _size = 0;
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

    private:
        void _destroy() {
#if XTRA_CONTAINERS_CHUNK_LIST_DESTROY_RECURSIVE
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
        size_type _size;
    };

    template<class T>
    class chunk_list_size {
    public:
        static size_t getSize(uint16_t numElements) {
            return sizeof(void *) + sizeof(T) * numElements;
        }
        static void list(Print &output, uint16_t from, uint16_t to, uint16_t alignment) {
            for (int i = from; i <= to; i++) {
                auto n = chunk_list_size<T>::getSize(i);
                if (!alignment || (alignment && (n % alignment == 0))) {
                    output.printf("elements=%u per_chunk=%u alignment=%u\n", i, n, alignment ? (n % alignment) : n % 16);
                }
            }
        }
    };


};
