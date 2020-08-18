/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "GFXCanvasConfig.h"

#include <debug_helper.h>
#if DEBUG_GFXCANVAS_MEM
#include <debug_helper_enable_mem.h>
#else
#include <debug_helper_disable_mem.h>
#endif

namespace GFXCanvas {

    using ByteBufferBase = std::vector<uint8_t, __LDBG_track_allocator<uint8_t>>;

    class ByteBuffer : public ByteBufferBase {
    public:
        using ByteBufferBase::ByteBufferBase;

        static constexpr size_t kExpandBufferSize = 16; // expand buffer in 16 byte blocks
        static_assert(kExpandBufferSize >= 4, "too small");

        inline void push(uint8_t data) {
            checkCapacity();
            push_back(data);
        }

        // different name to avoid mixing it up with byte/word
        inline void push2(uint8_t data1, uint8_t data2) {
            checkCapacity();
            push_back(data1);
            push_back(data2);
        }

        inline void push(uint8_t rle, ColorType color) {
            checkCapacity();
            push_back(rle);
            push_back(color);
            push_back(color >> 8);
        }

        uint8_t *begin() {
            return data();
        }
        const uint8_t *begin() const {
            return data();
        }
        uint8_t *end() {
            return &data()[length()];
        }
        const uint8_t *end() const {
            return &data()[length()];
        }

        inline size_t length() const {
            return ByteBufferBase::size();
        }


    private:
        // to avoid mixing up length() and size()
        size_t size() const {
            return capacity();
        }

        // allocation block size
        inline void checkCapacity() {
            size_t required = (ByteBufferBase::size() + kExpandBufferSize) & ~(kExpandBufferSize - 1);
            if (required > ByteBufferBase::capacity()) {
                reserve(required);
            }
        }

    public:
        // allocation shrink block size
        void shrink_to_fit() {
            size_t space = ByteBufferBase::capacity() - ByteBufferBase::size();
            if (space > kExpandBufferSize) {
                ByteBufferBase::reserve((ByteBufferBase::size() + 7) & ~7);
            }
        }
    };

}

#include <debug_helper_disable_mem.h>
