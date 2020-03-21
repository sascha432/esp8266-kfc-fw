/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

namespace WeatherStation {

    class RestAPI : public KFCRestAPI {
    public:
        typedef std::function<void(bool status, const String &error)> Callback;

    public:
        RestAPI(const String &url) : _url(url) {
        }

        virtual void getRestUrl(String &url) const {
            url = _url;
        }

        void call(JsonBaseReader *reader, int timeout, Callback callback) {
            debug_printf_P(PSTR("timeout=%u\n"), timeout);

            _timeout = timeout;

            _createRestApiCall(emptyString, emptyString, reader, [callback](int16_t code, KFCRestAPI::HttpRequest &request) {
                if (code == 200) {
                    callback(true, String());
                    //callback(false, F("Invalid response"));
                }
                else {
                    callback(false, request.getMessage());
                }
            });
        }

    private:
        String _url;
    };

};
