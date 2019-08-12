/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <functional>
#include <map>
#include <vector>

#ifndef DEBUG_JSON_READER
#define DEBUG_JSON_READER 	0
#endif

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
	enum JsonType_t : int8_t { 
		JSON_TYPE_ANY = -1, 
		JSON_TYPE_INVALID = 0, 
		JSON_TYPE_STRING, 
		JSON_TYPE_BOOLEAN, 
		JSON_TYPE_INT, 
		JSON_TYPE_FLOAT, 
		JSON_TYPE_NULL, 
		JSON_TYPE_EMPTY_ARRAY, 
		JSON_TYPE_EMPTY_OBJECT 
	};

	JsonBaseReader(Stream &stream) : _stream(stream) {
		_quoteChar = '"';
		clearLastError();
	}
	virtual ~JsonBaseReader() {
	}

	inline void setStream(Stream &stream) {
		_stream = stream;
	}

	inline size_t getPosition() const {
		return _position;
	}

	inline void setQuoteChar(char quoteChar) {
		_quoteChar = quoteChar;
	}

	void initParser();
	bool parseStream();

	// initParser() and parseStream() combined
	bool parse();

	virtual int readByte();
	virtual size_t position() const;

	// get key. key is empty if it is an array
	inline String getKey() const {
		return _keyStr;
	}
	
	// returns -1 if it isn't an array, otherwise the index
	inline int16_t getIndex() const {
		return _array;
	}

	// returns true if it is an array
	inline bool isArrayElement() const {
		return _array != -1;
	}

	// return value
	inline String getValue() const {
		return _valueStr;
	}

	// return type of value
	inline JsonType_t getType() const {
		return _type;
	}

	// index is 1 based, 0 points to an unnamed array or object
	String getPath(uint8_t index) const;

	// get full path
	String getPath() const;

	// level is 1 based
	inline int8_t getLevel() const {
		return _level;
	}

	String formatValue(const String &value, JsonType_t type);

	// return type of json value as String
	String jsonType2String(JsonType_t type);

	void error(const String &message);
	void clearLastError();
	JsonError getLastError() const;
	String getLastErrorMessage() const;

	virtual bool beginObject(bool isArray) { return true; }
	virtual bool endObject() { return true; }
	virtual bool processElement() = 0;

protected:
	inline bool _addCharacter(char ch) {
		_valueStr += ch;
		return true;
	}

	bool _prepareElement();
	void _appendIndex(int16_t index, String &str) const;
	bool _isValidNumber(const String &value, JsonType_t &_type) const;

protected:
	Stream &_stream;
	size_t _position;

	uint8_t _level;			// byte 1 (alignment)
	uint8_t _quoted: 1;		// byte 2
	uint8_t _key: 1;
	uint8_t _escaped: 1;
	char _quoteChar;		// byte 3
	JsonType_t _type;		// byte 4

	uint16_t _count;
	int16_t _array;

	String _keyStr;
	String _valueStr;
	JsonError _lastError;
	JsonVector _state;
#if DEBUG
	String _jsonSource;
#endif
};

