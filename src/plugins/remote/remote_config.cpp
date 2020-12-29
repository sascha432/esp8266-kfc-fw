/**
 * Author: sascha_lammers@gmx.de
 */

#include <Configuration.h>
#include <kfc_fw_config.h>

namespace KFCConfigurationClasses {

    Plugins::RemoteControl::Config_t::Config_t() :
        auto_sleep_time(kDefaultValueFor_auto_sleep_time),
        deep_sleep_time(kDefaultValueFor_deep_sleep_time),
        click_time(kDefaultValueFor_click_time),
        hold_time(kDefaultValueFor_hold_time),
        hold_repeat_time(kDefaultValueFor_hold_repeat_time),
        udp_port(kDefaultValueFor_udp_port),
        actions{},
        combo{},
        events({{false}, {false}, {true}, {true}, {false}, {false}, {false}, {true}, {false}})
    {}


    void Plugins::RemoteControl::defaults()
    {
        setConfig(RemoteControlConfig::Config_t());
        if (kButtonCount == 2) {
            setName1(F("on"));
            setName2(F("off"));
        }
        else if (kButtonCount == 4) {
            setName1(F("on"));
            setName2(F("up"));
            setName3(F("down"));
            setName4(F("off"));
        }
        else {
            for(uint8_t i = 0; i < kButtonCount; i++) {
                setName(i, PrintString(F("btn%u"), i + 1).c_str());
            }
        }
        setEvent1(F("{button_name}-down"));
        setEvent2(F("{button_name}-up"));
        setEvent3(F("{button_name}-press"));
        setEvent4(F("{button_name}-long-press"));
        setEvent5(F("{button_name}-single-click"));
        setEvent6(F("{button_name}-double-click"));
        setEvent7(F("{button_name}-multi-{repeat}-click"));
        setEvent8(F("{button_name}-hold"));
        setEvent9(F("{button_name}-hold-release"));
    }

    void Plugins::RemoteControl::setName(uint8_t num, const char *name)
    {
        switch(num) {
            case 0:
                setName1(name);
                break;
            case 1:
                setName2(name);
                break;
            case 2:
                setName3(name);
                break;
            case 3:
                setName4(name);
                break;
            case 4:
                setName5(name);
                break;
            case 5:
                setName6(name);
                break;
            case 6:
                setName7(name);
                break;
            case 7:
                setName8(name);
                break;
            case 8:
                setName9(name);
                break;
            case 9:
                setName10(name);
                break;
        }
    }

    const char *Plugins::RemoteControl::getName(uint8_t num) {
        switch(num) {
            case 0:
                return getName1();
            case 1:
                return getName2();
            case 2:
                return getName3();
            case 3:
                return getName4();
            case 4:
                return getName5();
            case 5:
                return getName6();
            case 6:
                return getName7();
            case 7:
                return getName8();
            case 8:
                return getName9();
            case 9:
                return getName10();
            default:
                return nullptr;
        }
    }

    void Plugins::RemoteControl::setEventName(uint8_t num, const char *name)
    {
        switch(num) {
            case 0:
                setEvent1(name);
                break;
            case 1:
                setEvent2(name);
                break;
            case 2:
                setEvent3(name);
                break;
            case 3:
                setEvent4(name);
                break;
            case 4:
                setEvent5(name);
                break;
            case 5:
                setEvent6(name);
                break;
            case 6:
                setEvent7(name);
                break;
            case 7:
                setEvent8(name);
                break;
            case 8:
                setEvent9(name);
                break;
        }
    }

    const char *Plugins::RemoteControl::getEventName(uint8_t num) {
        switch(num) {
            case 0:
                return getEvent1();
            case 1:
                return getEvent2();
            case 2:
                return getEvent3();
            case 3:
                return getEvent4();
            case 4:
                return getEvent5();
            case 5:
                return getEvent6();
            case 6:
                return getEvent7();
            case 7:
                return getEvent8();
            case 8:
                return getEvent9();
            default:
                return nullptr;
        }
    }


}
