// json_test.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <assert.h>
#include <PrintString.h>
#include <DumpBinary.h>
#include <PrintHtmlEntitiesString.h>
#include "JsonCallbackReader.h"
#include "BufferStream.h"
#include "JsonConverter.h"
#include "JsonValue.h"
#include "JsonBuffer.h"
#include "KFCJson.h"

PROGMEM_STRING_DECL(mime_application_json);
PROGMEM_STRING_DEF(mime_application_json, "application/json");

int mem_usage, peak_mem_usage;
int _mem_usage_stack = 1;
std::map<String, size_t> mem;

void begin_mem_usage() {
    mem_usage = 0;
    peak_mem_usage = 0;
    mem.clear();
    _mem_usage_stack = 0;
}

void print_end_mem_usage() {
    printf("mem usage: %u\npeak usage: %u\n", mem_usage, peak_mem_usage);
    _mem_usage_stack = 1;
    mem.clear();
}

//void * operator new(size_t size) 
//{ 
//    if (_mem_usage_stack) {
//        return malloc(size);
//    } else {
//        _mem_usage_stack++;
//        void * p = malloc(size); 
//        mem_usage += size;
//        peak_mem_usage = std::max(peak_mem_usage, mem_usage);
//        printf("new %u (%u)\n", size, mem_usage);
//        {
//            PrintString s("%p", p);
//            //mem.insert(std::pair<String,size_t>(String(s),size));
//            mem[String(s)] = size;
//        }
//        _mem_usage_stack--;
//        return p; 
//    }
//} 
//
//void operator delete(void *p) {
//    if (_mem_usage_stack) {
//        free(p);
//    } else {
//        _mem_usage_stack++;
//        {
//            PrintString s("%p", p);
//            String ss(s);
//            if (mem.find(ss) != mem.end()) {
//                printf("delete %u (%u)\n", mem[ss], mem_usage);
//                mem_usage -= mem[ss];
//            }
//            else {
//                printf("delete no ptr\n");
//            }
//        }
//        free(p);
//        _mem_usage_stack--;
//    }
//}

void print_escaped(String output) {
    output.replace("\\", "\\\\");
    output.replace("\"", "\\\"");
    output.replace("\n", "\\n");
    printf("\n\n\nSTRING = \"%s\";\n\n\n", output.c_str());
}


void tests() {
    String test = "{\"status\":\"OK\",\"message\":\"\",\"countryCode\":\"CA\",\"zoneName\":\"America/Vancouver\",\"abbreviation\":\"PST\",\"gmOffset\":-28800,\"dst\":\"0\",\"zoneStart\":1552212000,\"zoneEnd\":1583661599,\"nextAbbreviation\":\"PDT\",\"timestamp\":1542119691,\"formatted\":\"2018-11-13 14:34:51\"}";
    BufferStream stream;
    stream.write(test);

    String output;
    JsonCallbackReader reader(stream, [&output](const String &key, const String &value, size_t partialLength, JsonBaseReader &json) {
        char tmp[1024];
        snprintf(tmp, sizeof(tmp), "key %s = val %s, level = %d, path = %s, key pos %d, value pos %d\n", key.c_str(), value.c_str(), json.getLevel(), json.getPath().c_str(), (int)json.getKeyPosition(), (int)json.getValuePosition());
        output += tmp;
        return true;
    });
    reader.parse();

    assert(output == "key status = val OK, level = 1, path = status, key pos 2, value pos 11\nkey message = val , level = 1, path = message, key pos 16, value pos 16\nkey countryCode = val CA, level = 1, path = countryCode, key pos 29, value pos 43\nkey zoneName = val America/Vancouver, level = 1, path = zoneName, key pos 48, value pos 59\nkey abbreviation = val PST, level = 1, path = abbreviation, key pos 79, value pos 94\nkey gmOffset = val -28800, level = 1, path = gmOffset, key pos 100, value pos 110\nkey dst = val 0, level = 1, path = dst, key pos 118, value pos 124\nkey zoneStart = val 1552212000, level = 1, path = zoneStart, key pos 128, value pos 139\nkey zoneEnd = val 1583661599, level = 1, path = zoneEnd, key pos 151, value pos 160\nkey nextAbbreviation = val PDT, level = 1, path = nextAbbreviation, key pos 172, value pos 191\nkey timestamp = val 1542119691, level = 1, path = timestamp, key pos 197, value pos 208\nkey formatted = val 2018-11-13 14:34:51, level = 1, path = formatted, key pos 220, value pos 232\n");

    {
        BufferStream stream;
        const char *str = "[ NULL, true, false, 0, 0.0001, 5e3, 5e-3, \"string\", [ ], { } ]";
        stream.write(str);

        JsonConverter reader(stream);
        reader.parse();

        assert(reader.getLastError().type == JsonBaseReader::JSON_ERROR_NONE);
        assert(reader.getRoot() != nullptr);

        PrintString output;
        reader.getRoot()->printTo(output);
        assert(output.equals("[null,true,false,0,0.0001,5e3,5e-3,\"string\",[],{}]"));

        delete reader.getRoot();
    }

    {
        BufferStream stream;
        const char *str = "[\"key\":true]";
        stream.write(str);

        JsonConverter reader(stream);
        reader.parse();

        assert(reader.getLastError().type == JsonBaseReader::JSON_ERROR_ARRAY_WITH_KEY);
        assert(reader.getRoot() != nullptr);

        delete reader.getRoot();
    }

    {
        BufferStream stream;
        const char *str = "[true,,false]";
        stream.write(str);

        JsonConverter reader(stream);
        reader.parse();

        assert(reader.getLastError().type == JsonBaseReader::JSON_ERROR_EMPTY_VALUE);
        assert(reader.getRoot() != nullptr);

        delete reader.getRoot();
    }

    {
        BufferStream stream;
        const char *str = "[}";
        stream.write(str);

        JsonConverter reader(stream);
        reader.parse();

        assert(reader.getLastError().type == JsonBaseReader::JSON_ERROR_INVALID_END);
        assert(reader.getRoot() != nullptr);

        delete reader.getRoot();
    }

    assert(JsonVar::getDouble("5.10") == atof("5.10"));
    assert(JsonVar::getDouble("5.55") == atof("5.55"));
    assert(JsonVar::getDouble("-5.55") == atof("-5.55"));
    assert(JsonVar::getDouble("5.55e-1") == atof("0.555"));

    assert(JsonVar::getInt("5.0000e2") == 500);
    assert(JsonVar::getInt("5.0e2") == 500);
    assert(JsonVar::getInt("5.5500e2") == 555);
    assert(JsonVar::getInt("5.550000e2") == 555);
    assert(JsonVar::getInt("5.55e5") == 555000);
    assert(JsonVar::getUnsigned("4e9") == 4000000000UL);
    assert(JsonVar::getUnsigned("4000000000") == 4000000000UL);
    assert(JsonVar::getInt("-500e-2") == -5);


    assert(JsonVar::getNumberType("500.00e-2") == (1|((uint8_t)JsonVar::NumberType_t::EXPONENT)));
    assert(JsonVar::getNumberType("500.00e-3") == (2|((uint8_t)JsonVar::NumberType_t::EXPONENT)));
    assert(JsonVar::getNumberType("0.001e3") == (1|((uint8_t)JsonVar::NumberType_t::EXPONENT)));
    assert(JsonVar::getNumberType("0.001e2") == (2|((uint8_t)JsonVar::NumberType_t::EXPONENT)));

    assert(JsonVar::getNumberType("0") == 1);
    assert(JsonVar::getNumberType("0.00") == 1);
    assert(JsonVar::getNumberType("5") == 1);
    assert(JsonVar::getNumberType("5.00") == 1);
    assert(JsonVar::getNumberType("500.00") == 1);
    assert(JsonVar::getNumberType("-500.00") == (1|((uint8_t)JsonVar::NumberType_t::NEGATIVE)));
    assert(JsonVar::getNumberType("5.5") == 2);
    assert(JsonVar::getNumberType("5.005") == 2);
    assert(JsonVar::getNumberType("5.1000") == 2);
    assert(JsonVar::getNumberType("0.001") == 2);

}

std::vector<JsonString> v;

void test(JsonString &&str) {
    v.emplace_back(std::move(str));
}

void test(const JsonString &str) {
    v.emplace_back(str);
}

int malloc_size(int size) {
    if (size % 16) {
        size += 16;
    }
    return size & ~15;
}

// returns number of digits without trailing zeros after determining max. precision
int count_decimals(double value, uint8_t max_precision = 6, uint8_t max_decimals = 8) {
    auto precision = max_precision;
    auto number = abs(value);
    if (number < 1) {
        while ((number = (number * 10)) < 1 && precision < max_decimals) { // increase precision for decimals
            precision++;
        }
    }
    else {
        while ((number = (number / 10)) > 1 && precision > 1) { // reduce precision for decimals
            precision--;
        }
    }
    char format[8];
    snprintf_P(format, sizeof(format), PSTR("%%.%uf"), precision + 1);
    char buf[32];
auto len = snprintf_P(buf, sizeof(buf), format, value);
if (len > 1) {
    buf[--len] = 0;
}
char *ptr;
if (len < sizeof(buf) && (ptr = strchr(buf, '.'))) {
    ptr++;
    char *endPtr = ptr + strlen(ptr);
    while (--endPtr > ptr &&*endPtr == '0') { // remove trailing zeros
        *endPtr = 0;
        precision--;
    }
}
else {
    precision = max_decimals; // buffer to small
}
printf("%.*f ", precision, value);
return precision;
}

#include "C:\Users\sascha\Documents\PlatformIO\Projects\kfc_fw\src\plugins\home_assistant\HassJsonReader.h"

class HassStates : public JsonVariableReader::Result {
public:
    HassStates() {
    }

    virtual bool empty() const {
        return !_friendlyName.length() || !_entityId.length() || !_state.length();
    }

    virtual Result *create() {
        if (!empty()) {
            return new HassStates(std::move(*this));
        }
        return nullptr;
    }

    static void apply(JsonVariableReader::ElementGroup &group) {
        group.initResultType<HassStates>();
        group.add(F("entity_id"), [](Result &result, JsonVariableReader::Reader &reader) {
            auto &value = reader.getValueRef();
            if (!value.startsWith(F("light.")) && !value.startsWith(F("switch."))) {
                return false;
            }
            if (value.length()) {
                reinterpret_cast<HassStates &>(result).setFriendlyName(value);
            }
            return true;
        });
        group.add(F("state"), [](Result &result, JsonVariableReader::Reader &reader) {
            auto &value = reader.getValueRef();
            if (value.equals(F("unavailable"))) {
                return false;
            }
            if (value.length()) {
                reinterpret_cast<HassStates &>(result).setState(value);
            }
            return true;
        });
        group.add(F("attributes.friendly_name"), [](Result &result, JsonVariableReader::Reader &reader) {
            auto &value = reader.getValueRef();
            if (value.length()) {
                reinterpret_cast<HassStates &>(result).setEntityId(value);
            }
            return true;
        });
    }

    void setFriendlyName(const String &friendlyName) {
        _friendlyName = friendlyName;
    }

    void setEntityId(const String &entityId) {
        _entityId = entityId;
    }

    void setState(const String &state) {
        _state = state;
    }

    void dump() {
        Serial.printf("%s %s %s\n", _friendlyName.c_str(), _entityId.c_str(), _state.c_str());
    }

private:
    String _state;
    String _friendlyName;
    String _entityId;
};

#include "ESP8266HttpClient.h"

#include "RetrieveSymbols.h"
#include "RemoteTimezone.h"



int main()
{
#if _DEBUG
    ESP._enableMSVCMemdebug();
#endif

#if 0

    JsonUnnamedArray attriutes;
    StringVector args;
    args.push_back("");
    args.push_back("");
    args.push_back("test1=\"1234\"");
    args.push_back("test2=123");
    args.push_back("test3=5.4");

    for (uint8_t i = 2; i < args.size(); i++) 
    {
        auto attribute = args.at(i); //args.toString(i);
        auto pos = attribute.indexOf('=');
        if (pos != -1) {
            String name = attribute.substring(0, pos - 1);
            if (attribute.charAt(pos + 1) == '"') {
                String str = attribute.substring(pos + 2);
                if (String_endsWith(str, '"')) {
                    str.remove(str.length() - 1, 1);
                }
                attriutes.add(str);
            }
            else if (attribute.indexOf('.') != -1) {
                attriutes.add(atof(attribute.c_str() + pos + 1));
            }
            else {
                attriutes.add(atoi(attribute.c_str() + pos + 1));
            }
        }
    }
    attriutes.printTo(Serial);

#endif

#if 0

    StringVector addresses;
    addresses.push_back("0x40239dd5");
    addresses.push_back("0x401004c8");
    addresses.push_back("0x402088b8");
    addresses.push_back("0x4021bd90");

    xtra_containers::remove_duplicates(addresses);
    RetrieveSymbols::RestApi rs;
    rs.setAddresses(std::move(addresses));
    String url;
    rs.getRestUrl(url);
    rs.call([](RetrieveSymbols::JsonReaderResult *result, const String &error) {
    });

    LoopFunctions::loop();

#endif


#if 1

    HTTPClient client;

    //client.begin("http://www.d0g3.space/timezone/api.php?by=zone&format=json&zone=America/Vancouver");

    //client.begin("http://192.168.0.3:8123/api/states/light.kfc4f22d0_0");
    //client.begin("http://192.168.0.3:8123/api/states/light.floor_lamp_top");
    
    //client.begin("http://192.168.0.3:8123/api/states/switch.bathroom_lightx");

    //client.begin("http://192.168.0.3:8123/api/services/light/turn_on");
    //light.kfc4f22d0_0
    
    //client.begin("http://192.168.0.3:8123/api/states");

    client.addHeader("Content-Type", "application/json");
    //client.setAuthorization("Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJiMmNlNjkwZDBhMTY0ZDI2YWY4MWUxYzJiNjgzMjM3NCIsImlhdCI6MTU0ODY0MzczMywiZXhwIjoxODY0MDAzNzMzfQ.h1287xhv5nY5Fvu2GMSzIMnP51IsyFtKg9RFCS8qMBQ");
    client.setTimeout(5);

    auto rt = new RemoteTimezone::RestApi();
    rt->setUrl(F("http://www.d0g3.space/timezone/api.php?by=zone&format=json&zone=${timezone}"));
    rt->setZoneName(F("America/Vancouver"));

    rt->setAutoDelete(true);
    rt->call([](RemoteTimezone::JsonReaderResult *result, const String &error) {
        int k = 0;
    });

    LoopFunctions::loop();
    LoopFunctions::loop();

    return 0;

    PrintString payload;
    JsonUnnamedObject json;
    json.add(F("entity_id"), F("light.kfc4f22d0_0"));
    json.add(F("brightness"), 40);
    json.printTo(payload);

    //if (client.POST(payload) == 200) {
    if (client.GET() == 200) {

        client.dump(Serial);
        auto reader = JsonVariableReader::Reader();
        auto elements = reader.getElementGroups();

#if 0
        elements->emplace_back(JsonString());
        HassJsonReader::CallService::apply(elements->back());
        
        reader.initParser();
        reader.setStream(&client.getStream());
        if (reader.parse()) {
            auto &results = reader.getElementGroups()->front().getResults<HassJsonReader::GetState>();
            auto &result = results.front();
            if (result) {
                //Serial.println(result->_brightness);
            }
        }
#endif

        elements->emplace_back(JsonString());
        HassJsonReader::GetState::apply(elements->back());
        reader.initParser();
        reader.setStream(&client.getStream());
        if (reader.parse()) {
            auto &results = reader.getElementGroups()->front().getResults<HassJsonReader::GetState>();
            auto &result = results.front();
            if (result) {
                Serial.println(result->_entitiyId);
                Serial.println(result->_state);
                Serial.println(result->_brightness);
            }
        }

        //auto reader = JsonVariableReader::Reader();

        //auto elements = reader.getElementGroups();
        //elements->emplace_back(F("[]"));
        //HassStates::apply(elements->back());


        //reader.initParser();
        //reader.setStream(&client.getStream());
        //if (reader.parse()) {
        //    for (auto& element : *elements) {
        //        auto& results = element.getResults<HassStates>();
        //        for (auto& result : results) {
        //            result->dump();
        //        }
        //    }
        //}
    }
    else {
        JsonCallbackReader reader(client.getStream(), [](const String& key, const String& value, size_t partialLength, JsonBaseReader& json) {
            if (json.getLevel() == 1 && key.equals(F("message"))) {
                Serial.println(String("Message: ") + value);
            }
            return true;
        });
        reader.parse();
        
        client.dump(Serial);
    }


#endif

#if 0
    auto fstr = F("\t1test1\n2test2\r\r\"3test3\r");
    String str(fstr);
    int len;

    Serial.println("--progmem");

    Serial.println(JsonTools::lengthEscaped(fstr));
    len = JsonTools::printToEscaped(Serial, fstr);
    Serial.println();
    Serial.println(len);

    Serial.println("--string");

    Serial.println(JsonTools::lengthEscaped(str));
    len = JsonTools::printToEscaped(Serial, str);
    Serial.println();
    Serial.println(len);

#endif

#if 0
    printf("%u\n", count_decimals(1.123f));
    printf("%u\n", count_decimals(100.0f));
    printf("%u\n", count_decimals(100.78f));
    printf("%u\n", count_decimals(100.123456f));
    printf("%u\n", count_decimals(10.123456f));
    printf("%u\n", count_decimals(1.123456f));
    printf("%u\n", count_decimals(0.123456f));
    printf("%u\n", count_decimals(0.0123456f));
    printf("%u\n", count_decimals(0.0000000123456f, 6, 15));
    return 0;
#endif

#if 0

    {

        JsonString str(F("\u00b0C test test test test"));

        PrintHtmlEntitiesString str3;

        str.printTo(str3);

        printf("%u\n", sizeof(JsonString::_str_t));

        printf("%s\n", str3.c_str());

        return 0;
           
        //
        //printf("JsonString %u = buffer_size, malloc %u\n", sizeof(JsonString), malloc_size(sizeof(JsonString)));
        //printf("JsonNamedVariant<JsonString> sizeof %u, malloc %u\n", sizeof(JsonNamedVariant<JsonString>), malloc_size(sizeof(JsonNamedVariant<JsonString>)));
        //printf("JsonUnnamedVariant<JsonString> sizeof %u, malloc %u\n", sizeof(JsonUnnamedVariant<JsonString>), malloc_size(sizeof(JsonUnnamedVariant<JsonString>)));
        //printf("JsonNamedVariant<String> sizeof %u, malloc %u\n", sizeof(JsonNamedVariant<String>), malloc_size(sizeof(JsonNamedVariant<String>)));
        //printf("JsonUnnamedVariant<const __FlashStringHelper *> sizeof %u, malloc %u\n", sizeof(JsonUnnamedVariant<const __FlashStringHelper *>), malloc_size(sizeof(JsonUnnamedVariant<const __FlashStringHelper *>)));
        //printf("JsonNamedVariant<const __FlashStringHelper *> sizeof %u, malloc %u\n", sizeof(JsonNamedVariant<const __FlashStringHelper *>), malloc_size(sizeof(JsonNamedVariant<const __FlashStringHelper *>)));

        //return 0;

        
//        BufferStream stream;
//        const char *str = "{\"type\":\"ui\",\"data\":[{\"type\":\"row\",\"extra_classes\":\"title\",\"columns\":[{\"type\":\"group\",\"name\":\"Dimmer\",\"has_switch\":true}]},{\"type\":\"row\",\"columns\":[{\"type\":\"slider\",\"id\":\"dimmer_channel0\",\"name\":\"Dimmer Ch. 1\",\"value\":\"1684\",\"state\":1,\"min\":0,\"max\":16666,\"zero_off\":true}]},{\"type\":\"row\",\"columns\":[{\"type\":\"slider\",\"id\":\"dimmer_channel1\",\"name\":\"Dimmer Ch. 2\",\"value\":\"0\",\"state\":0,\"min\":0,\"max\":16666,\"zero_off\":true},{\"type\":\"sensor\",\"id\":\"dimmer_vcc\",\"name\":\"Dimmer VCC\",\"value\":\"4.793\",\"state\":1,\"columns\":1,\"unit\":\"V\",\"render_type\":\"badge\"},{\"type\":\"sensor\",\"id\":\"dimmer_frequency\",\"name\":\"Dimmer Frequency\",\"value\":\"59.64\",\"state\":1,\"columns\":1,\"unit\":\"Hz\",\"render_type\":\"badge\"},{\"type\":\"sensor\",\"id\":\"dimmer_temp\",\"name\":\"Dimmer Internal Temperature\",\"value\":\"76.00\",\"state\":1,\"columns\":1,\"unit\":\"°C\",\"render_type\":\"badge\"}]},{\"type\":\"row\",\"extra_classes\":\"title\",\"columns\":[{\"type\":\"group\",\"name\":\"Atomic Sun\",\"has_switch\":true}]},{\"type\":\"row\",\"columns\":[{\"type\":\"slider\",\"id\":\"brightness\",\"name\":\"brightness\",\"columns\":6,\"min\":0,\"max\":16666,\"zero_off\":true},{\"type\":\"slider\",\"id\":\"color\",\"name\":\"color\",\"columns\":6,\"color\":\"temperature\"}]},{\"type\":\"row\",\"align\":\"right\",\"columns\":[{\"type\":\"sensor\",\"id\":\"int_temp\",\"name\":\"Internal Temperatur\",\"value\":\"47.28\",\"offset\":3,\"unit\":\"°C\"},{\"type\":\"sensor\",\"id\":\"vcc\",\"name\":\"VCC\",\"value\":\"3.286\",\"unit\":\"V\"},{\"type\":\"sensor\",\"id\":\"frequency\",\"name\":\"Frequency\",\"value\":\"59.869\",\"unit\":\"Hz\"}]},{\"type\":\"row\",\"extra_classes\":\"title\",\"columns\":[{\"type\":\"group\",\"name\":\"Sensors\",\"has_switch\":false}]},{\"type\":\"row\",\"columns\":[{\"type\":\"sensor\",\"id\":\"temperature\",\"name\":\"Temperature\",\"value\":\"25.78\",\"unit\":\"°C\"},{\"type\":\"sensor\",\"id\":\"humidity\",\"name\":\"Humidity\",\"value\":\"47.23\",\"unit\":\"%\"},{\"type\":\"sensor\",\"id\":\"pressure\",\"name\":\"Pressure\",\"value\":\"1023.42\",\"unit\":\"hPa\"}]}]}";
//        stream.write(str);
//
//        begin_mem_usage();
//
//        JsonConverter reader(stream);
//        reader.parse();
//
//        if (reader.getRoot()) {
//            print_end_mem_usage();
//
//            JsonUnnamedObject *root = reinterpret_cast<JsonUnnamedObject *>(reader.getRoot());
//            JsonArray *_rows = reinterpret_cast<JsonArray *>(root->find("data"));
//
//
//            //reader.getRoot()->printTo(Serial);
//            //Serial.println();
//
////            reader.getRoot()->dump();
//
//            delete reader.getRoot();
//        }
//
//
//        return 0;
        
        begin_mem_usage();
        JsonUnnamedObject &json1 = * new JsonUnnamedObject();

        float f = 0.0;
        //json1.add("-inf_float", -1/f);
        //json1.add("inf_float", 1/f);
        //json1.add("nan_float", 0/f);
        //json1.add("float", (double)12345678901234567890);
        //json1.add("float", (double)12345678901234567);
        //json1.add("float", (double)1234567890123456);
        //json1.add("float", (double)123456789012345);
        //json1.add("float", (double)12345678901234);
        //json1.add("float", (double)1234567890123456.7);
        //json1.add("float", (double)123456789012345.67);
        //json1.add("float", (double)12345678901234.567);
        //json1.add("float", 105.02634);
        //json1.add("float", 10.502634);
        //json1.add("float", 1.0502634);
        //json1.add("float", 1.10);
        //json1.add("float", 1.0);
        json1.add("float", 0.00502634);
        json1.add("int", -1);
        json1.add("null", nullptr);
        json1.add("bool", true);
        json1.add("str", "test"); 
        json1.add("F_str", F("test"));
        json1.add("String", String("test"));
        json1.add("JsonString", JsonString("test1"));
        json1.replace("JsonString", JsonString("test2"));
        auto number = JsonNumber("123.12345678901234567890");
        number.validate();
        json1.add("number", number);
        json1.add("number2", JsonNumber(1, 5));
        auto &arr1 = json1.addArray("arr");
        print_end_mem_usage();

        auto &unnamedArr1 = arr1.addArray();
        unnamedArr1.add(1);
        unnamedArr1.add(2);
        unnamedArr1.add(3);
        auto &unnamedArr2 = arr1.addArray();
        unnamedArr2.add(1);
        unnamedArr2.addObject().add("key", 2);
        unnamedArr2.add(3);
        arr1.addArray().addArray().addArray().addArray().addArray().addArray(1).add("string");

        auto find = json1.find("arr");

        json1.replace("str", "new string value");

        //for (auto &json : json1) {

        //    printf("%d\n", json.getType());

        //}

        //json1.add("float", 1.0);
        //auto &arr1 = json1.addArray("arr");

        print_end_mem_usage();
        //_mem_usage_stack = 0;

        //json1.printTo(Serial);
        //printf("\n");

        printf("%u\n", json1.length());

        int len3 = 0;

        {
            JsonBuffer jBuf(json1);
            const int bufSize = 8;
            uint8_t buf[bufSize];
            int len;
            while (len = jBuf.fillBuffer(buf, bufSize)) {
                len3 += len;
                printf("%*.*s", len, len, buf);
            }
            printf("\n");
        }

        //print_end_mem_usage();

        delete &json1;
    }

    return 0;
#endif

#if 0
    //{
    //    //UnnamedJsonObject json1;
    //    JsonObject json1(JsonElement::JSON_OBJECT);
    //    json1.add(String(F("str6")), F("value6"));
    //    json1.add(JsonElement(F("str1"), F("value1")));
    //    json1.add("str3", "value3");
    //    json1.add("int3", -30000);
    //    json1.add("long1", (long)2000000000);
    //    json1.add("uint1", (unsigned)10000);
    //    json1.add("uint16_1", (uint16_t)10000);


    //    auto &arr1 = json1.addJsonArray("arr1");

    //    auto &_unnamed_obj1 = arr1.add(new UnnamedJsonObject());
    //    _unnamed_obj1.add(JsonElement(F("str2"), F("value2")));
    //    _unnamed_obj1.add("str4", "value4");

    //    auto &_unnamed_arr1 = arr1.addJsonArray();
    //    _unnamed_arr1.add("value5");
    //    _unnamed_arr1.add(200000);
    //    auto &val1 = _unnamed_arr1.add(new UnnamedJsonArray());
    //    val1.add(1);
    //    val1.add(2);
    //    val1.add(3);
    //    auto &val2 = _unnamed_arr1.add(new UnnamedJsonObject());
    //    val2.add("1", 1);
    //    val2.add("2", 2);
    //    val2.add("bool", true);

    //    auto &obj1 = json1.addJsonObject(F("obj1"));
    //    obj1.add(JsonElement(F("int1"), -10000));
    //    obj1.add(JsonElement(F("byte1"), (uint8_t)100));
    //    obj1.add(F("int2"), -20000);

    //    printf("length %u=%u\n", json1.length(), json1.toString().length());

    //    print_escaped(json1.toString());

    //    extern void jsonstr_debug_print(bool force = false);
    //    jsonstr_debug_print(true);

    //    auto stream = JsonStream(json1);
    //    while (stream.available()) {
    //        printf("%c", (char)stream.read());
    //    }

    //}
    //return 0;

#endif

#if 0
    tests();
#endif
    return  0;
}

