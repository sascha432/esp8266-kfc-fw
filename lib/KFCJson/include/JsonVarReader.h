/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "JsonBaseReader.h"

class JsonReadVar {
public:
	JsonReadVar();
	JsonReadVar(const String &path, JsonBaseReader::JsonType_t expectedType);

	// valid value found?
	operator bool();

	inline bool equals(const String &path) const {
		return _path.equals(path);
	}

	inline const String &getPath() const {
		return _path;
	}

	// invalid class / path not found
	inline bool isEmpty() const {
		return _path.length() == 0;
	}

	inline bool notFoundInResponse() {
		return (_type == JsonBaseReader::JsonType_t::JSON_TYPE_INVALID);
	}

	inline bool isTypeValid() {
		return (_type != JsonBaseReader::JsonType_t::JSON_TYPE_INVALID) && (_expectedType == _type || _expectedType == JsonBaseReader::JsonType_t::JSON_TYPE_ANY);
	}

	inline JsonBaseReader::JsonType_t getExpectedType() const {
		return _expectedType;
	}

	inline String getValue() const {
		return _value;
	}
	inline void setValue(const String &value) {
		_value = value;
	}

	inline JsonBaseReader::JsonType_t getType() const {
		return _type;
	}
	inline void setType(JsonBaseReader::JsonType_t type) {
		_type = type;
	}

	inline long getLong() const {
		return _value.toInt();
	}

	inline int getInt() const {
		return (int)_value.toInt();
	}

	inline float getFloat() const {
		return _value.toFloat();
	}

	bool getBoolean() const;

	inline bool isTrue() const {
		return getBoolean();
	}

private:
	String _path;
	JsonBaseReader::JsonType_t _expectedType;
	JsonBaseReader::JsonType_t _type;
	String _value;
};


typedef std::vector<JsonReadVar> JsonVarReaderVector;

class JsonVarReader : public JsonBaseReader {
public:
	JsonVarReader(Stream &stream) : JsonBaseReader(stream) {
	}

	JsonReadVar &add(const String &path, JsonBaseReader::JsonType_t expectedType) {
		auto var = JsonReadVar(path, expectedType);
		_list.push_back(var);
		return _list.back();
	}

	JsonReadVar &addString(const String &path) {
		return add(path, JsonBaseReader::JsonType_t::JSON_TYPE_STRING);
	}

	JsonReadVar &addInt(const String &path) {
		return add(path, JsonBaseReader::JsonType_t::JSON_TYPE_INT);
	}

	JsonReadVar &addFloat(const String &path) {
		return add(path, JsonBaseReader::JsonType_t::JSON_TYPE_FLOAT);
	}

	JsonReadVar &addBool(const String &path) {
		return add(path, JsonBaseReader::JsonType_t::JSON_TYPE_BOOLEAN);
	}

	JsonReadVar &addNull(const String &path) {
		return add(path, JsonBaseReader::JsonType_t::JSON_TYPE_NULL);
	}

	JsonReadVar &addAny(const String &path) {
		return add(path, JsonBaseReader::JsonType_t::JSON_TYPE_ANY);
	}

	JsonReadVar &addEmptyArray(const String &path) {
		return add(path, JsonBaseReader::JsonType_t::JSON_TYPE_EMPTY_ARRAY);
	}

	JsonReadVar &addEmptyObject(const String &path) {
		return add(path, JsonBaseReader::JsonType_t::JSON_TYPE_EMPTY_OBJECT);
	}


	JsonReadVar *find(const String &path);

	void dumpVar(Print &out, const JsonReadVar &var);
	void dump(Print &out);

	inline void reserve(size_t n) {
		_list.reserve(n);
	}

	inline JsonReadVar &get(int index) {
		return _list.at(index);
	}

private:
	virtual bool processElement() override;

private:
	JsonVarReaderVector _list;
	static const JsonReadVar _invalidJsonReadVar;
};
