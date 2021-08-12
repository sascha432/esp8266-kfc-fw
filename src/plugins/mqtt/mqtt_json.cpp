/**
 * Author: sascha_lammers@gmx.de
 */

#include "mqtt_client.h"
#include "mqtt_json.h"
#include "JsonVar.h"

#if DEBUG_MQTT_JSON
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif


using namespace MQTT::Json;

Reader::Reader(Stream *stream) :
    JsonBaseReader(stream)
{
    clear();
}

Reader::Reader() : Reader(nullptr)
{
    clear();
}

bool Reader::processElement()
{
    __LDBG_printf("key=%s val=%s type=%s path=%s level=%u", _keyStr.c_str(), _valueStr.c_str(), jsonType2String(getType()), getPath().c_str(), getLevel());
    auto path = getPath();
    switch(getType()) {
        case JsonType_t::JSON_TYPE_INT:
        case JsonType_t::JSON_TYPE_FLOAT:
        case JsonType_t::JSON_TYPE_NUMBER:
        if (path == F("brightness")) {
                brightness = getIntValue();
            }
            else if (path == F("color_temp")) {
                color_temp = getIntValue();
            }
            else if (path == F("white_value")) {
                white_value = getIntValue();
            }
            else if (path == F("transition")) {
                transition = JsonVar::getDouble(_valueStr.c_str());
            }
            else if (path == F("state")) {
                state = getType() == JsonType_t::JSON_TYPE_FLOAT ? (JsonVar::getDouble(_valueStr.c_str()) != 0) : (getIntValue() != 0);
            }
            else if (path == F("effect")) {
                effect = _valueStr;
            }
            else if (path.startsWith(F("color.")) && path.length() == 7) {
                switch(path.charAt(6)) {
                    case 'r':
                        color.r = static_cast<uint8_t>(getIntValue());
                        color.has_r = true;
                        break;
                    case 'g':
                        color.g = static_cast<uint8_t>(getIntValue());
                        color.has_g = true;
                        break;
                    case 'b':
                        color.b = static_cast<uint8_t>(getIntValue());
                        color.has_b = true;
                        break;
                    case 'x':
                        color.x = JsonVar::getDouble(_valueStr.c_str());
                        color.has_x = true;
                        break;
                    case 'y':
                        color.y = JsonVar::getDouble(_valueStr.c_str());
                        color.has_y = true;
                        break;
                    case 'h':
                        color.h = JsonVar::getDouble(_valueStr.c_str());
                        color.has_h = true;
                        break;
                    case 's':
                        color.s = JsonVar::getDouble(_valueStr.c_str());
                        color.has_s = true;
                        break;
                }
            }
            break;
        case JsonType_t::JSON_TYPE_STRING:
            if (path == F("effect")) {
                effect = _valueStr;
            }
            else if (path == F("state")) {
                state = MQTT::Client::toBool(_valueStr.c_str());
            }
            break;
        case JsonType_t::JSON_TYPE_BOOLEAN:
            if (path == F("state")) {
                auto var = JsonVar(getType(), _valueStr);
                state = static_cast<int8_t>(var.getBooleanValue());
            }
            break;
        default:
            break;
    }
	return true;
}
