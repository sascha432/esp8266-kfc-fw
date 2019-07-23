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

Adafruit_SSD1306 Display(SSD1306_PLUGIN_WIDTH, SSD1306_PLUGIN_HEIGHT, &Wire, SSD1306_PLUGIN_RESET_PIN);

static EventScheduler::TimerPtr ssd1306_status_timer = nullptr;

void ssd1306_update_time(EventScheduler::TimerPtr timer) {
    char buf[32];
    time_t now = time(nullptr);
    Display.setCursor(0, 24);
    Display.fillRect(0, 24, SSD1306_PLUGIN_WIDTH, 16, BLACK);
    timezone_strftime_P(buf, sizeof(buf), PSTR("%F\n%T %Z"), timezone_localtime(&now));
    Display.print(buf);
    Display.display();
}

void ssd1306_clear_display() {
    Display.clearDisplay();
    Display.setCursor(0, 0);
    Display.display();
}

void ssd1306_update_status() {
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
    if (ssd1306_status_timer) {
        ssd1306_update_status();
    }
}

void ssd1306_disable_status() {
    if (Scheduler.hasTimer(ssd1306_status_timer)) {
        Scheduler.removeTimer(ssd1306_status_timer);
        ssd1306_status_timer = nullptr;
        ssd1306_clear_display();
    }
}

void ssd1306_enable_status() {
    Scheduler.removeTimer(ssd1306_status_timer);
    ssd1306_update_status();
    ssd1306_status_timer = Scheduler.addTimer(1000, true, ssd1306_update_time);
}

void ssd1306_setup() {

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

#if SSD1306_PLUGIN_DISPLAY_STATUS_DELAY
    Scheduler.addTimer(SSD1306_PLUGIN_DISPLAY_STATUS_DELAY, false, [](EventScheduler::TimerPtr timer) {
        ssd1306_enable_status();
    });
#endif

    WiFiCallbacks::add(WiFiCallbacks::CONNECTED|WiFiCallbacks::DISCONNECTED, ssd1306_wifi_event);
}

#if AT_MODE_SUPPORTED

#include "at_mode.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(SSDCLR, "SSDCLR", "Clear display");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(SSDW, "SSDW", "<line1[,line2,...]>", "Display text");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(SSDST, "SSDST", "Display time and WiFi status");

bool ssd1306_at_mode_command_handler(Stream &serial, const String &command, int8_t argc, char **argv) {

    if (command.length() == 0) {
        at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(SSDCLR));
        at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(SSDW));
        at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(SSDST));
    }
    else if (constexpr_String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(SSDCLR))) {
        ssd1306_disable_status();
        ssd1306_clear_display();
        at_mode_print_ok(serial);
        return true;
    }
    else if (constexpr_String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(SSDW))) {
        ssd1306_disable_status();
        int8_t line = 0;
        while(line < argc) {
            Display.println(argv[line++]);
        }
        Display.display();
        at_mode_print_ok(serial);
        return true;
    }
    else if (constexpr_String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(SSDST))) {
        ssd1306_enable_status();
        at_mode_print_ok(serial);
        return true;
    }
    return false;
}

#endif

PROGMEM_PLUGIN_CONFIG_DEF(
/* pluginName               */ ssd1306,
/* setupPriority            */ 1,
/* allowSafeMode            */ false,
/* autoSetupWakeUp          */ true,
/* rtcMemoryId              */ 0,
/* setupPlugin              */ ssd1306_setup,
/* statusTemplate           */ nullptr,
/* configureForm            */ nullptr,
/* reconfigurePlugin        */ nullptr,
/* reconfigure Dependencies */ nullptr,
/* prepareDeepSleep         */ nullptr,
/* atModeCommandHandler     */ ssd1306_at_mode_command_handler
);

#endif
