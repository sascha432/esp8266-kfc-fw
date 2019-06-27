/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "JsonBaseReader.h"

class JsonCallbackReader;

typedef std::function<bool(const String &key, const String &value, size_t partialLength, JsonBaseReader &json)> JsonReaderCallback;

class JsonCallbackReader : public JsonBaseReader {
public:
	JsonCallbackReader(Stream &stream, JsonReaderCallback callback, uint16_t maxBufferSize = -1);
	virtual bool processElement() override;

private:
	JsonReaderCallback _callback;
	uint16_t _maxBufferSize;
};

