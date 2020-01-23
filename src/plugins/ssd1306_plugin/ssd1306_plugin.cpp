/**
 * Author: sascha_lammers@gmx.de
 */

#if SSD1306_PLUGIN

#include "ssd1306_plugin.h"
#include <Adafruit_GFX.h>
#include <WiFiCallbacks.h>
#include <Timezone.h>
#include "kfc_fw_config.h"
#include "EventScheduler.h"
#include "plugins.h"

#if DEBUG_SSD1306_PLUGIN
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#include <crc16.h>

class GFXfontContainer {
public:
    typedef std::unique_ptr<GFXfontContainer> Ptr;

    const static uint8_t VERSION = 0x02;
    const static uint16_t OPTIONS_HAS_CRC16 = 0x01 << 8;

    GFXfontContainer() {
        _gfxFont.bitmap = nullptr;
        _gfxFont.glyph = nullptr;
    }
    ~GFXfontContainer() {
        if (_gfxFont.bitmap) {
            free(_gfxFont.bitmap);
        }
        if (_gfxFont.glyph) {
            free(_gfxFont.glyph);
        }
    }

    const GFXfont *getFontPtr() const {
        _debug_printf_P(PSTR("GFXfontContainer::getFontPtr(): _gfxFont.bitmap=%p\n"), _gfxFont.bitmap);
        if (_gfxFont.bitmap) {
            return &_gfxFont;
        }
        return nullptr;
    }

    static bool readFromFile(const String &filename, GFXfontContainer::Ptr &target) {
        _debug_printf_P(PSTR("GFXfontContainer::readFromFile(%s)\n"), filename.c_str());

        File file = SPIFFS.open(filename, fs::FileOpenMode::read);
        if (file && file.size()) {
            return readFromStream(file, target);
        }
        return false;
    }

    static bool readFromStream(Stream &stream, GFXfontContainer::Ptr &target) {
        _debug_printf_P(PSTR("GFXfontContainer::readFromStream()\n"));

        GFXfontContainer::Ptr font(new GFXfontContainer());
        uint16_t length, options, headerCrc;
        uint16_t crc = ~0;

        if (stream.readBytes((uint8_t *)&options, sizeof(options)) != sizeof(options) || stream.readBytes((uint8_t *)&headerCrc, sizeof(headerCrc)) != sizeof(headerCrc)) {
            return false;
        }
        if ((options & 0xff) > VERSION) {
            _debug_printf_P(PSTR("Version not supported\n"));
            return false;
        }
        _debug_printf_P(PSTR("Version=%02x, flags=%02x, CRC=%04x\n"), (options & 0xff), ((options >> 8) & 0xff), headerCrc);
        if ((options & OPTIONS_HAS_CRC16) != OPTIONS_HAS_CRC16) {
            if (headerCrc != 0xffff) { // CRC16 is always 0xffff for version 1
                _debug_printf_P(PSTR("Invalid CRC\n"));
                return false;
            }
        }

        auto &_font = font->getFont();
        if (
            stream.readBytes((uint8_t *)&_font.first, sizeof(_font.first)) +
            stream.readBytes((uint8_t *)&_font.last, sizeof(_font.last)) +
            stream.readBytes((uint8_t *)&_font.yAdvance, sizeof(_font.yAdvance)) != 3
        ) {
            return false;
        }

        crc = crc16_update(crc, &_font.first, sizeof(_font.first));
        crc = crc16_update(crc, &_font.last, sizeof(_font.last));
        crc = crc16_update(crc, &_font.yAdvance, sizeof(_font.yAdvance));

        _debug_printf_P(PSTR("First '%c' last '%c' yAdvance %u\n"), _font.first, _font.last, _font.yAdvance);

        if (stream.readBytes((uint8_t *)&length, sizeof(length)) != sizeof(length)) {
            return false;
        }
        crc = crc16_update(crc, (uint8_t *)&length, sizeof(length));

        _debug_printf_P(PSTR("Bitmap size %u\n"), length);
        _font.bitmap = (uint8_t *)malloc(length);
        if (!_font.bitmap || stream.readBytes((uint8_t *)_font.bitmap, length) != length) {
            return false;
        }
        crc = crc16_update(crc, (uint8_t *)_font.bitmap, length);

        if (stream.readBytes((uint8_t *)&length, sizeof(length)) != sizeof(length)) {
            return false;
        }
        crc = crc16_update(crc, (uint8_t *)&length, sizeof(length));

        _debug_printf_P(PSTR("Glyph table size %u\n"), length);
        _font.glyph = (GFXglyph *)malloc(length);
        if (!_font.glyph || stream.readBytes((uint8_t *)_font.glyph, length) != length) {
            return false;
        }
        crc = crc16_update(crc, (uint8_t *)_font.glyph, length);
        if (options & OPTIONS_HAS_CRC16 && crc != headerCrc) {
            _debug_printf_P(PSTR("CRC mismatch %04x!=%04x\n"), crc, headerCrc);
            return false;
        }

        _debug_printf_P(PSTR("CRC=%04x,stored CRC=%04x\n"), crc, headerCrc);
        font.swap(target);
        return true;
    }

private:
    GFXfont &getFont() {
        return _gfxFont;
    }

private:
    GFXfont _gfxFont;
};

Adafruit_SSD1306 Display(SSD1306_PLUGIN_WIDTH, SSD1306_PLUGIN_HEIGHT, &Wire, SSD1306_PLUGIN_RESET_PIN);
GFXfontContainer::Ptr clockFont;

static EventScheduler::TimerPtr ssd1306_status_timer = nullptr;

void ssd1306_update_time(EventScheduler::TimerPtr timer) {
    _debug_println(F("ssd1306_update_time()"));
    char buf[32];
    time_t now = time(nullptr);
    Display.setCursor(0, 24);
#if SSD1306_PLUGIN_HEIGHT == 64
    timezone_strftime_P(buf, sizeof(buf), PSTR("%F %Z\n%T"), timezone_localtime(&now));
    Display.fillRect(0, 24, SSD1306_PLUGIN_WIDTH, SSD1306_PLUGIN_HEIGHT - 24, BLACK);
    Display.print(strtok(buf, "\n"));
    if (clockFont) {
        Display.setFont(clockFont->getFontPtr());
        Display.setCursor(0, 60);
    } else {
        Display.setCursor(0, 24);
        Display.setTextSize(2, 3);
    }
    Display.print(strtok(nullptr, ""));
    Display.setTextSize(1);
    Display.setFont(nullptr);
#else
    Display.fillRect(0, 24, SSD1306_PLUGIN_WIDTH, SSD1306_PLUGIN_HEIGHT - 24, BLACK);
    timezone_strftime_P(buf, sizeof(buf), PSTR("%F %T"), timezone_localtime(&now));
    Display.print(buf);
#endif
    Display.display();
}

void ssd1306_clear_display() {
    _debug_println(F("ssd1306_clear_display()"));
    Display.setTextColor(WHITE);
    Display.setFont(nullptr);
    Display.setTextSize(1);
    Display.clearDisplay();
    Display.setCursor(0, 0);
    Display.display();
}

void ssd1306_update_status() {
    _debug_println(F("ssd1306_update_status()"));
    Display.clearDisplay();
    Display.setTextColor(WHITE);
    Display.setTextSize(1);
    Display.setCursor(0, 0);
    if (WiFi.isConnected()) {
        Display.println(F("WiFi connected to"));
        Display.println(WiFi.SSID());
        Display.print(F("IP: "));
        Display.print(WiFi.localIP().toString());
    } else {
        Display.print(F("WiFi disconnected"));
    }
    ssd1306_update_time(nullptr);
}

void ssd1306_wifi_event(uint8_t event, void *payload) {
    _debug_println(F("ssd1306_wifi_event()"));
    if (ssd1306_status_timer) {
        ssd1306_update_status();
    }
}

void ssd1306_disable_status() {
    _debug_println(F("ssd1306_disable_status()"));
    if (Scheduler.hasTimer(ssd1306_status_timer)) {
        Scheduler.removeTimer(ssd1306_status_timer);
        ssd1306_status_timer = nullptr;
        ssd1306_clear_display();
    }
}

void ssd1306_enable_status() {
    _debug_println(F("ssd1306_enable_status()"));
    Scheduler.removeTimer(ssd1306_status_timer);
    ssd1306_clear_display();
    ssd1306_update_status();
    Scheduler.addTimer(&ssd1306_status_timer, 1000, true, ssd1306_update_time);
}

void ssd1306_setup() {
    _debug_printf_P(PSTR("ssd1306_setup(): sda=%d, scl=%d, rst=%d, i2c_addr=%02x, width=%d, height=%d\n"),
        SSD1306_PLUGIN_SDA_PIN,
        SSD1306_PLUGIN_SCL_PIN,
        SSD1306_PLUGIN_RESET_PIN,
        SSD1306_PLUGIN_I2C_ADDR,
        SSD1306_PLUGIN_WIDTH,
        SSD1306_PLUGIN_HEIGHT
    );

    Wire.begin(SSD1306_PLUGIN_SDA_PIN, SSD1306_PLUGIN_SCL_PIN);
    if (!Display.begin(SSD1306_SWITCHCAPVCC, SSD1306_PLUGIN_I2C_ADDR)) {
        Serial.printf_P(PSTR("Failed to initialize SSD1306 display: SDA=%d, SCL=%d, RST=%d, I2C=%02x\n"),
            SSD1306_PLUGIN_SDA_PIN,
            SSD1306_PLUGIN_SCL_PIN,
            SSD1306_PLUGIN_RESET_PIN,
            SSD1306_PLUGIN_I2C_ADDR
        );
    }

    Display.setTextColor(WHITE);
    Display.setTextSize(1);
    Display.cp437(true);

    Display.clearDisplay();
    Display.println(F("KFC Firmware"));
    Display.print('v');
    Display.print(config.getShortFirmwareVersion());
    Display.display();


    GFXfontContainer::readFromFile(F("/fonts/7digit"), clockFont);

#if SSD1306_PLUGIN_DISPLAY_STATUS_DELAY
    Scheduler.addTimer(SSD1306_PLUGIN_DISPLAY_STATUS_DELAY, false, [](EventScheduler::TimerPtr timer) {
        ssd1306_enable_status();
    });
#endif

    WiFiCallbacks::add(WiFiCallbacks::CONNECTED|WiFiCallbacks::DISCONNECTED, ssd1306_wifi_event);
}

class SSD1306Plugin : public PluginComponent {
public:
    SSD1306Plugin() {
        REGISTER_PLUGIN(this, "SSD1306Plugin");
    }

    virtual PGM_P getName() const {
        return PSTR("ssd1306");
    }

    virtual void setup(PluginSetupMode_t mode) override {
        ssd1306_setup();
    }

    virtual bool hasAtMode() const override {
        return true;
    }
    virtual void atModeHelpGenerator() override;
    virtual bool atModeHandler(AtModeArgs &args) override;

};

static SSD1306Plugin plugin;

#if AT_MODE_SUPPORTED

#if defined(ESP8266)
    #include <ESP8266HttpClient.h>
#elif defined(ESP32)
    #include <HTTPClient.h>
#endif

GFXfontContainer::Ptr currentFont;

#include "at_mode.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(SSDST, "SSDST", "Display time and WiFi status continuously");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(SSDCLR, "SSDCLR", "Clear display and reset to detault");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(SSDXY, "SSDXY", "<x,y>", "Set cursor");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(SSDW, "SSDW", "<line1[,line2,...]>", "Display text");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(SSDRF, "SSDRF", "<filename>", "Read font from SPIFFS");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(SSDDF, "SSDDF", "<url>", "Download font");

void SSD1306Plugin::atModeHelpGenerator()
{
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(SSDCLR), getName());
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(SSDST), getName());
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(SSDXY), getName());
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(SSDW), getName());
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(SSDRF), getName());
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(SSDDF), getName());
}

bool SSD1306Plugin::atModeHandler(AtModeArgs &args)
{
    if (String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(SSDCLR))) {
        ssd1306_disable_status();
        ssd1306_clear_display();
        at_mode_print_ok(serial);
        return true;
    }
    else if (String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(SSDDF))) {
        if (args.requireArgs(1, 1)) {
            HTTPClient http;
            auto url = args.get(0);
            http.begin(url);
            int httpCode = http.GET();
            if (httpCode == 200) {
                Stream &stream = http.getStream();
                serial.printf_P("+SSDDF: Downloading %d bytes from %s...\n", http.getSize(), url);
                if (GFXfontContainer::readFromStream(stream, currentFont)) {
                    at_mode_print_ok(serial);
                } else {
                    serial.println(F("+SSDDF: Failed to load font"));
                }
            } else {
                serial.printf_P(PSTR("+SSDDF: HTTP error, code %d"), httpCode);
            }
            http.end();
            if (currentFont) {
                Display.setFont(currentFont->getFontPtr());
            }
        }
        return true;
    }
    else if (String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(SSDRF))) {
        if (args.requireArgs(1, 1)) {
            auto filename = args.get(0);
            auto file = SPIFFS.open(filename, fs::FileOpenMode::read);
            if (file) {
                ssd1306_disable_status();
                serial.printf_P("+SSDRF: Reading %d bytes from %s...\n", file.size(), filename);
                if (GFXfontContainer::readFromStream(file, currentFont)) {
                    at_mode_print_ok(serial);
                } else {
                    serial.println(F("+SSDRF: Failed to load font"));
                }
                file.close();
                if (currentFont) {
                    Display.setFont(currentFont->getFontPtr());
                }
            } else {
                serial.printf_P(PSTR("+SSDRF: Cannot open %s\n"), filename);
            }
        }
        return true;
    }
    else if (String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(SSDXY))) {
        if (args.requireArgs(2, 2)) {
            ssd1306_disable_status();
            uint16_t x = (uint16_t)args.toInt(0);
            uint16_t y = (uint16_t)args.toInt(1);
            Display.setCursor(x, y);
            serial.printf_P(PSTR("+SSDXY: x=%d,y=%d\n"), x, y);
        }
        return true;
    }
    else if (String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(SSDW))) {
        ssd1306_disable_status();
        for(auto line: args.getArgs()) {
            Display.println(line);
        }
        Display.display();
        at_mode_print_ok(serial);
        return true;
    }
    else if (String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(SSDST))) {
        ssd1306_enable_status();
        at_mode_print_ok(serial);
        return true;
    }
    return false;
}

#endif

#endif
