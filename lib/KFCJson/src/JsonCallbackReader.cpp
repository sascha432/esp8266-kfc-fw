/**
* Author: sascha_lammers@gmx.de
*/

#include "JsonCallbackReader.h"

#if DEBUG_KFC_JSON
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

// Parse JSON and send objects to callback function

JsonCallbackReader::JsonCallbackReader(Stream *stream, JsonReaderCallback callback, uint16_t maxBufferSize) : JsonBaseReader(stream) {
	_callback = callback;
	_maxBufferSize = maxBufferSize;
}

bool JsonCallbackReader::processElement() {
	bool result = _callback(_keyStr, _valueStr, 0, *this);
	if (!result) {
		error(F("Callback returned false"), JSON_ERROR_USER_ABORT);
	}
	return result;
}
