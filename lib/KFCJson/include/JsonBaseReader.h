/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <functional>
#include <map>
#include <vector>

struct JsonError {
	String message;
	size_t position;
};

#if _WIN32 || _WIN64

class JsonException : public std::exception {
public:
	JsonException(const char *message) : exception(message) {
	}
};

#endif

struct JsonNestedState {
	String key;
	int16_t arrayState;
	uint16_t count;
};

typedef std::vector<JsonNestedState> JsonVector;

class JsonBaseReader {
public:
	enum JsonType_t { JSON_TYPE_ANY = -1, JSON_TYPE_INVALID = 0, JSON_TYPE_STRING, JSON_TYPE_BOOLEAN, JSON_TYPE_INT, JSON_TYPE_FLOAT, JSON_TYPE_NULL, JSON_TYPE_EMPTY_ARRAY, JSON_TYPE_EMPTY_OBJECT };

	JsonBaseReader(Stream &stream) : _stream(stream) {
		_quoteChar = '"';
		clearLastError();
	}
	virtual ~JsonBaseReader() {
	}

	void setStream(Stream &stream) {
		_stream = stream;
	}

	size_t getPosition() const {
		return _position;
	}

	void setQuoteChar(char quoteChar) {
		_quoteChar = quoteChar;
	}

	void initParser();
	bool parseStream();
	bool parse();

	virtual int readByte();
	virtual size_t position() const;

	String getKey() const;
	String getValue() const;

	String getPath() const;
	int16_t getIndex() const;
	int8_t getLevel() const;
	JsonType_t getType() const;
	bool isArrayElement() const;

	String formatValue(String value, JsonType_t type);
	String jsonType2String(JsonType_t type);

	void error(String message);
	void clearLastError();
	JsonError getLastError() const;
	String getLastErrorMessage() const;

	virtual bool beginObject(bool isArray) { return true; }
	virtual bool endObject() { return true; }
	virtual bool processElement() = 0;

protected:
	bool _addCharacter(char ch);
	bool _prepareElement();
	void _appendIndex(int16_t index, String &str) const;
	bool _isValidNumber(String value, JsonType_t &_type) const;

protected:
	Stream &_stream;
	size_t _position;
	char _quoteChar;
	uint8_t _level;
	bool _quoted;
	bool _key;
	bool _escaped;
	uint16_t _count;
	int16_t _array;
	JsonType_t _type;
	String _keyStr;
	String _valueStr;
	JsonError _lastError;
	JsonVector _state;
#if DEBUG
	String _jsonSource;
#endif
};

