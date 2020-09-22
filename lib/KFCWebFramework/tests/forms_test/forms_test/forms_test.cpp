
//#include <misc.h>
//#include <array>
//#include <stl_ext.h>
//#include <assert.h>
//#include <PrintString.h>
//#include <PrintHtmlEntitiesString.h>
//#include "KFCForms.h"
//#include "EnumBase.h"
//#include "EnumBitset.h"
//
//#include "Form/Compat.h"

#include <Arduino_compat.h>
#include <pgmspace.h>


class YYY {
public:
    YYY() : _copy(0), _move(0), _deleted(false) {
    }

    YYY(const YYY &y) : _copy(y._copy + 1), _move(y._move), _deleted(y._deleted) {
    }
    YYY(YYY &&y) noexcept : _copy(y._copy), _move(y._move + 1), _deleted(y._deleted) {
        y._deleted = true;
    }

    ~YYY() {
        Serial.printf("move=%u copy=%u del=%u\n", _move, _copy, _deleted);
    }

    int _copy, _move;
    bool _deleted;
};

class XXX {
public:

    void processArgs() {
        Serial.println("no args");
    }

    template <typename _Ta>
    void processArgs(_Ta &&arg) {
        Serial.printf("<%s> args=%u ptr=%u ref=%u arr=%u class=%u const=%u\n", 
            typeid(_Ta).name(), 
            0, 
            std::is_pointer_v<_Ta>, 
            std::is_reference_v<_Ta>,
            std::is_array_v<_Ta>,
            std::is_class_v<_Ta>,
            std::is_const_v<_Ta>
       );
    }

    template <typename _Ta, typename... Args>
    void processArgs(_Ta &&arg, Args &&... args) {
        Serial.printf("<%s> args=%u ptr=%u ref=%u arr=%u class=%u const=%u\n",
            typeid(_Ta).name(),
            sizeof...(args),
            std::is_pointer_v<_Ta>,
            std::is_reference_v<_Ta>,
            std::is_array_v<_Ta>,
            std::is_class_v<_Ta>,
            std::is_const_v<_Ta>
        );
        processArgs(args...);
    }

    template <typename... Args>
    void vprintf_va_list(Args &&... args) {
        Serial.printf("args=%u\n", sizeof...(args));
        processArgs(args...);
    }

    template <typename _Ta, typename... Args>
    void printf_P(const _Ta *format, const Args &... args) {
        Serial.println("---- vprintf_va_list(std::forward<const Args &>(args)...); -------");
        vprintf_va_list((const char *)format, std::forward<const Args &>(args)...);
        //Serial.println("---- vprintf_va_list() ----");
        //vprintf_va_list();
        //Serial.println("---- vprintf_va_list(args...) ----");
        //vprintf_va_list(format, args...);
    }
};

void test_form_01_cpp();
void test_form_02_cpp();
void test_form_03_cpp();


YYY testfunc(YYY &&a) {
    return a;
}


#include <stl_ext_all.h>
#include <vector>
#include <array>
#include <algorithm>
#include <type_traits>

#include "Utility/ProgMemHelper.h"

#include "KFCForms.h"
typedef enum WiFiMode
{
    WIFI_OFF = 0, WIFI_STA = 1, WIFI_AP = 2, WIFI_AP_STA = 3
} WiFiMode_t;


#include <stl_ext/variant.h>


int main() {

    ESP._enableMSVCMemdebug();
    DEBUG_HELPER_INIT();

    std::variant<int32_t, uint32_t, float> variant2;//(1.77f);

    //variant2._index = 2;
    //variant2._union._next._next._value = 1.23f;

    //std_ex::nth_element<1, int, float>;

    //variant2 = nullptr;

    //if (std::holds_alternative<float>(variant2)) {
    //    Serial.println("float");
    //}

    variant2 = 1.5f;

    if (std::holds_alternative<float>(variant2)) {
        Serial.println("float");
    }
    if (!std::holds_alternative<int>(variant2)) {
        Serial.println("not int");
    }

    //variant2 = {};

    //*std_ex::get<float>(&variant2) = 1.5;

    //d::nth_element<0, int32_t, uint32_t, float, double>::type;

    /*variant2.index_of<float>();*/

    //auto aa = std_ex::get_if<float>(&variant2);


    auto a = std::get_if<0>(&variant2);
    auto b = std::get_if<1>(&variant2);
    auto c = std::get_if<2>(&variant2);
    auto d = std::get_if<float>(&variant2);

    //if (d) {
    //    Serial.println(*d);
    //}
    return 0;

    //std_ex::variant_index<1, int32_t, uint32_t, float, double>::type x;

    //constexpr size_t index1 = 
    //auto x = variant2._union._next._next._next._index;
    //constexpr size_t index2 = variant2._data.;


    //std::variant < int32_t, uint32_t, float, String, const __FlashStringHelper * > variant1;

    //constexpr size_t variant1Size = sizeof(variant1);
    //constexpr size_t variant2Size = sizeof(variant2);
    //constexpr size_t variant2UnionSize = sizeof(variant2._union);
    //constexpr size_t stringSize = sizeof(String);

    //variant1 = (int16_t)-100;

    //auto result1 = std::get_if<0>(&variant1);
    //auto index1 = variant1.index();

    //variant1 = F("test");

    //auto result2 = std::get_if<4>(&variant1);
    //index1 = variant1.index();

    //auto str1 = String(1.23, 2);

    //variant1 = str1;

    //auto result3 = std::get_if<3>(&variant1);
    //index1 = variant1.index();

    //variant1 = nullptr;

    //index1 = variant1.index();
    //auto result4 = std::get_if<0>(&variant1);


    ////PROGMEM_DEF_LOCAL_VARNAMES(_VAR_, 6, (name)(value));

    //for (int i = 0; i < 6; i++) {
    //    Serial.println(FPSTR(_VAR_name[i]));
    //}
    //for (int i = 0; i < 6; i++) {
    //    Serial.println(FPSTR(_VAR_value[i]));
    //}

    //XXX().printf_P(F("test str=%s int=%d float=%f"), "test", 123, 1.23456789f);

#if 0
    char *strx = strdup("test");

    FormUI::Container::List items(
        WIFI_OFF, FSPGM(Disabled),
        WIFI_STA, strx,
        (int8_t)-100, strx,
        (int16_t)WIFI_AP, FSPGM(Access_Point),
        (long)WIFI_AP_STA, F("Access Point and Station Mode"),
        'A', F("char"),
        123.4567890f, 123.456789012345
    );

    free(strx);

    items.dump(Serial, "created");

    FormUI::Container::List items2(1,2);
    
    items2 = items;

    items2.dump(Serial, "assign op");

    FormUI::Container::List items3(items);

    items3.dump(Serial, "copy ctor");

    FormUI::Container::List items4(std::move(items));

    items4.dump(Serial, "move ctor");

    FormUI::Container::List channelItems(0, FSPGM(Auto));
    for (uint8_t i = 1; i <= 11; i++) {
        channelItems.push_back(i, i);
    }

    channelItems.dump(Serial);
#endif

    test_form_02_cpp();
        

    return 0;
}

