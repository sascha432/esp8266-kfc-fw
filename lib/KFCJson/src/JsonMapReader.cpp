/**
* Author: sascha_lammers@gmx.de
*/

#include "JsonMapReader.h"

JsonVar::JsonVar() {
	_type = JsonBaseReader::JSON_TYPE_INVALID;
}

JsonVar::JsonVar(JsonBaseReader::JsonType_t type) {
	_type = type;
}

JsonVar::JsonVar(JsonBaseReader::JsonType_t type, const String &value) {
	_type = type;
	_value = value;
}

void JsonVar::setValue(const String &value) {
	_value = value;
}

String JsonVar::getValue() const {
	return _value;
}

JsonBaseReader::JsonType_t JsonVar::getType() const {
	return _type;
}


bool JsonMapReader::processElement() {
	_map[getPath()] = JsonVar(getType(), getValue());
	return true;
}

void JsonMapReader::dump(Print &out) {
	for (auto &element : _map) {
		auto &var = element.second;
		out.printf_P(PSTR("%s=%s (%s)\n"), element.first.c_str(), formatValue(var.getValue().c_str(), var.getType()).c_str(), jsonType2String(var.getType()).c_str());
	}
}

JsonVar JsonMapReader::get(const String &path) {
	const auto &it = _map.find(path);
	if (it != _map.end()) {
		return it->second;
	}
	return JsonVar();
}
