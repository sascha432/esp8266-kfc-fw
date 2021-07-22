/**
 * Author: sascha_lammers@gmx.de
 */

#include "kfc_fw_config/base.h"
#include "ConfigurationHelper.h"

        // --------------------------------------------------------------------
        // IOT Switch

        #define IOT_SWITCH_NO_DECL
        #include "../src/plugins/switch/switch_def.h"
        #undef IOT_SWITCH_NO_DECL

        class IOTSwitch {
        public:
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
            struct __attribute__packed__ SwitchConfig {
                using Type = SwitchConfig;

                struct __attribute__packed__ DataType {
                    using Type = DataType;

                    CREATE_UINT8_BITFIELD_MIN_MAX(length, 8, 0, 31, 0, 1);
                    CREATE_ENUM_D_BITFIELD(state, StateEnum, StateEnum::OFF);
                    CREATE_ENUM_D_BITFIELD(webUI, WebUIEnum, WebUIEnum::NONE);

                    DataType() : length(kDefaultValueFor_length), state(kDefaultValueFor_state), webUI(kDefaultValueFor_webUI) {}
                    DataType(const String &_name, StateEnum _state, WebUIEnum _webUI) : length(_name.length()), state(static_cast<uint8_t>(_state)), webUI(static_cast<uint8_t>(_webUI)) {}

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

                SwitchConfig &operator =(const String &name) {
                    _data.length = name.length();
                    return *this;
                }

                SwitchConfig &operator =(StateEnum state) {
                    _data.state = static_cast<uint8_t>(state);
                    return *this;
                }

                SwitchConfig &operator =(WebUIEnum webUI) {
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

                SwitchConfig() : _data() {}
                SwitchConfig(const String &name, StateEnum state, WebUIEnum webUI) : _data(name, state, webUI) {}
            };

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
                    while(ptr + SwitchConfig::size() <= endPtr && i < names.size()) {
                        memcpy(&configs[i].data(), ptr, configs[i].size());
                        // ::printf("ptr=%p endPtr=%p i=%u sz=%u namelen=%u\n", ptr, endPtr, i, names.size(), configs[i].getNameLength());
                        ptr += configs[i].size();
                        auto len = configs[i].getNameLength();
                        if (ptr + len < endPtr) {
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
        };
