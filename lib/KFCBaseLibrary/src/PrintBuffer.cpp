/**
 * Author: sascha_lammers@gmx.de
 */

#include "Buffer.h"
#include "PrintBuffer.h"

PrintBuffer::PrintBuffer(size_t size) : Buffer(size), Print() {
}

inline size_t PrintBuffer::write(uint8_t data) {
	return Buffer::write(data);
}

inline size_t PrintBuffer::write(const uint8_t * buffer, size_t size) {
	return Buffer::write((uint8_t *)buffer, size);
}
