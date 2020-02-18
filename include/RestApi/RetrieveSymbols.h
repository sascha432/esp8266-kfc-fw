/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include "KFCRestApi.h"

namespace RetrieveSymbols {

    class JsonReaderResult : public JsonVariableReader::Result {
    public:
        class Element {
        public:
            Element() : _srcAddress(0), _address(0), _size(0) {
            }
            void setSrcAddress(uint32_t srcAddress) {
                _srcAddress = srcAddress;
            }
            void setAddress(uint32_t address) {
                _address = address;
            }
            void setName(const String &name) {
                _name = name;
            }
            void setSize(size_t size) {
                _size = size;
            }
            bool operator==(uint32_t address) const {
                return _address == address;
            }
            uint32_t getSrcAddress() const {
                return _srcAddress;
            }
            uint32_t getAddress() const {
                return _address;
            }
            const String &getName() const {
                return _name;
            }
            size_t getSize() const {
                return _size;
            }
        private:
            uint32_t _srcAddress;
            uint32_t _address;
            String _name;
            size_t _size;
        };
        typedef std::vector<Element> ElementsVector;

        JsonReaderResult() {
        }

        virtual bool empty() const {
            return _error.length() == 0 && _items.empty();
        }

        virtual Result *create() {
            if (!empty()) {
                return new JsonReaderResult(std::move(*this));
            }
            return nullptr;
        }

        static void apply(JsonVariableReader::ElementGroup& group) {
            group.initResultType<JsonReaderResult>();
            group.add(F("[].a"), [](Result &_result, JsonVariableReader::Reader &reader) {
                auto value = strtoul(reader.getValueRef().c_str(), nullptr, 0);
                reinterpret_cast<JsonReaderResult &>(_result)._resizeItems(reader.getObjectIndex()).setAddress(value);
                return true;
            });
            group.add(F("[].m"), [](Result &_result, JsonVariableReader::Reader &reader) {
                auto value = strtoul(reader.getValueRef().c_str(), nullptr, 0);
                reinterpret_cast<JsonReaderResult &>(_result)._resizeItems(reader.getObjectIndex()).setSrcAddress(value);
                return true;
            });
            group.add(F("[].n"), [](Result &_result, JsonVariableReader::Reader &reader) {
                reinterpret_cast<JsonReaderResult &>(_result)._resizeItems(reader.getObjectIndex()).setName(reader.getValueRef());
                return true;
            });
            group.add(F("[].l"), [](Result &_result, JsonVariableReader::Reader &reader) {
                reinterpret_cast<JsonReaderResult &>(_result)._resizeItems(reader.getObjectIndex()).setSize(reader.getIntValue());
                return true;
            });
            group.add(F("e"), [](Result &_result, JsonVariableReader::Reader &reader) {
                reinterpret_cast<JsonReaderResult &>(_result)._error = reader.getValueRef();
                return true;
            });
        }

        const ElementsVector &getItems() const {
            return _items;
        }

        const String &getError() const {
            return _error;
        }

    private:
        Element &_resizeItems(size_t size) {
            while (size >= _items.size()) {
                _items.emplace_back();
            }
            return _items.at(size);
        }

    private:
        String _error;
        ElementsVector _items;
    };

    #include "../build_id.h"

    class RestApi : public KFCRestAPI {
    public:
        typedef std::function<void(JsonReaderResult *result, const String &error)> Callback;

        RestApi() {
            _file = PrintString(F("bid%08x_firmware.elf"), atoi(__BUILD_ID)-1);
        }

        void setAddresses(StringVector &&addresses) {
            _addresses = std::move(addresses);
        }
        void setNames(StringVector &&names) {
            _names = std::move(names);
        }

        virtual void getRestUrl(String &url) const {
            url = F("http://192.168.0.3/kfcfw/dump_section.php?file=");
            url += _file.c_str();
            if (_names.size()) {
                url += F("&name=");
                url += implode(',', _names);
            }
            if (_addresses.size()) {
                url += F("&addr=");
                url += implode(',', _addresses);
            }
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
                        auto result = results.front();
                        if (result->getError().length()) {
                            callback(nullptr, PrintString(F("error retrieving symbols: %s"), result->getError().c_str()));
                        }
                        else {
                            callback(result, String());
                        }
                    }
                    else {
                        callback(nullptr, F("error retrieving symbols: invalid response"));
                    }
                } else {
                    callback(nullptr, request.getMessage());
                }
            });
        }

    private:
        String _file;
        StringVector _names;
        StringVector _addresses;
    };

};
