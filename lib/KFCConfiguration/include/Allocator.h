/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include "ConfigurationHelper.h"

namespace ConfigurationHelper {

    class Allocator {
    public:
        static constexpr uint8_t kDefaultAlignment = 0;
        static constexpr uint8_t kAllocBlockSize = (1 << 3) - 1;            // match mallocs 8 byte block size

    public:
        Allocator() {}
        ~Allocator() {
            release();
        }

        uint8_t *allocate(size_t size, size_t *realSize, uint8_t alignment = kDefaultAlignment);
        uint8_t *allocate(size_t size, uint8_t alignment = kDefaultAlignment);
        void deallocate(void *ptr);

        void allocate(size_t size, ConfigurationParameter &parameter, uint8_t alignment = kDefaultAlignment);
        void deallocate(ConfigurationParameter &parameter);

        void release();

    private:
        size_t _get_aligned_size(size_t size, uint8_t alignment) const;
    };

}
