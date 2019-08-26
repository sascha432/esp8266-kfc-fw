// json_test.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <assert.h>
#include <PrintString.h>
#include "JsonCallbackReader.h"
#include "BufferStream.h"
#include "JsonConverter.h"
#include "JsonValue.h"
#include "JsonBuffer.h"
#include "KFCJson.h"


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

void * operator new(size_t size) 
{ 
    if (_mem_usage_stack) {
        return malloc(size);
    } else {
        _mem_usage_stack++;
        void * p = malloc(size); 
        mem_usage += size;
        peak_mem_usage = std::max(peak_mem_usage, mem_usage);
        printf("new %u (%u)\n", size, mem_usage);
        {
            PrintString s("%p", p);
            //mem.insert(std::pair<String,size_t>(String(s),size));
            mem[String(s)] = size;
        }
        _mem_usage_stack--;
        return p; 
    }
} 

void operator delete(void *p) {
    if (_mem_usage_stack) {
        free(p);
    } else {
        _mem_usage_stack++;
        {
            PrintString s("%p", p);
            String ss(s);
            if (mem.find(ss) != mem.end()) {
                printf("delete %u (%u)\n", mem[ss], mem_usage);
                mem_usage -= mem[ss];
            }
            else {
                printf("delete no ptr\n");
            }
        }
        free(p);
        _mem_usage_stack--;
    }
}

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
    v.emplace_back(std::move(str));
}

int main()
{
    ESP._enableMSVCMemdebug();

    {
        
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
        arr1.addArray().addArray().addArray().addArray().addArray().addArray();

        auto find = json1.find("arr");

        json1.replace("str", "new string value");

        //for (auto &json : json1) {

        //    printf("%d\n", json.getType());

        //}

        //json1.add("float", 1.0);
        //auto &arr1 = json1.addArray("arr");

        //print_end_mem_usage();
        //_mem_usage_stack = 0;

        json1.printTo(Serial);
        printf("\n");

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

        print_end_mem_usage();

        delete &json1;
    }

    return 0;


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

    tests();
}

