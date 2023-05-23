/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <LoopFunctions.h>
#include <fs_mapping.h>

namespace StreamOutput {

    class Cat {
    public:
        using Callback = std::function<void(Cat *cat)>;

        static constexpr uint8_t kPrintInfo = 0x01;             // print filename and size
        static constexpr uint8_t kPrintBinary = 0x02;           // write input directly, otherwise non printable characters are displayed as hex codes
        static constexpr uint8_t kPrintCrLfAsText = 0x04;       // not in combination with kPrintBinary

        // dump file to stream
        //
        // returns the pointer to the object or nullptr in case of an error
        // the callback is executed when EOF has been reached, unless an error occured before
        // do not use the pointer returned by dump() except to identify the object
        //
        // void *ptr1 = nullptr;
        // void *ptr2 = nullptr;
        // void callback(Cat *cat) {
        //     if (cat == ptr1) {
        //         Serial.printf("dump1 done: file size %u file name %s\n", cat->getSize(), cat->getFilename().c_str());
        //     }
        //     else if (cat == ptr2)
        //         Serial.println("dump2 done");
        //     }
        // }
        // ptr1 = dump("/test", Serial, 0, callback);
        // ptr2 = dump("/test", Serial1, 0, callback);
        static const void *dump(const String &filename, Stream &output, uint8_t flags = 0, Callback callback = nullptr, size_t bufferSize = 256) {
            // object deletes itself
            auto cat = new Cat(filename, output, flags, bufferSize, callback);
            if (cat->_hasError) { // unless an error occured
                delete cat;
                return nullptr;
            }
            return cat;
        }

        static const void *dumpBinary(const String &filename, Stream &output, uint8_t flags = 0, Callback callback = nullptr, size_t bufferSize = 256) {
            return dump(filename, output, flags | kPrintBinary, callback, bufferSize);
        }

        operator const void *() const {
            return this;
        }

        Stream &getOutput() {
            return _output;
        }

        File &getFile() {
            return _file;
        }

        String getFilename() {
            if (!_file) {
                return String();
            }
            return _file.fullName();
        }

        size_t getSize() {
            return _file.size();
        }

    protected:
        Cat(const String &filename, Stream &output, uint8_t flags, size_t bufferSize, Callback callback) :
            _callback(callback),
            _output(output),
            _buffer(nullptr),
            _bufferSize(bufferSize),
            _printInfo(flags & kPrintInfo),
            _printCrLfAsText(flags & kPrintCrLfAsText),
            _printBinary(flags & kPrintBinary),
            _loopAdded(false),
            _hasError(false)
        {
            _buffer = new uint8_t[_bufferSize];
            if (_buffer) {
                _file = FSWrapper::open(filename, fs::FileOpenMode::read);
            }
            if (_buffer && _file) {
                if (_printInfo) {
                    output.printf_P(PSTR("+CAT: file=%s:\n"), _file.fullName());
                }
                _loopAdded = true;
                LOOP_FUNCTION_ADD_ARG([this]() {
                    loop();
                }, this);
            }
            else {
                if (_printInfo) {
                    output.printf_P(PSTR("+CAT: failed to open file=%s\n"), filename.c_str());
                }
                _hasError = true;
            }
        }
        ~Cat() {
            if (_loopAdded) {
                LoopFunctions::remove(this);
                _loopAdded = false;
            }
            if (_buffer) {
                delete[] _buffer;
                _buffer = nullptr;
            }
        }

        void loop() {
            if (_file) {
                if (_file.available()) {
                    auto len = _file.readBytes(reinterpret_cast<char *>(_buffer), _bufferSize - 1);
                    if (len > 0) {
                        if (_printBinary) {
                            _output.write(_buffer, len);
                        } else {
                            _buffer[len] = 0;
                            if (_printCrLfAsText) {
                                printable_string(_output, _buffer, len, 0, nullptr, true);
                            }
                            else {
                                printable_string(_output, _buffer, len, 0, PSTR("\r\n"));
                            }
                        }
                    }
                }
                if (!_file.available()) {
                    if (_printInfo) {
                        Serial.printf_P(PSTR("\n+CAT: ---<EOF>--- size=%u\n"), (unsigned)_file.size());
                    }
                    if (_callback) {
                        _callback(this);
                    }
                    delete this;
                }
            }
            else {
                if (_callback) {
                    _callback(this);
                }
                delete this;
            }
        }

    private:
        Callback _callback;
        File _file;
        Stream &_output;
        uint8_t *_buffer;
        size_t _bufferSize;
        uint8_t _printInfo: 1;
        uint8_t _printCrLfAsText: 1;
        uint8_t _printBinary: 1;
        uint8_t _loopAdded: 1;
        uint8_t _hasError: 1;
    };

};
