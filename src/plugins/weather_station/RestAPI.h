/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <KFCRestApi.h>

namespace WeatherStation {

    class RestAPI : public KFCRestAPI {
    public:
        // typedef std::function<void(bool status, const String &error)> Callback;

    public:
        RestAPI(const String &url) : _url(url) {
        }

        virtual void getRestUrl(String &url) const {
            url = _url;
        }

        virtual void autoDelete(void *restApiPtr) override {
            __LDBG_printf("executing auto delete api=%p", restApiPtr);
            delete reinterpret_cast<RestAPI *>(restApiPtr);
        }

        void call(JsonBaseReader *reader, int timeout, HttpRequest::Callback_t callback) {
            _timeout = timeout;
            __DBG_printf("callback=%p timeout=%u", &callback, _timeout);
            _createRestApiCall(emptyString, emptyString, reader, callback);
            //  [callback](int16_t code, KFCRestAPI::HttpRequest &request) {
            //     if (code == 200) {
            //         callback(true, emptyString);
            //     }
            //     else {
            //         callback(false, request.getMessage());
            //     }
            // });
        }

    private:
        String _url;
    };

};
