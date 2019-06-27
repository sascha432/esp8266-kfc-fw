/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "JsonBaseReader.h"

class JsonReadVar {
public:
	JsonReadVar() {
		_expectedType = JsonBaseReader::JsonType_t::JSON_TYPE_INVALID;
		_type = JsonBaseReader::JsonType_t::JSON_TYPE_INVALID;
	}
	JsonReadVar(const String &path, JsonBaseReader::JsonType_t expectedType) {
		_path = path;
		_expectedType = expectedType;
		_type = JsonBaseReader::JsonType_t::JSON_TYPE_INVALID;
	}

	// valid value found?
	operator boolean() {
		return _path.length() && isTypeValid();
	}

	boolean equals(const String &path) const {
		return _path.equals(path);
	}

	bool equals(const JsonReadVar &var) const {
		return var.getPath() == _path;
	}

	const String &getPath() const {
		return _path;
	}

	// invalid class / path not found
	bool isEmpty() const {
		return _path.length() == 0;
	}

	bool notFoundInResponse() {
		return (_type == JsonBaseReader::JsonType_t::JSON_TYPE_INVALID);
	}

	bool isTypeValid() {
		return (_type != JsonBaseReader::JsonType_t::JSON_TYPE_INVALID) && (_expectedType == _type || _expectedType == JsonBaseReader::JsonType_t::JSON_TYPE_ANY);
	}

	JsonBaseReader::JsonType_t getExpectedType() const {
		return _expectedType;
	}

	String getValue() const {
		return _value;
	}
	void setValue(const String &value) {
		_value = value;
	}

	JsonBaseReader::JsonType_t getType() const {
		return _type;
	}
	void setType(JsonBaseReader::JsonType_t type) {
		_type = type;
	}

	long getLong() {
		return _value.toInt();
	}

	int getInt() {
		return (int)_value.toInt();
	}

	float getFloat() {
		return _value.toFloat();
	}

	bool getBoolean() {
		return _value.equalsIgnoreCase(F("true"));
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


	JsonReadVar &find(const String &path) {
		for (auto &element : _list) {
			if (element.equals(path)) {
				return element;
			}
		}
		return (JsonReadVar &)_invalidJsonReadVar;
	}

	void dumpVar(Print &out, const JsonReadVar &var) {
		if (var.isEmpty()) {
			out.printf("Invalid variable\n");
		} else {
			out.printf("addr %p\npath %s\nexcpectedType %s\ntype %s\nvalue %s\n", &var, var.getPath().c_str(), jsonType2String(var.getExpectedType()).c_str(), jsonType2String(var.getType()).c_str(), var.getValue().c_str());
		}
	}

	void dump(Print &out) {
		for (const auto &element : _list) {
			dumpVar(out, element);
		}
	}

	void reserve(size_t n) {
		_list.reserve(n);
	}

	JsonReadVar &get(int index) {
		return _list[index];
	}

private:
	virtual bool processElement() override {
		JsonReadVar &var = find(getPath());
		if (!var.isEmpty()) {
			var.setValue(getValue());
			var.setType(getType());
		}
		return true;
	}

private:
	JsonVarReaderVector _list;
	static const JsonReadVar _invalidJsonReadVar;
};
