/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#ifdef HAVE_KFC_FIRMWARE_VERSION
#include <ESPAsyncWebServer.h>
#else
#include <map>
#endif

namespace FormUI {

    namespace Form {

#ifdef HAVE_KFC_FIRMWARE_VERSION

        class Data : public AsyncWebServerRequest {
        public:
            using AsyncWebServerRequest::AsyncWebServerRequest;
            bool hasArg(const String &name) const {
            return AsyncWebServerRequest::hasArg(name.c_str());
            }
        };

#else

        using DataMap = std::map<String, String>;

        class Data {
        public:
            Data();
            virtual ~Data();

            void clear();

            const String arg(const String &name) const;
            bool hasArg(const String &name) const;

            void setValue(const String &name, const String &value);

        private:
            DataMap _data;
        };

#endif

    }

}
