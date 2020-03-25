/**
* Author: sascha_lammers@gmx.de
*/

#include "JsonMapReader.h"

#if DEBUG_KFC_JSON
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

JsonMapReader::JsonMapReader(Stream & stream) : JsonBaseReader(stream) {
    _filter = nullptr;
}

void JsonMapReader::setFilter(FilterCallback_t filter)
{
    _filter = filter;
}

bool JsonMapReader::processElement() {
    String path = getPath();
    if (!_filter || _filter(path, getValue(), getType(), *this)) {
        _map[path] = JsonVar(getType(), getValue());
    }
	return true;
}

void JsonMapReader::dump(Print &out) {
	for (auto &element : _map) {
		auto &var = element.second;
		out.printf_P(PSTR("%s=%s (%s)\n"), element.first.c_str(), JsonVar::formatValue(var.getValue().c_str(), var.getType()).c_str(), jsonType2String(var.getType()).c_str());
	}
}

JsonVar JsonMapReader::get(const String &path) const {
	auto it = _map.find(path);
	if (it != _map.end()) {
		return it->second;
	}
	return JsonVar();
}
