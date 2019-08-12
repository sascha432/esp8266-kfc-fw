/**
* Author: sascha_lammers@gmx.de
*/

#define HAVE_REGEX 0

#if HAVE_REGEX
#include <regex>
#endif
#include <memory>
#include "JsonBaseReader.h"

#if DEBUG_JSON_READER
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

void JsonBaseReader::initParser() {
	_level = 0;
	_quoted = false;
	_escaped = false;
	_array = -1;
	_type = JSON_TYPE_INVALID;
	_position = 0;
	_count = 0;
	_keyStr = String();
	_valueStr = String();
}

String JsonBaseReader::formatValue(const String &value, JsonType_t type) {
	String tmp;
	switch (type) {
	case JSON_TYPE_EMPTY_ARRAY:
		return F("[]");
	case JSON_TYPE_EMPTY_OBJECT:
		return F("{}");
	case JSON_TYPE_NULL:
		return F("null");
	case JSON_TYPE_BOOLEAN:
		tmp = value;
		tmp.toLowerCase();
		return tmp;
	case JSON_TYPE_INT:
		return String(value.toInt());
	case JSON_TYPE_FLOAT:
		return String(value.toFloat());
	case JSON_TYPE_STRING:
		tmp = value;
		tmp.replace(F("\""), F("\\\""));
		tmp.replace(F("\\"), F("\\\\"));
		return '"' + tmp + '"';
	case JSON_TYPE_INVALID:
	case JSON_TYPE_ANY:
		break;
	}
	return F("<invalid type>");
}

void JsonBaseReader::error(const String &message) {
	_debug_printf_P(PSTR("JSON error: %s at %d\n"), message.c_str(), position());
	_lastError.message = message;
	_lastError.position = position();
	//#if _WIN32 || _WIN64
	//    throw JsonException(getLastErrorMessage().c_str());
	//#endif
}

void JsonBaseReader::clearLastError() {
	_lastError.position = (size_t)(~0);
	_lastError.message = String();
}

JsonError JsonBaseReader::getLastError() const {
	return _lastError;
}

String JsonBaseReader::getLastErrorMessage() const {
	if (_lastError.position == (size_t)(~0)) {
		return String();
	}
	String error = F("JSON parser error at ");
	error += String(_lastError.position);
	error += F(": ");
	error += _lastError.message;
#if DEBUG_JSON_READER
	error += '\n';
	error += _jsonSource;
	int count = 48;
	while (count-- && _stream.available()) {
		char ch = _stream.read();
		if (count < 16 && (isspace(ch) || ispunct(ch))) {
			break;
		}
		error += (char)ch;
	}
	error += '\n';
	int diff = _jsonSource.length();
	while (--diff > 0) {
		error += ' ';
	}
	error += '^';
#endif
	return error;
}

String JsonBaseReader::jsonType2String(JsonType_t type) {
	switch (type) {
	case JSON_TYPE_BOOLEAN:
		return F("boolean");
	case JSON_TYPE_EMPTY_ARRAY:
		return F("array");
	case JSON_TYPE_EMPTY_OBJECT:
		return F("object");
	case JSON_TYPE_FLOAT:
		return F("float");
	case JSON_TYPE_INT:
		return F("int");
	case JSON_TYPE_STRING:
		return F("string");
	case JSON_TYPE_NULL:
		return F("null");
	case JSON_TYPE_INVALID:
	case JSON_TYPE_ANY:
		break;
	}
	return F("INVALID TYPE");
}

boolean JsonBaseReader::_isValidNumber(const String &value, JsonType_t &_type) const {
#if HAVE_REGEX
	std::smatch match;
	if (std::regex_match(value, match, std::regex("^-?(?=[1-9]|0(?!\\d))\\d+(\\.\\d+)?([eE][+-]?\\d+)?$"))) {
		if (match.str(1).length() || match.str(2).length()) {  // group 1=fraction, 2=exponent
			_type = JSON_TYPE_FLOAT;
		} else {
			_type = JSON_TYPE_INT;
		}
		return true;
	}
	return false;
#else
	JsonType_t newType = JSON_TYPE_INT;
	const char *ptr = value.c_str();
	// -?
	if (*ptr == '-') {
		ptr++;
	}
	// (?=[1-9] | 0(?!\d))
	if (!((*ptr != '0' && isdigit(*ptr)) || (*ptr == '0' && !isdigit(*(ptr + 1))))) {
		return false;
	}
	ptr++;
	// \d+
	while (isdigit(*ptr)) {
		ptr++;
	}
	// (
	// \\.
	if (*ptr == '.') {
		newType = JSON_TYPE_FLOAT;  // with a fraction type is float
		ptr++;
		// \d+
		if (!isdigit(*ptr++)) {
			return false;
		}
		while (isdigit(*ptr)) {
			ptr++;
		}
	}
	// )?

	// (
	// [eE]
	if (tolower(*ptr) == 'e') {
		newType = JSON_TYPE_FLOAT;  // an exponent indicates float
		ptr++;
		// [+-]?
		if (*ptr == '+' && *ptr == '-') {
			ptr++;
		}
		// \d+
		if (!isdigit(*ptr++)) {
			return false;
		}
		while (isdigit(*ptr)) {
			ptr++;
		}
	}
	// )?
	if (*ptr) {
		return false;
	}
	_type = newType;  // number validated, set type
	return true;
#endif
}

boolean JsonBaseReader::_prepareElement() {
	if (_keyStr.length() == 0 && _valueStr.length() == 0) {
		_type = JSON_TYPE_INVALID;
		_debug_printf_P(PSTR("key and data length 0\n"));
		return true;
	}
	if (_array == -1 && !_keyStr.length()) {
		error(F("An object requires a key"));
		return false;
	}
	if (_type == JSON_TYPE_INVALID) {
		//size_t fake_pos = max(0, (int)(position - _valueStr.length() - 1));
		_valueStr.trim();
		if (_valueStr.equalsIgnoreCase(F("true")) || _valueStr.equalsIgnoreCase(F("false"))) {
			_type = JSON_TYPE_BOOLEAN;
		} else if (_valueStr.equalsIgnoreCase(F("null"))) {
			_type = JSON_TYPE_NULL;
		} else if (_isValidNumber(_valueStr, _type)) {
			//_type = JSON_TYPE_NUMBER;  // _isValidNumber sets _type to int or float
		} else {
			//_position = fake_pos++;
			error(F("Invalid value"));
			return false;
		}
	}

	_count++;
	_debug_printf_P(PSTR("processing key '%s' data %s type %d level %d at %d\n"), _keyStr.c_str(), formatValue(_valueStr, getType()).c_str(), (int)getType(), (int)getLevel(), (int)getPosition());

	bool result = processElement();
	_keyStr = String();
	_valueStr = String();
	_type = JSON_TYPE_INVALID;
	return result;
}

boolean JsonBaseReader::parseStream() {
	_debug_printf_P(PSTR("JSONparseStream available %d\n"), _stream.available());

	int ch;
	while ((ch = readByte()) != -1) {
		if (ch == 0) {
			error("NUL byte found");
			return false;
		}
		if (_escaped) {
			_escaped = false;
			if (!_addCharacter(ch)) {
				return false;
			}
		} else if (!_quoted && (ch == '{' || ch == '[')) {
			_debug_printf_P(PSTR("open %s level %d key %s array %d count %d\n"), (ch == '[' ? "array" : "object"), _level + 1, _keyStr.c_str(), _array, _count);
			_state.push_back({_keyStr, _array, _count});
			if (++_level == 255) {
				error(F("Maximum nested level reached"));
				return false;
			}
			if (ch == '[') {
				if (!beginObject(true)) {
					return false;
				}
				_array = 0;
			} else {
				if (!beginObject(false)) {
					return false;
				}
				_array = -1;
			}
			_keyStr = String();
			_valueStr = String();
			_count = 0;
		} else if (!_quoted && (ch == '}' || ch == ']')) {
			_debug_printf_P(PSTR("closing %s level %d key %s data %s array %d count %d\n"), (ch == ']' ? "array" : "object"), _level, _keyStr.c_str(), _valueStr.c_str(), _array, _count);
			if (_count == 0 && _keyStr.length() == 0 && _valueStr.length() == 0) {
				_type = ch == ']' ? JSON_TYPE_EMPTY_ARRAY : JSON_TYPE_EMPTY_OBJECT;
				if (!processElement()) {
					return false;
				}
				_type = JSON_TYPE_INVALID;
			} else if (!_prepareElement()) {
				return false;
			}
			if (!endObject()) {
				return false;
			}
			if (--_level == 255) {
				error(F("Out of bounds"));
				return false;
			}
			_array = _state.back().arrayState;
			_count = _state.back().count;
			_state.pop_back();
		} else if (ch == _quoteChar) {
			_quoted = !_quoted;
			_type = JSON_TYPE_STRING;

		} else if (!_quoted && ch == ':') {
			if (_array != -1) {
				error(F("keys are not allowed inside arrays"));
				return false;
			}
			_debug_printf_P(PSTR("got key '%s' at %d\n"), _valueStr.c_str(), getPosition());

			_keyStr = _valueStr;
			_valueStr = String();
			_type = JSON_TYPE_INVALID;

		} else if (!_quoted && ch == ',') {
			if (!_prepareElement()) {
				return false;
			}
			if (_array != -1) {
				_array++;
			}
		} else if (ch == '\\') {
			_escaped = true;
		} else if (!_addCharacter(ch)) {
			return false;
		}
	}
	_debug_printf_P(PSTR("JSON parser end\n"));
	return true;
}

int JsonBaseReader::readByte() {
	if (!_stream.available()) {
		return -1;
	}
	int ch = _stream.read();
	if (ch != -1) {
		_position++;
#if DEBUG_JSON_READER
		_jsonSource += ch;
		if (_jsonSource.length() > 16) {
			_jsonSource.remove(0, _jsonSource.length() - 16);
		}
#endif
	}
	return ch;
}

size_t JsonBaseReader::position() const {
	return _position;
}

String JsonBaseReader::getPath(uint8_t index) const {
	if (index < _state.size()) {
		return _state.at(index).key;
	}
	return String();
}

String JsonBaseReader::getPath() const {
	String _path;
	for (const auto &state : _state) {
		if (state.key.length()) {
			if (_path.length()) {
				_path += '.';
			}
			_path += state.key;
			if (state.arrayState != -1) {
				_appendIndex(state.arrayState, _path);
			}
		} else if (state.arrayState != -1) {
			_appendIndex(state.arrayState, _path);
		}
	}
	if (_keyStr.length() && _path.length()) {
		_path += '.';
	}
	_path += _keyStr;
	if (_array != -1) {
		_appendIndex(_array, _path);
	}

	return _path;
}

boolean JsonBaseReader::parse() {
	initParser();
	return parseStream();
}

void JsonBaseReader::_appendIndex(int16_t index, String &str) const {
	str += '[';
	str += String(index);
	str += ']';
}
