/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <functional>
#include <KFCRestApi.h>
#include "progmem_data.h"

#ifndef DEBUG_REMOTE_TIMEZONE
#define DEBUG_REMOTE_TIMEZONE 					0
#endif

typedef std::function<void(bool status, const String &message, time_t zoneEnd)> RemoteTimezoneStatusCallback_t;
namespace RemoteTimezone {

    class JsonReaderResult : public JsonVariableReader::Result {
    public:
        JsonReaderResult() : _status(false), _offset(0), _zoneEnd(0), _dst(false) {
        }

        virtual bool empty() const {
            return !_message.length() && !_zoneName.length();
        }

        virtual Result *create() {
            if (!empty()) {
                return new JsonReaderResult(std::move(*this));
            }
            return nullptr;
        }

        static void apply(JsonVariableReader::ElementGroup &group) {
            group.initResultType<JsonReaderResult>();
            group.add(FSPGM(status), [](Result &_result, JsonVariableReader::Reader &reader) {
                reinterpret_cast<JsonReaderResult &>(_result)._status = reader.getValueRef().equalsIgnoreCase(F("OK"));
                return true;
            });
            group.add(FSPGM(message), [](Result &_result, JsonVariableReader::Reader &reader) {
                reinterpret_cast<JsonReaderResult &>(_result)._message = reader.getValueRef();
                return true;
            });
            group.add(F("zoneName"), [](Result &_result, JsonVariableReader::Reader &reader) {
                reinterpret_cast<JsonReaderResult &>(_result)._zoneName = reader.getValueRef();
                return true;
            });
            group.add(F("abbreviation"), [](Result &_result, JsonVariableReader::Reader &reader) {
                reinterpret_cast<JsonReaderResult &>(_result)._abbreviation = reader.getValueRef();
                return true;
            });
            group.add(F("gmOffset"), [](Result &_result, JsonVariableReader::Reader &reader) {
                reinterpret_cast<JsonReaderResult &>(_result)._offset = reader.getIntValue();
                return true;
            });
            group.add(F("zoneEnd"), [](Result &_result, JsonVariableReader::Reader &reader) {
                reinterpret_cast<JsonReaderResult &>(_result)._zoneEnd = reader.getIntValue();
                return true;
            });
            group.add(F("dst"), [](Result &_result, JsonVariableReader::Reader &reader) {
                reinterpret_cast<JsonReaderResult &>(_result)._dst = reader.getIntValue();
                return true;
            });
        }

        bool getStatus() const {
            return _status;
        }

        const String &getMessage() const {
            return _message;
        }

        const String &getZoneName() const {
            return _zoneName;
        }

        const String &getAbbreviation() const {
            return _abbreviation;
        }

        int32_t getOffset() const {
            return _offset;
        }

        time_t getZoneEnd() const {
            return _zoneEnd;
        }

        bool getDst() const {
            return _dst;
        }

    private:
        bool _status;
        String _message;
        String _zoneName;
        String _abbreviation;
        int32_t _offset;
        time_t _zoneEnd;
        bool _dst;
    };

    class RestApi : public KFCRestAPI {
    public:
        typedef std::function<void(JsonReaderResult *result, const String &error)> Callback;

        RestApi() {
        }

        virtual void getRestUrl(String &url) const {
            url = _url;
            if (_zoneName.length()) {
                url.replace(F("${timezone}"), _zoneName);
            }
        }

        void setUrl(const String &url) {
            _url = url;
        }

        void setZoneName(const String &zoneName) {
            _zoneName = zoneName;
        }

        void call(Callback callback) {

            auto reader = new JsonVariableReader::Reader();
            auto result = reader->getElementGroups();
            result->emplace_back(JsonString());
            JsonReaderResult::apply(result->back());

            _createRestApiCall(emptyString, emptyString, reader, [callback](int16_t code, KFCRestAPI::HttpRequest &request) {
                if (code == 200) {
                    auto &group = request.getElementsGroup()->front();
                    auto &results = group.getResults<JsonReaderResult>();
                    if (results.size()) {
                        callback(results.front(), String());
                    }
                    else {
                        callback(nullptr, F("Invalid response"));
                    }
                }
                else {
                    callback(nullptr, request.getMessage());
                }
            });
        }

    private:
        String _url;
        String _zoneName;
    };

};
