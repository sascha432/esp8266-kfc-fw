/*
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <vector>

#ifndef KFC_FORMS_HAVE_VALIDATE_CALLBACK
#define KFC_FORMS_HAVE_VALIDATE_CALLBACK                0
#endif

#ifndef KFC_FORMS_INCLUDE_HPP_AS_INLINE
#define KFC_FORMS_INCLUDE_HPP_AS_INLINE                 1
#endif

#if KFC_FORMS_INCLUDE_HPP_AS_INLINE
#define __KFC_FORMS_INLINE_METHOD__                     inline
#else
#define __KFC_FORMS_INLINE_METHOD__
#endif


#pragma push_macro("DEFAULT")
#undef DEFAULT

#include <stdint.h>

class PrintArgs;
class StringDepulicator;
class PrintHtmlEntitiesString;

namespace FormUI {

    using PrintInterface = PrintArgs;

    namespace Form {

        class BaseForm;

        class Data;
        class Error;
        using ErrorVector = ::std::vector<Error>;

    }

    namespace Validator {

        class BaseValidator;

        using Vector = ::std::vector<BaseValidator *>;
        using Callback = ::std::function<bool(Form::BaseForm &form)>;

    }

    namespace Field {

        class BaseField;

        using Ptr = ::std::unique_ptr<BaseField>;
        using Vector = ::std::vector<Ptr>;

        enum class Type : uint8_t {
            NONE = 0,
            CHECK,
            SELECT,
            TEXT,
            GROUP,
            TEXTAREA,
            MAX
        };

        // namespace Value {

        //     enum class Type : uint8_t {
        //         NONE = 0,
        //         BOOLEAN,
        //         INTEGRAL,
        //         FLOATING_POINT,
        //         STRING,
        //         MAX
        //     };

        // }
        // //using ValueType = Value::Type;

    }

    class Group;

    namespace WebUI {

        class BaseUI;
        class Config;

        enum class Type : uint8_t {
            NONE,
            SELECT,
            TEXT,
            TEXTAREA,
            NUMBER,
            NUMBER_RANGE,
            RANGE,
            RANGE_SLIDER,
            PASSWORD,
            NEW_PASSWORD,
            // VISIBLE_PASSWORD,
            CHECKBOX,

            // (start group) >= BEGIN_GROUPS and < END_GROUPS
            // (end group = start group + 1 ) > BEGIN_GROUPS and <= END_GROUPS
            BEGIN_GROUPS,
            GROUP_START = BEGIN_GROUPS,
            GROUP_END,
            GROUP_START_DIV,
            GROUP_END_DIV,
            GROUP_START_HR,
            GROUP_END_HR,
            GROUP_START_CARD,
            GROUP_END_CARD,
            END_GROUPS = GROUP_END_CARD,

            HIDDEN,
            HIDDEN_SELECT,
        };

        enum class StyleType : uint8_t {
            DEFAULT = 0,
            ACCORDION,
            MAX
        };

    }

    namespace Storage {

        class Vector;

        namespace Value {

            class Label;
            using String = Label;

        }

        using VectorBase = std::vector<uint8_t>;
        using Iterator = VectorBase::iterator;
        using ConstIterator = VectorBase::const_iterator;
        using ValueStringVector = std::vector<Value::String>;

        enum class Type : uint8_t {
            LABEL = 0,
            MIN = LABEL,
            LABEL_RAW,
            ATTRIBUTE,
            ATTRIBUTE_INT,
            ATTRIBUTE_MIN_MAX,
            SUFFIX_TEXT,
            SUFFIX_HTML,
            SUFFIX_OPTION,
            SUFFIX_OPTION_NUM_KEY,
            OPTION,
            OPTION_NUM_KEY,
            MAX,
            MAX_COUNT = 16
        };
        static_assert(Type::MAX < Type::MAX_COUNT, "Type exceeds 4 bit");

        static constexpr size_t kTypeMax = (size_t)Type::MAX;
        static constexpr size_t kTypeMaxCount = (size_t)Type::MAX_COUNT;

    }

    inline namespace Container {

        class Mixed;
        class MixedString;
        class List;
        class CheckboxButtonSuffix;
        class ZeroconfSuffix;

    }

    inline namespace HostnameValidator {

        enum class AllowedType : uint8_t {
            NONE = 0,
            EMPTY = 0x01,
            ZEROCONF = 0x02,
            IPADDRESS = 0x04,
            HOSTNAME = 0x08,
            HOST_OR_IP = HOSTNAME | IPADDRESS,
            HOST_OR_IP_OR_EMPTY = HOSTNAME | IPADDRESS | EMPTY,
            EMPTY_AND_ZEROCONF = EMPTY | ZEROCONF,
            HOST_OR_IP_OR_ZEROCONF = HOST_OR_IP | ZEROCONF,
            HOST_OR_IP_OR_ZEROCONF_OR_EMPTY = HOST_OR_IP | ZEROCONF | EMPTY,
        };

    }

    using InputFieldType = Field::Type;
    // using FieldValueType = Field::Value::Type;
    using RenderType = WebUI::Type;
    using FormStyleType = WebUI::StyleType;
    using HostnameAllowedType = HostnameValidator::AllowedType;

}
