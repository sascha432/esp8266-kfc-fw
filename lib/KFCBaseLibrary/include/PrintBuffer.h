/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "Buffer.h"

class PrintBuffer : public Buffer, public Print {
public:
	PrintBuffer(size_t size = 0);

	size_t write(uint8_t data) override;
	size_t write(const uint8_t *buffer, size_t size) override;
	size_t write(const char *buffer, size_t size) {
        return Buffer::write((uint8_t *)buffer, size);
    }
	size_t write(char *buffer, size_t size) {
		return Buffer::write((uint8_t *)buffer, size);
	}
};
