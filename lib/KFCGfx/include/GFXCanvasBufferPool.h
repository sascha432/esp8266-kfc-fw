/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <Buffer.h>
#include <PrintString.h>
#include <list>
#include "GFXCanvasConfig.h"
#include "GFXCanvasBufferPool.h"

#include <debug_helper.h>
#if DEBUG_GFXCANVAS_MEM
#include <debug_helper_enable_mem.h>
#else
#include <debug_helper_disable_mem.h>
#endif

#ifndef GFXCANVAS_BUFFER_POOL_PACK_PTR
#    if _MSC_VER
#        define UMM_MALLOC_CFG_HEAP_ADDR            0x100000
#        define UMM_MALLOC_CFG_HEAP_SIZE            0xffff
#        define GFXCANVAS_BUFFER_POOL_PACK_PTR      0
#    elif defined(ESP8266)
#        include "umm_malloc/umm_malloc_cfg.h"
#        define GFXCANVAS_BUFFER_POOL_PACK_PTR      1
#    else
#        define GFXCANVAS_BUFFER_POOL_PACK_PTR      0
#    endif
#endif

#include <push_pack.h>

namespace GFXCanvas {

    template<typename data_type, typename size_type = uint8_t>
    class BufferTemplate;

    using ByteBuffer = BufferTemplate<ByteBufferDataType, ByteBufferSizeType>;

    class BufferPool {
    public:
        using BufferList = std::list<Buffer>;
        using BufferType = ByteBuffer;
        using BufferTypePtr = ByteBuffer *;
#if GFXCANVAS_BUFFER_POOL_PACK_PTR
        using BufferIdType = uint16_t;
#else
        using BufferIdType = BufferTypePtr;
#endif

        // size per allocated buffer pool
        // 256 - 512 seems to be a good compromise between overhead, unusable space and free space
        static constexpr size_t kBufferPoolSize = 512;

        // recommended size is the 95th rule of the length of compressed canvas lines
        // if not available, use 25-50% of the uncompressed size
        static constexpr size_t kTempBufferInitSize = 48;

    public:
        static BufferPool &getInstance();
        static void destroyInstance();

        Buffer &get();

        void dump(Print &output);

    protected:
        BufferPool() {}

    private:
#if _MSC_VER
    public:
#endif
        typedef struct __attribute__packed__ BufferHeader_t {

        private:
            BufferIdType _id;                       // pointer to BufferType

#if GFXCANVAS_BUFFER_POOL_PACK_PTR
            static constexpr BufferIdType __pack_id(nullptr_t) {
                return 0;
            }
            BufferIdType __pack_id(BufferTypePtr id) const {
                return (int32_t)id - UMM_MALLOC_CFG_HEAP_ADDR;
            }
            BufferTypePtr __unpack_id(BufferIdType id) const {
                return id ? (BufferTypePtr)((uint32_t)id + UMM_MALLOC_CFG_HEAP_ADDR) : nullptr;
            }
#endif

        public:
            ByteBufferSizeType _dataSize;           // size of the data following

        public:
            BufferHeader_t() : _id(nullptr), _dataSize(0) {}
            BufferHeader_t(BufferTypePtr id, size_t dataSize) : _id(id), _dataSize((ByteBufferSizeType)dataSize) {}

            BufferTypePtr getId() const {
                return _id;
            }

            bool operator==(BufferTypePtr id) const {
                return getId() == id;
            }
            bool operator==(const BufferType &id) const {
                return getId() == &id;
            }

            ByteBufferSizeType dataSize() const {
                return _dataSize;
            }
            size_t size() const {
                return _dataSize + sizeof(*this);
            }
            struct BufferHeader_t &header() {
                return *reinterpret_cast<struct BufferHeader_t *>(this);
            }
            uint8_t *data() {
                return reinterpret_cast<uint8_t *>(this) + sizeof(*this);
            }
            const uint8_t *begin() const {
                return reinterpret_cast<const uint8_t *>(this);
            }
            const uint8_t *end() const {
                return reinterpret_cast<const uint8_t *>(this) + size();
            }
            uint8_t *begin() {
                return reinterpret_cast<uint8_t *>(this);
            }
            uint8_t *end() {
                return reinterpret_cast<uint8_t *>(this) + size();
            }
        } BufferHeader_t;

        class BufferIterator {
        public:
            BufferIterator() : _buffer(nullptr), _iterator(nullptr) {}
            BufferIterator(Buffer &buffer) : _buffer(&buffer), _iterator(buffer.begin()) {}
            BufferIterator(Buffer &buffer, uint8_t *iterator) : _buffer(&buffer), _iterator(iterator) {}
            BufferIterator(Buffer &buffer, BufferHeader_t *header) : _buffer(&buffer), _iterator(reinterpret_cast<uint8_t *>(header)) {}

            //BufferIterator(const BufferIterator &copy) = default; // : _buffer(copy._buffer), _iterator(copy._iterator) {}
            //BufferIterator(BufferIterator &&move) noexcept : _buffer(std::exchange(move._buffer, nullptr)), _iterator(std::exchange(move._iterator, nullptr)) {}

            //BufferIterator &operator=(const BufferIterator &copy) = default;
            //    _buffer = copy.
            //    return this;
            //}

            inline operator bool() const {
                return _buffer && _iterator;
            }

            inline bool operator==(nullptr_t) const {
                return !operator bool();
            }

            inline bool operator!=(nullptr_t) const {
                return operator bool();
            }

            inline bool operator==(const BufferIterator &iterator) const {
                return
                    // valid
                    _buffer &&
                    // same buffer
                    iterator._buffer == _buffer &&
                    // iterator match
                    iterator._iterator == _iterator;
            }

            inline bool operator!=(const BufferIterator &iterator) const {
                return !operator==(iterator);
            }

            BufferIterator operator+(int step) const {
                BufferIterator tmp = *this;
                if (_buffer) {
                    while (tmp._iterator && step-- > 0) {
                        // save to access the iterator contents?
                        if (tmp._iterator < _buffer->end() - sizeof(BufferHeader_t)) {
                            tmp._iterator += tmp.operator*().size();
                            if (tmp._iterator < _buffer->end()) { // not the last item, continue loop
                                continue;
                            }
                        }
                        tmp._iterator = nullptr;
                    }
                }
                return tmp;
            }

            BufferIterator operator-(int step) const {
                BufferIterator tmp = *this;
                if (_buffer) {
                    while (tmp._iterator && step-- > 0) {
                        tmp._iterator -= tmp.operator*().size();
                        if (tmp._iterator < _buffer->begin()) { // first item passed
                            tmp._iterator = nullptr;
                        }
                    }
                }
                return tmp;
            }

            inline BufferIterator &operator++() {
#if 0
                // safe version
                _iterator = (*this + 1)._iterator;
#else
                // only for valid iterators
                // for (auto iterator = BufferIterator(buffer); iterator; ++iterator) { ... }
                if (!(_iterator < _buffer->end() - sizeof(BufferHeader_t) && (_iterator = _iterator + operator*().size()) < _buffer->end())) {
                    _iterator = nullptr;
                }
#endif
                return *this;
            }

            inline BufferIterator &operator--() {
                _iterator = (*this - 1)._iterator;
                return *this;
            }

            inline BufferHeader_t &operator*() {
                return *reinterpret_cast<BufferHeader_t *>(_iterator);
            }

            inline Buffer *operator->() {
                return _buffer;
            }

            inline Buffer &getBuffer() {
                return *_buffer;
            }

        private:
            Buffer *_buffer;
            uint8_t *_iterator;
        };

    private:
        friend ByteBuffer;

        Buffer &getTemp();
        void moveTempTo(ByteBuffer &buffer);
        BufferIterator find(BufferTypePtr id);
        bool remove(BufferTypePtr id);
        BufferIterator add(BufferTypePtr id);

    private:
#if _MSC_VER
    public:
#endif
        static BufferPool *_instance;
        BufferList _list;
        Buffer _temp;
    };

}

#include <pop_pack.h>
