class PrintString : public String, public Print {
   public:
    PrintString() : String(), Print() {
    }
    PrintString(const char *format, ...) __attribute__((format(printf, 2, 3))) : String(), Print() {
        va_list arg;
        va_start(arg, format);
        printf(format, arg);
        va_end(arg);
    }
    PrintString(const char *format, va_list arg) : String(), Print() {
        printf(format, arg);
    }
    PrintString(const __FlashStringHelper *format, ...) : String(), Print() {
        va_list arg;
        va_start(arg, format);
        printf_P(reinterpret_cast<PGM_P>(format), arg);
        va_end(arg);
    }
    PrintString(const __FlashStringHelper *format, va_list arg) : String(), Print() {
        printf_P(reinterpret_cast<PGM_P>(format), arg);
    }

    virtual size_t write(uint8_t data) override {
        return concat((char)data) ? 1 : 0;
    }
    virtual size_t write(const uint8_t *buffer, size_t size) override {
        return concat((const char *)buffer, size);
    }
};
