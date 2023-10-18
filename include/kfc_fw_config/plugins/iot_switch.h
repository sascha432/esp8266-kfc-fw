/**
 * Author: sascha_lammers@gmx.de
 */

#include "kfc_fw_config/base.h"
#include "ConfigurationHelper.h"

#define IOT_SWITCH_NO_DECL
#include "../src/plugins/switch/switch_def.h"
#undef IOT_SWITCH_NO_DECL

namespace KFCConfigurationClasses {

    namespace Plugins {

        // --------------------------------------------------------------------
        // IOT Switch

        namespace SwitchConfigNS {

            enum class StateEnum : uint8_t {
                OFF =       0x00,
                ON =        0x01,
                RESTORE =   0x02,
                MAX
            };

            enum class WebUIEnum : uint8_t {
                NONE =      0x00,
                HIDE =      0x01,
                NEW_ROW =   0x02,
                TOP =       0x03,
                MAX
            };

            struct __attribute__packed__ SwitchConfigType {
                using Type = SwitchConfigType;

                struct __attribute__packed__ DataType {
                    using Type = DataType;

                    CREATE_UINT8_BITFIELD_MIN_MAX(length, 8, 0, 31, 0, 1);
                    CREATE_ENUM_D_BITFIELD(state, StateEnum, StateEnum::OFF);
                    CREATE_ENUM_D_BITFIELD(webUI, WebUIEnum, WebUIEnum::NONE);

                    DataType() :
                        length(kDefaultValueFor_length),
                        state(kDefaultValueFor_state),
                        webUI(kDefaultValueFor_webUI)
                    {
                    }
                    DataType(const String &_name, StateEnum _state, WebUIEnum _webUI) :
                        length(_name.length()),
                        state(static_cast<uint8_t>(_state)),
                        webUI(static_cast<uint8_t>(_webUI))
                    {
                    }

                } _data;

                void setLength(size_t length) {
                    _data.length = length;
                }

                size_t getNameLength() const {
                    return _data.length;
                }

                // size_t getLength() const {
                //     return getNameLength();
                // }

                SwitchConfigType &operator =(const String &name) {
                    _data.length = name.length();
                    return *this;
                }

                SwitchConfigType &operator =(StateEnum state) {
                    _data.state = static_cast<uint8_t>(state);
                    return *this;
                }

                SwitchConfigType &operator =(WebUIEnum webUI) {
                    _data.webUI = static_cast<uint8_t>(webUI);
                    return *this;
                }

                // void setState(StateEnum state) {
                //     _data.state = static_cast<uint8_t>(state);
                // }

                StateEnum getState() const {
                    return static_cast<StateEnum>(_data.state);
                }

                // void setWebUI(WebUIEnum webUI) {
                //     _data.webUI = static_cast<uint8_t>(webUI);
                // }

                WebUIEnum getWebUI() const {
                    return static_cast<WebUIEnum>(_data.webUI);
                }

                WebUINS::NamePositionType getWebUINamePosition() const {
                    switch(static_cast<WebUIEnum>(_data.webUI)) {
                        case WebUIEnum::TOP:
                        case WebUIEnum::NEW_ROW:
                            return WebUINS::NamePositionType::TOP;
                        default:
                            break;
                    }
                    return WebUINS::NamePositionType::SHOW;
                }

                DataType data() const {
                    return _data;
                }

                DataType &data() {
                    return _data;
                }

                static constexpr size_t size() {
                    return sizeof(DataType);
                }

                SwitchConfigType() : _data() {}
                SwitchConfigType(const String &name, StateEnum state, WebUIEnum webUI) : _data(name, state, webUI) {}
            };


            class IotSwitch {
            public:
                static const uint8_t *getConfig(uint16_t &length);
                static void setConfig(const uint8_t *buf, size_t size);

                // T = std::array<String, N>, R = std::array<SwitchConfig, N>
                template <class _List, class _Array>
                static void getConfig(_List &names, _Array &configs) {
                    names = _List();
                    configs = _Array();
                    uint16_t length = 0;
                    auto ptr = getConfig(length);
                    #if DEBUG_IOT_SWITCH
                        __dump_binary_to(DEBUG_OUTPUT, ptr, length, length, PSTR("getConfig"));
                    #endif
                    if (ptr) {
                        uint8_t i = 0;
                        auto endPtr = ptr + length;
                        while(ptr + SwitchConfigType::size() <= endPtr && i < names.size()) {
                            memmove(&configs[i].data(), ptr, configs[i].size());
                            ptr += configs[i].size();
                            auto len = configs[i].getNameLength();
                            // ::printf("n=%u ptr=%p endPtr=%p i=%u sz=%u namelen=%u err=%d\n", i, ptr, endPtr, i, names.size(), len, !(ptr + len <= endPtr));
                            if (ptr + len <= endPtr) {
                                names[i] = PrintString(ptr, len);
                            }
                            else {
                                // invalid data, remove name
                                names[i] = String();
                                configs[i] = names[i];
                            }
                            ptr += len;
                            i++;
                        }
                    }
                }

                template <class _List, class _Array>
                static void setConfig(const _List &names, _Array &configs) {
                    Buffer buffer;
                    for(uint8_t i = 0; i < names.size(); i++) {
                        auto name = String(names[i]);
                        name.rtrim();
                        configs[i] = name;
                        buffer.push_back(configs[i].data());
                        buffer.writeString(name);
                    }
                    setConfig(buffer.begin(), buffer.length());
                    #if DEBUG_IOT_SWITCH
                        __dump_binary_to(DEBUG_OUTPUT, buffer.begin(), buffer.length(), buffer.length(), PSTR("setConfig"));
                    #endif
                }

                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.iotswitch, Icon0, 0, 64);
                #if IOT_SWITCH_CHANNEL_NUM >= 2
                    CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.iotswitch, Icon1, 0, 64);
                #endif
                #if IOT_SWITCH_CHANNEL_NUM >= 3
                    CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.iotswitch, Icon2, 0, 64);
                #endif
                #if IOT_SWITCH_CHANNEL_NUM >= 4
                    CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.iotswitch, Icon3, 0, 64);
                #endif
                #if IOT_SWITCH_CHANNEL_NUM >= 5
                    CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.iotswitch, Icon4, 0, 64);
                #endif
                #if IOT_SWITCH_CHANNEL_NUM >= 6
                    CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.iotswitch, Icon5, 0, 64);
                #endif
                #if IOT_SWITCH_CHANNEL_NUM >= 7
                    CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.iotswitch, Icon6, 0, 64);
                #endif
                #if IOT_SWITCH_CHANNEL_NUM >= 8
                    CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.iotswitch, Icon7, 0, 64);
                #endif
                #if IOT_SWITCH_CHANNEL_NUM >= 9
                    CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.iotswitch, Icon8, 0, 64);
                #endif
                #if IOT_SWITCH_CHANNEL_NUM >= 10
                    CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.iotswitch, Icon9, 0, 64);
                #endif
                #if IOT_SWITCH_CHANNEL_NUM >= 11
                    CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.iotswitch, Icon10, 0, 64);
                #endif
                #if IOT_SWITCH_CHANNEL_NUM >= 12
                    CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.iotswitch, Icon11, 0, 64);
                #endif

                static void setIcon(uint8_t num, const char *icon) {
                    // check if the icon is set
                    if (!icon || !*icon) {
                        // get prev. icon
                        auto prevIcon = getIcon(num);
                        if (!prevIcon || !*prevIcon) {
                            // if empty or not set, return to save memory
                            // currently there is no method to delete a string
                            return;
                        }
                    }
                    switch(num) {
                        case 0:
                            setIcon0(icon);
                            break;
                        #if IOT_SWITCH_CHANNEL_NUM >= 2
                            case 1:
                                setIcon1(icon);
                                break;
                        #endif
                        #if IOT_SWITCH_CHANNEL_NUM >= 3
                            case 2:
                                setIcon2(icon);
                                break;
                        #endif
                        #if IOT_SWITCH_CHANNEL_NUM >= 4
                            case 3:
                                setIcon3(icon);
                                break;
                        #endif
                        #if IOT_SWITCH_CHANNEL_NUM >= 5
                            case 4:
                                setIcon4(icon);
                                break;
                        #endif
                        #if IOT_SWITCH_CHANNEL_NUM >= 6
                            case 5:
                                setIcon5(icon);
                                break;
                        #endif
                        #if IOT_SWITCH_CHANNEL_NUM >= 7
                            case 6:
                                setIcon6(icon);
                                break;
                        #endif
                        #if IOT_SWITCH_CHANNEL_NUM >= 8
                            case 7:
                                setIcon7(icon);
                                break;
                        #endif
                        #if IOT_SWITCH_CHANNEL_NUM >= 9
                            case 8:
                                setIcon8(icon);
                                break;
                        #endif
                        #if IOT_SWITCH_CHANNEL_NUM >= 10
                            case 9:
                                setIcon9(icon);
                                break;
                        #endif
                        #if IOT_SWITCH_CHANNEL_NUM >= 11
                            case 10:
                                setIcon10(icon);
                                break;
                        #endif
                        #if IOT_SWITCH_CHANNEL_NUM >= 12
                            case 11:
                                setIcon11();
                        #endif
                        default:
                            __DBG_panic("invalid icon #%u", num);
                            break;
                    }
                }

                static const char *getIcon(uint8_t num) {
                    switch(num) {
                        case 0:
                            return getIcon0();
                        #if IOT_SWITCH_CHANNEL_NUM >= 2
                            case 1:
                                return getIcon1();
                        #endif
                        #if IOT_SWITCH_CHANNEL_NUM >= 3
                            case 2:
                                return getIcon2();
                        #endif
                        #if IOT_SWITCH_CHANNEL_NUM >= 4
                            case 3:
                                return getIcon3();
                        #endif
                        #if IOT_SWITCH_CHANNEL_NUM >= 5
                            case 4:
                                return getIcon4();
                        #endif
                        #if IOT_SWITCH_CHANNEL_NUM >= 6
                            case 5:
                                return getIcon5();
                        #endif
                        #if IOT_SWITCH_CHANNEL_NUM >= 7
                            case 6:
                                return getIcon6();
                        #endif
                        #if IOT_SWITCH_CHANNEL_NUM >= 8
                            case 7:
                                return getIcon7();
                        #endif
                        #if IOT_SWITCH_CHANNEL_NUM >= 9
                            case 8:
                                return getIcon8();
                        #endif
                        #if IOT_SWITCH_CHANNEL_NUM >= 10
                            case 9:
                                return getIcon9();
                        #endif
                        #if IOT_SWITCH_CHANNEL_NUM >= 11
                            case 10:
                                return getIcon10();
                        #endif
                        #if IOT_SWITCH_CHANNEL_NUM >= 12
                            case 11:
                                return getIcon11();
                        #endif
                    }
                    __DBG_panic("invalid icon #%u", num);
                    return nullptr;
                }
            };

        }
    }
}
