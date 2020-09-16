/**
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

#ifndef KFC_FORMS_NO_DIRECT_COPY
// if set to 0, copying data into vectors is done with reserve, resize and memcpy instead of reserve and push_back
#define KFC_FORMS_NO_DIRECT_COPY                        1
#endif

#if KFC_FORMS_INCLUDE_HPP_AS_INLINE
#define __KFC_FORMS_INLINE_METHOD__                     inline
#define __inlined_hpp__                                 inline
#else
#define __KFC_FORMS_INLINE_METHOD__
#define __inlined_hpp__
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

        namespace Value {

            enum class Type : uint8_t {
                NONE = 0,
                BOOLEAN,
                INTEGRAL,
                FLOATING_POINT,
                STRING,
                MAX
            };

        }

        using ValueType = Value::Type;

    }

    namespace WebUI {

        class BaseUI;
        class Config;

        enum class Type : uint8_t {
            NONE,
            SELECT,
            TEXT,
            NUMBER,
            INTEGER,
            FLOAT,
            RANGE,
            RANGE_SLIDER,
            PASSWORD,
            NEW_PASSWORD,
            // VISIBLE_PASSWORD,
            GROUP_START,
            GROUP_END,
            GROUP_START_DIV,
            GROUP_END_DIV,
            GROUP_START_HR,
            GROUP_END_HR,
            GROUP_START_CARD,
            GROUP_END_CARD,
            HIDDEN,
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
            ATTRIBUTE_MIN_MAX,
            SUFFIX_TEXT,
            SUFFIX_HTML,
            OPTION,
            OPTION_NUM_KEY,
            MAX,
            MAX_COUNT = 16
        };
        static_assert(Type::MAX < Type::MAX_COUNT, "Type exceeds 4 bit");

        static constexpr size_t kTypeMax = (size_t)Type::MAX;
        static constexpr size_t kTypeMaxCount = (size_t)Type::MAX_COUNT;

    }

    inline namespace HostnameValidator {

        enum class AllowedType : uint8_t {
            NONE = 0,
            EMPTY = 0x01,
            ZEROCONF = 0x02,
            IPADDRESS = 0x04,
            HOSTNAME = 0x08,
            HOST_OR_IP = HOSTNAME | IPADDRESS,
            EMPTY_AND_ZEROCONF = EMPTY | ZEROCONF,
        };

    }

}
