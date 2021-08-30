/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "Buffer.h"

#ifndef _MSC_VER
#pragma GCC push_options
#pragma GCC optimize ("O3")
#endif

class PrintBuffer : public Buffer, public Print {
public:
    PrintBuffer(size_t size = 0);

    size_t write(uint8_t data) override;
    size_t write(const uint8_t *buffer, size_t size) override;
    size_t write(const char *buffer, size_t size);
    size_t write(char *buffer, size_t size);
};

inline PrintBuffer::PrintBuffer(size_t size) : Buffer(size), Print()
{
}

inline size_t PrintBuffer::write(uint8_t data)
{
    return Buffer::write(data);
}

inline size_t PrintBuffer::write(const uint8_t *buffer, size_t size)
{
    return Buffer::write(buffer, size);
}

inline size_t PrintBuffer::write(const char *buffer, size_t size)
{
    return Buffer::write(reinterpret_cast<const uint8_t *>(buffer), size);
}

inline size_t PrintBuffer::write(char *buffer, size_t size)
{
    return Buffer::write(reinterpret_cast<uint8_t *>(buffer), size);
}

#ifndef _MSC_VER
#pragma GCC pop_options
#endif
