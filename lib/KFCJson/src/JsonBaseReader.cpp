/**
* Author: sascha_lammers@gmx.de
*/

#include <memory>
#include "JsonBaseReader.h"
#include "JsonVar.h"
#include "JsonTools.h"

#if DEBUG_KFC_JSON
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

void JsonBaseReader::initParser() {
	_level = 0;
	_quoted = false;
	_escaped = false;
    _arrayIndex = -1;
	_type = JSON_TYPE_INVALID;
	_position = 0;
	_count = 0;
	_keyStr = String();
	_valueStr = String();
}

void JsonBaseReader::error(const String &message, JsonErrorEnum_t type)
{
	_debug_printf_P(PSTR("JSON error: %s at %d\n"), message.c_str(), position());
	_lastError.message = message;
	_lastError.position = position();
    _lastError.type = type;
	//#if _WIN32 || _WIN64
	//    throw JsonException(getLastErrorMessage().c_str());
	//#endif
}

void JsonBaseReader::clearLastError()
{
	_lastError.position = (size_t)(~0);
	_lastError.message = String();
    _lastError.type = JSON_ERROR_NONE;
}

JsonBaseReader::JsonError_t JsonBaseReader::getLastError() const
{
	return _lastError;
}

String JsonBaseReader::getLastErrorMessage() const
{
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
	while (count-- && _stream->available()) {
		char ch = _stream->read();
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

bool JsonBaseReader::beginObject(bool isArray)
{
    return true;
}

bool JsonBaseReader::endObject()
{
    return true;
}

bool JsonBaseReader::recoverableError(JsonErrorEnum_t errorType)
{
    return true;
}

String JsonBaseReader::jsonType2String(JsonType_t type)
{
	switch (type) {
	case JSON_TYPE_BOOLEAN:
		return F("boolean");
	case JSON_TYPE_FLOAT:
		return F("float");
	case JSON_TYPE_INT:
		return F("int");
	case JSON_TYPE_STRING:
		return F("string");
	case JSON_TYPE_NULL:
		return F("null");
	default:
		break;
	}
	return F("INVALID TYPE");
}

bool JsonBaseReader::_isValidNumber(const String &value, JsonType_t &_type)
{
    auto type = JsonVar::getNumberType(value.c_str());
    if (type & JsonVar::NumberType_t::EXPONENT) {
        _type = JSON_TYPE_NUMBER;
        return true;
    }
    type &= JsonVar::NumberType_t::TYPE_MASK;
    if (type == JsonVar::NumberType_t::INVALID) {
        return false;
    }
    else if (type == JsonVar::NumberType_t::FLOAT) {
        _type = JSON_TYPE_FLOAT;
    }
    else if (type == JsonVar::NumberType_t::INT) {
        _type = JSON_TYPE_INT;
    }
    return true;
}

bool JsonBaseReader::_addCharacter(char ch)
{
    if (!_quoted && isspace(ch)) {
        return true;
    }
    if (_valueStr.length() == 0) {
        _valuePosition = _position - 1;
    }
    _valueStr += ch;
    return true;
}

bool JsonBaseReader::_prepareElement()
{
    bool result = true;
    if (_type != JSON_TYPE_OBJECT_END) {
        if (_keyStr.length() == 0 && _valueStr.length() == 0) {
		    _type = JSON_TYPE_INVALID;
            error(F("Empty value not allowed"), JSON_ERROR_EMPTY_VALUE);
		    return recoverableError(JSON_ERROR_EMPTY_VALUE);
	    }
	    if (_arrayIndex == -1 && !_keyStr.length()) {
		    error(F("An object requires a key"), JSON_ERROR_OBJECT_VALUE_WITHOUT_KEY);
            return recoverableError(JSON_ERROR_OBJECT_VALUE_WITHOUT_KEY);
	    }
	    if (_type == JSON_TYPE_INVALID) {
		    //size_t fake_pos = max(0, (int)(position - _valueStr.length() - 1));
		    _valueStr.trim();
            if (strcasecmp_P(_valueStr.c_str(), SPGM(true)) == 0 || strcasecmp_P(_valueStr.c_str(), SPGM(false)) == 0) {
		    //if (_valueStr.equalsIgnoreCase(FSPGM(true)) || _valueStr.equalsIgnoreCase(F("false"))) {
			    _type = JSON_TYPE_BOOLEAN;
		    }
            else if (strcasecmp_P(_valueStr.c_str(), SPGM(null)) == 0) {
            //else if (_valueStr.equalsIgnoreCase(F("null"))) {
                _type = JSON_TYPE_NULL;
		    }
            else if (_isValidNumber(_valueStr, _type)) {
                // _isValidNumber() sets _type
		    }
            else {
			    //_position = fake_pos++;
			    error(F("Invalid value"), JSON_ERROR_INVALID_VALUE);
                if (!recoverableError(JSON_ERROR_INVALID_VALUE)) {
                    return false;
                }
		    }
	    }

	    _count++;
	    _debug_printf_P(PSTR("processing key '%s' data %s type %d level %d at %d\n"), _keyStr.c_str(), JsonVar::formatValue(_valueStr, getType()).c_str(), (int)getType(), (int)getLevel(), (int)getLength());

	    result = processElement();
    }
	_keyStr = String();
	_valueStr = String();
	_type = JSON_TYPE_INVALID;
	return result;
}

bool JsonBaseReader::parseStream()
{
#if DEBUG_KFC_JSON
	if (_stream) {
		_debug_printf_P(PSTR("JSONparseStream available %d\n"), _stream->available());
	}
#endif

	int ch;
	while ((ch = readByte()) != -1) {
		if (ch == 0) {
			error("NUL byte found", JSON_ERROR_NUL_TERMINATOR);
			return false;
		}
		if (_escaped) {
			_escaped = false;
			if (!_addCharacter(ch)) {
				return false;
			}
		}
        else if (!_quoted && (ch == '{' || ch == '[')) {
			_debug_printf_P(PSTR("open %s level %d key %s array %d count %d\n"), (ch == '[' ? "array" : "object"), _level + 1, _keyStr.c_str(), _arrayIndex, _count);
			_stack.push_back({_keyStr, _keyPosition, _arrayIndex, _count});
			if (++_level <= 0) {
				error(F("Maximum nested level reached"), JSON_ERROR_MAX_NESTED_LEVEL);
				return false;
			}
			if (ch == '[') {
                // array
				if (!beginObject(true)) {
					return false;
				}
                _arrayIndex = 0;
			} else {
                // object
				if (!beginObject(false)) {
					return false;
				}
                _arrayIndex = -1;
			}
			_keyStr = String();
			_valueStr = String();
			_count = 0;
		}
        else if (!_quoted && (ch == '}' || ch == ']')) {
			_debug_printf_P(PSTR("closing %s level %d key %s data %s array %d count %d\n"), (ch == ']' ? "array" : "object"), _level, _keyStr.c_str(), _valueStr.c_str(), _arrayIndex, _count);
            if ((_arrayIndex == -1 && ch == ']') || (_arrayIndex != -1 && ch == '}')) {
                error(F("Invalid array or object end"), JSON_ERROR_INVALID_END);
                return false;
            }
            else
			if (_count == 0 && _keyStr.length() == 0 && _valueStr.length() == 0) {
			    _type = JSON_TYPE_OBJECT_END;
			}
            else if (!_prepareElement()) {
                return false;
            }
			if (!endObject()) {
				return false;
			}
			if (_level-- == 0) {
				error(F("Out of bounds"), JSON_ERROR_OUT_OF_BOUNDS);
				return false;
			}
            _arrayIndex = _stack.back().arrayIndex;
			_count = _stack.back().count;
			_stack.pop_back();
            _type = JSON_TYPE_OBJECT_END;
		}
        else if (ch == _quoteChar) {
			_quoted = !_quoted;
			_type = JSON_TYPE_STRING;
		}
        else if (!_quoted && ch == ':') {
			if (_arrayIndex != -1) {
				error(F("Key not allowed inside array"), JSON_ERROR_ARRAY_WITH_KEY);
                if (!recoverableError(JSON_ERROR_ARRAY_WITH_KEY)) {
                    return false;
                }
                //_valueStr = String();
			}
			_debug_printf_P(PSTR("got key '%s' at %d\n"), _valueStr.c_str(), getLength());

			_keyStr = _valueStr;
            _keyPosition = _valuePosition;
			_valueStr = String();
			_type = JSON_TYPE_INVALID;

		}
        else if (!_quoted && ch == ',') {
			if (!_prepareElement()) {
				return false;
			}
			if (_arrayIndex != -1) {
                _arrayIndex++;
			}
		}
        else if (ch == '\\') {
			_escaped = true;
		}
        else if (!_addCharacter(ch)) {
			return false;
		}
	}
	_debug_printf_P(PSTR("JSON parser end\n"));
	return true;
}

int JsonBaseReader::readByte()
{
#if DEBUG
	if (!_stream) {
		debug_println("JsonBaseReader::readByte(): _stream = nullptr");
		return -1;
	}
#endif
	if (!_stream->available()) {
		return -1;
	}
	int ch = _stream->read();
	_position++;
#if DEBUG_JSON_READER
	_jsonSource += ch;
	if (_jsonSource.length() > 16) {
		_jsonSource.remove(0, _jsonSource.length() - 16);
	}
#endif
	return ch;
}

size_t JsonBaseReader::position() const
{
	return _position;
}

String JsonBaseReader::getPath(int index) const
{
    if ((size_t)index < _stack.size()) {
        return _stack.at(index).key;
    }
    return String();
}

String JsonBaseReader::getPath(uint8_t index, size_t &keyPosition) const
{
    if (index < _stack.size()) {
        auto &state = _stack.at(index);
        keyPosition = state.keyPosition;
        return state.key;
    }
    return String();
}

String JsonBaseReader::getPath(bool numericIndex, int fromIndex) const
{
    String _path = getObjectPath(numericIndex, fromIndex);
    if (_keyStr.length() && _path.length()) {
        _path += '.';
    }
    _path += _keyStr;
    if (_arrayIndex != -1) {
        _appendIndex(_arrayIndex, _path, numericIndex);
    }
    return _path;
}

String JsonBaseReader::getObjectPath(bool numericIndex, int fromIndex) const
{
    String _path;
	int n = 0;
    for (const auto &state : _stack) {
		if (fromIndex > n++) {
			continue;
		}
        if (state.key.length()) {
            if (_path.length()) {
                _path += '.';
            }
            _path += state.key;
            if (state.arrayIndex != -1) {
                _appendIndex(state.arrayIndex, _path, numericIndex);
            }
        } else if (state.arrayIndex != -1) {
            _appendIndex(state.arrayIndex, _path, numericIndex);
        }
    }
    return _path;
}

bool JsonBaseReader::parse()
{
	initParser();
	return parseStream();
}

void JsonBaseReader::_appendIndex(int16_t index, String &str, bool numericIndex) const
{
	str += '[';
    if (numericIndex) {
        str += String(index);
    }
	str += ']';
}
