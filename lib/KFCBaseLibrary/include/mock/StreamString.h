#if defined(_WIN32) || defined(_WIN64)


#pragma once

class StreamString: public Stream, public String {
public:
    using String::String;
    using String::operator=;

    size_t write(const uint8_t *buffer, size_t size) override;
    size_t write(uint8_t data) override;

    int available() override;
    int read() override;
    int peek() override;
    void flush() override;
};

#endif
