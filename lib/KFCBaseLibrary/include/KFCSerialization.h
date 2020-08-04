/**
 * Author: sascha_lammers@gmx.de
 */

#include "Arduino_compat.h"

// Wrapper for serialization library with PROGMEM support

#define KFC_SERIALIZATION_NVP(member)                   kfc::serialization::make_nvp(PSTR(_STRINGIFY(member)), member)
#define KFC_SERIALIZATION_MAKE_NVP(name, member)        kfc::serialization::make_nvp(PSTR(name), member)
#define KFC_SERIALIZATION_MAKE_NVP_F(name, member)      kfc::serialization::make_nvp(name, member)
#define KFC_SERIALIZATION_PSTR(str)                     reinterpret_cast<const char *>(str)

#define KFC_SERIALIZATION_CLASS_VERSION(name, version)  static constexpr kfc::serialization::version kClassVersion_#name = version;

namespace kfc {

    namespace serialization {

        using version = uint8_t; //const unsigned int;

        template<typename T>
        class NameValuePair {
        public:
            NameValuePair(const char *name, T &&value) : _name(FPSTR(name)), _value(std::forward<T>(value)) {}
            NameValuePair(String &&name, T &&value) : _name(std::move(name)), _value(std::forward<T>(value)) {}

            const char *getNameCStr() const {
                return _name.c_str();
            }
            const String &getName() const {
                return _name;
            }

            template<typename Archive>
            void serialize(Archive &ar, kfc::serialization::version version) {
                ar.serialize(_name, std::forward<decltype(_value)>(_value), version);
            }

        private:
            String _name;
            T &&_value;
        };

        template<typename T>
        NameValuePair<T> make_nvp(const char *name, T &&value) {
            return { KFC_SERIALIZATION_PSTR(name), std::forward<T>(value) };
        }

        template<typename T>
        NameValuePair<T> make_nvp(const __FlashStringHelper *name, T &&value) {
            return { KFC_SERIALIZATION_PSTR(name), std::forward<T>(value) };
        }

    }

}


/*

Examples

template<typename Archive>
void serialize(Archive & ar, kfc::serialization::version version) {
    ar & anonymous_member;
    ar & KFC_SERIALIZATION_NVP(member_with_name);
    ar & KFC_SERIALIZATION_MAKE_NVP("custom_member", member42);
}

template<typename Archive>
void load(Archive & ar, kfc::serialization::version version) {
    ar & anonymous_member;
    ar & KFC_SERIALIZATION_NVP(member_with_name);
    ar & KFC_SERIALIZATION_MAKE_NVP("custom_member", member42);
}

template<typename Archive>
void save(Archive & ar, kfc::serialization::version version) const {
    ar & anonymous_member;
    ar & KFC_SERIALIZATION_NVP(member_with_name);
    ar & KFC_SERIALIZATION_MAKE_NVP("custom_member", member42);
}

*/
