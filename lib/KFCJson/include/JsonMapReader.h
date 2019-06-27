/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "JsonBaseReader.h"

class JsonVar {
public:
	JsonVar();
	JsonVar(JsonBaseReader::JsonType_t type);
	JsonVar(JsonBaseReader::JsonType_t type, const String &value);

	void setValue(const String &value);
	String getValue() const;
	JsonBaseReader::JsonType_t getType() const;

private:
	JsonBaseReader::JsonType_t _type;
	String _value;
};

class JsonReadVar;

typedef std::map<String, JsonVar> JsonMemoryMap;

class JsonMapReader : public JsonBaseReader {
public:
	JsonMapReader(Stream &stream) : JsonBaseReader(stream) {
	}

	virtual bool processElement() override;

	void dump(Print &out);

	JsonVar get(const String &path);

private:
	JsonMemoryMap _map;
};
