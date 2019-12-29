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

#if _WIN32 || _WIN64

class JsonException : public std::exception {
public:
	JsonException(const char *message) : exception(message) {
	}
};

#endif


class JsonBaseReader {
public:
    typedef struct  {
        String key;
        size_t keyPosition;
        int16_t arrayIndex;
        uint16_t count;
    } JsonStack_t;

    typedef enum : uint8_t {
        JSON_ERROR_NONE = 0,
        JSON_ERROR_OBJECT_VALUE_WITHOUT_KEY,
        JSON_ERROR_ARRAY_WITH_KEY,
        JSON_ERROR_INVALID_VALUE,
        JSON_ERROR_EMPTY_VALUE,
        JSON_ERROR_NUL_TERMINATOR,                  // NUL terminator found while reading stream
        JSON_ERROR_MAX_NESTED_LEVEL,
        JSON_ERROR_OUT_OF_BOUNDS,                   // array or object terminator without array or object
        JSON_ERROR_USER_ABORT,
        JSON_ERROR_INVALID_END,                     // array closed with } or object closed with ]
    } JsonErrorEnum_t ;

    typedef struct {
        String message;
        size_t position;
        JsonErrorEnum_t type;
    } JsonError_t;

    typedef std::vector<JsonStack_t> JsonStackVector;

    typedef enum : int8_t {
		JSON_TYPE_ANY = -1,
		JSON_TYPE_INVALID = 0,
		JSON_TYPE_STRING,
		JSON_TYPE_BOOLEAN,
		JSON_TYPE_INT,
        JSON_TYPE_FLOAT,
        JSON_TYPE_NUMBER,                   // number in E notation, might need special parsing function see class JsonVar
        JSON_TYPE_NULL,
        JSON_TYPE_OBJECT_END,
	} JsonType_t;

    JsonBaseReader(Stream &stream) : JsonBaseReader(&stream) {
    }

	JsonBaseReader(Stream *stream) : _stream(stream) {
		_quoteChar = '"';
		clearLastError();
	}

	virtual ~JsonBaseReader() {
	}

	void setStream(Stream *stream) {
		_stream = stream;
	}

	inline size_t getLength() const {
		return _position;
	}

    // position is undefined if the value is an empty string
    inline size_t getValuePosition() const {
        return _valuePosition;
    }

    // position is undefined if the key is an empty string
    inline size_t getKeyPosition() const {
        return _keyPosition;
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
		return _arrayIndex;
	}

    // get index of the parent object
    int16_t getObjectIndex() {
        return _stack.size() ? _stack.back().arrayIndex : -1;
    }

	// returns true if it is an array
	inline bool isArrayElement() const {
		return _arrayIndex != -1;
	}

	// return value
	inline String getValue() const {
		return _valueStr;
	}

    inline long getIntValue() const {
        return _valueStr.toInt();
    }

    inline float getFloatValue() const {
        return _valueStr.toFloat();
    }

    // return type of value
	inline JsonType_t getType() const {
		return _type;
	}

	// index is 1 based, 0 points to an unnamed array or object
    String getPath(int index) const;

    // get path and key position. key position is undefined if path.length() == 0
    String getPath(uint8_t index, size_t &keyPosition) const;

	// get full path
    String getPath(bool numericIndex = true) const;

    // get path of the parent object
    String getObjectPath(bool numericIndex = true) const;

	// level is 1 based
	inline int8_t getLevel() const {
		return _level;
	}

	// return type of JSON value as String
	String jsonType2String(JsonType_t type);

	void error(const String &message, JsonErrorEnum_t type);
	void clearLastError();
	JsonError_t getLastError() const;
	String getLastErrorMessage() const;

    virtual bool beginObject(bool isArray);
    virtual bool endObject();
	virtual bool processElement() = 0;
    virtual bool recoverableError(JsonErrorEnum_t errorType);

protected:
    bool _addCharacter(char ch);
	bool _prepareElement();
	void _appendIndex(int16_t index, String &str, bool numericIndex = false) const;
    bool _isValidNumber(const String &value, JsonType_t &_type);

protected:
	Stream *_stream;
	size_t _position;
    size_t _valuePosition;
    size_t _keyPosition;

	uint8_t _level;			// byte 1 (alignment)
	uint8_t _quoted : 1;	// byte 2
	uint8_t _key : 1;
	uint8_t _escaped : 1;
    char _quoteChar;		// byte 3
	JsonType_t _type;		// byte 4

    int16_t _arrayIndex;

	uint16_t _count;

	String _keyStr;
	String _valueStr;
	JsonError_t _lastError;
	JsonStackVector _stack;
#if DEBUG_JSON_READER
	String _jsonSource;
#endif
};

