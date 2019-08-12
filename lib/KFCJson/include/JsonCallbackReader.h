/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "JsonBaseReader.h"

#define DEBUG_JSON_KEY_OR_INDEX(key, json) \
	(json.isArrayElement() ? String('[' + String(json.getIndex()) + ']') : String('\'' + key + '\'')).c_str()

#define DEBUG_JSON_CALLBACK_P(printf_P_func, key, value, json) \
	printf_P_func(PSTR("key=%s value='%s' level=%u path='%s' type=%s\n"), DEBUG_JSON_KEY_OR_INDEX(key, json), value.c_str(), json.getLevel(), json.getPath().c_str(), json.jsonType2String(json.getType()).c_str());

#define DEBUG_JSON_CALLBACK(printf_func, key, value, json) \
	printf_func("key=%s value='%s' level=%u path='%s' type=%s\n", DEBUG_JSON_KEY_OR_INDEX(key, json), value.c_str(), json.getLevel(), json.getPath().c_str(), json.jsonType2String(json.getType()).c_str());

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

