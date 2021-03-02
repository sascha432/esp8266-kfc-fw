/**
 * Author: sascha_lammers@gmx.de
 */

#include "mqtt_client.h"
#include "mqtt_json.h"
#include "JsonVar.h"

using namespace MQTT;

JsonReader::JsonReader(Stream *stream) :
    JsonBaseReader(stream)
{
    clear();
}

JsonReader::JsonReader() : JsonReader(nullptr)
{
    clear();
}


bool JsonReader::processElement() {
    // Serial.printf_P(PSTR("key=%s val=%s type=%s path=%s level=%u\n"), _keyStr.c_str(), _valueStr.c_str(), jsonType2String(getType()), getPath().c_str(), getLevel());
    auto path = getPath();
    switch(getType()) {
        case JsonType_t::JSON_TYPE_INT:
        case JsonType_t::JSON_TYPE_FLOAT:
        case JsonType_t::JSON_TYPE_NUMBER:
        if (path == F("brightness")) {
                _brightness = getIntValue();
            }
            else if (path == F("color_temp")) {
                _color_temp = getIntValue();
            }
            else if (path == F("white_value")) {
                _white_value = getIntValue();
            }
            else if (path == F("transition")) {
                _transition = JsonVar::getDouble(_valueStr.c_str());
            }
            else if (path == F("state")) {
                _state = getType() == JsonType_t::JSON_TYPE_FLOAT ? (JsonVar::getDouble(_valueStr.c_str()) != 0) : (getIntValue() != 0);
            }
            else if (path == F("effect")) {
                _effect = _valueStr;
            }
            else if (path.startsWith(F("color.")) && path.length() == 7) {
                switch(path.charAt(6)) {
                    case 'r':
                        _color.r = static_cast<uint8_t>(getIntValue());
                        _color.has_r = true;
                        break;
                    case 'g':
                        _color.g = static_cast<uint8_t>(getIntValue());
                        _color.has_g = true;
                        break;
                    case 'b':
                        _color.b = static_cast<uint8_t>(getIntValue());
                        _color.has_b = true;
                        break;
                    case 'x':
                        _color.x = JsonVar::getDouble(_valueStr.c_str());
                        _color.has_x = true;
                        break;
                    case 'y':
                        _color.y = JsonVar::getDouble(_valueStr.c_str());
                        _color.has_y = true;
                        break;
                    case 'h':
                        _color.h = JsonVar::getDouble(_valueStr.c_str());
                        _color.has_h = true;
                        break;
                    case 's':
                        _color.s = JsonVar::getDouble(_valueStr.c_str());
                        _color.has_s = true;
                        break;
                }
            }
            break;
        case JsonType_t::JSON_TYPE_STRING:
            if (path == F("effect")) {
                _effect = _valueStr;
            }
            else if (path == F("state")) {
                _state = MQTTClient::toBool(_valueStr.c_str());
            }
            break;
        case JsonType_t::JSON_TYPE_BOOLEAN:
            if (path == F("state")) {
                auto var = JsonVar(getType(), _valueStr);
                _state = static_cast<int8_t>(var.getBooleanValue());
            }
            break;
        default:
            break;
    }
    // out.printf_P(PSTR("%s=%s (%s)\n"), element.first.c_str(), JsonVar::formatValue(var.getValue().c_str(), var.getType()).c_str(), jsonType2String(var.getType()));
	return true;
}
