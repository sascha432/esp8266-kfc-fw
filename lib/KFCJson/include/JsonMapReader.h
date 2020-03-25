/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "JsonBaseReader.h"
#include "JsonVar.h"

// read (and filter) JSON into key/value pairs

typedef std::map<String, JsonVar> JsonMemoryMap;

class JsonMapReader : public JsonBaseReader {
public:
    typedef std::function<bool(const String &path, const String &value, JsonBaseReader::JsonType_t type, JsonMapReader &reader)> FilterCallback_t;

    JsonMapReader(Stream &stream);

    void setFilter(FilterCallback_t filter);

	virtual bool processElement() override;

    JsonVar get(const String &path) const;

    void dump(Print &out);

private:
    FilterCallback_t _filter;
	JsonMemoryMap _map;
};
