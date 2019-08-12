/**
* Author: sascha_lammers@gmx.de
*/

#include "JsonVarReader.h"

JsonReadVar::JsonReadVar() {
    _expectedType = JsonBaseReader::JsonType_t::JSON_TYPE_INVALID;
    _type = JsonBaseReader::JsonType_t::JSON_TYPE_INVALID;
}
JsonReadVar::JsonReadVar(const String &path, JsonBaseReader::JsonType_t expectedType) {
    _path = path;
    _expectedType = expectedType;
    _type = JsonBaseReader::JsonType_t::JSON_TYPE_INVALID;
}

JsonReadVar::operator boolean() {
    return _path.length() && isTypeValid();
}

boolean JsonReadVar::getBoolean() const {
    return _value.equalsIgnoreCase(F("true"));
}

JsonReadVar *JsonVarReader::find(const String &path) {
    for (auto &element : _list) {
        if (element.equals(path)) {
            return &element;
        }
    }
    return nullptr;
}

void JsonVarReader::dumpVar(Print &out, const JsonReadVar &var) {
    if (var.isEmpty()) {
        out.printf("Invalid variable\n");
    } else {
        out.printf("addr %p\npath %s\nexcpectedType %s\ntype %s\nvalue %s\n", &var, var.getPath().c_str(), jsonType2String(var.getExpectedType()).c_str(), jsonType2String(var.getType()).c_str(), var.getValue().c_str());
    }
}

boolean JsonVarReader::processElement() {
    auto var = find(getPath());
    if (var) {
        var->setValue(getValue());
        var->setType(getType());
    }
    return true;
}

void JsonVarReader::dump(Print &out) {
    for (const auto &element : _list) {
        dumpVar(out, element);
    }
}
