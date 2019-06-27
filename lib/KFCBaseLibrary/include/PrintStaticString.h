/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include "Buffer.h"

class PrintStaticString : public Print {
public:
	PrintStaticString(char *buffer, size_t size, const char *str);
	PrintStaticString(char *buffer, size_t size, const __FlashStringHelper *str);
	PrintStaticString(char *buffer, size_t size);

	String toString() const;

	operator String() const;

	const char *getBuffer();
	size_t size() const;
	size_t length() const;

	virtual size_t write(uint8_t data) override;
	virtual size_t write(const uint8_t *buffer, size_t size) override;
	    
private:
	char *_buffer;
	size_t _size;
	size_t _position;
};
