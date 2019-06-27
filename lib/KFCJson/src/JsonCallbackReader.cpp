/**
* Author: sascha_lammers@gmx.de
*/

#include "JsonCallbackReader.h"


JsonCallbackReader::JsonCallbackReader(Stream &stream, JsonReaderCallback callback, uint16_t maxBufferSize) : JsonBaseReader(stream) {
	_callback = callback;
	_maxBufferSize = maxBufferSize;
}

bool JsonCallbackReader::processElement() {
	bool result = _callback(_keyStr, _valueStr, 0, *this);
	if (!result) {
		error(F("Callback returned false"));
	}
	return result;
}
