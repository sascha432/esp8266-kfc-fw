
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




int main() {

    ESP._enableMSVCMemdebug();
    DEBUG_HELPER_INIT();


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

    test_form_01_cpp();
        

    return 0;
}

