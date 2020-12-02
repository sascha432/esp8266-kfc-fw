
/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#if HOME_ASSISTANT_INTEGRATION == 0
#error HOME_ASSISTANT_INTEGRATION not set
#endif

#include <KFCJson.h>

namespace HassJsonReader {

    class GetState : public JsonVariableReader::Result {
    public:
        GetState() : _brightness(0) {
        }

        virtual bool empty() const {
            return !_entitiyId.length() || !_state.length();
        }

        virtual Result *create() {
            if (!empty()) {
                return new GetState(std::move(*this));
            }
            return nullptr;
        }

        static void apply(JsonVariableReader::ElementGroup &group) {
            group.initResultType<GetState>();
            group.add(F("entity_id"), [](Result &result, JsonVariableReader::Reader &reader) {
                reinterpret_cast<GetState &>(result)._entitiyId = reader.getValueRef();
                return true;
            });
            group.add(F("state"), [](Result &result, JsonVariableReader::Reader &reader) {
                reinterpret_cast<GetState &>(result)._state = reader.getValueRef();
                return true;
            });
            group.add(F("attributes.brightness"), [](Result &result, JsonVariableReader::Reader &reader) {
                reinterpret_cast<GetState &>(result)._brightness = reader.getIntValue();
                return true;
            });
        }

        uint32_t getBrightness() const {
            return _brightness;
        }

        //private:
        String _entitiyId;
        String _state;
        uint32_t _brightness;
    };


    class CallService : public JsonVariableReader::Result {
    public:
        CallService() {
        }

        virtual bool empty() const {
            return false;
        }

        virtual Result *create() {
            return new CallService();
        }

        static void apply(JsonVariableReader::ElementGroup &group) {
            group.initResultType<CallService>();
        }
    };

};

