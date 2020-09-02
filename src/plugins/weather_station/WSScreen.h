/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include "WSDraw.h"

class WeatherStationBase;

extern WeatherStationBase &__weatherStationGetPlugin();

namespace WeatherStation {

    using ConfigType = WSDraw::WeatherStationConfigType;

    namespace Screen {

        class BaseScreen {
        public:
            BaseScreen(uint32_t timeout = 0, BaseScreen *nextScreen = nullptr) :
                _wsDraw(reinterpret_cast<WSDraw &>(__weatherStationGetPlugin())),
                _config(_wsDraw.getConfig()),
                _timeout(timeout),
                _nextScreen(nextScreen)
            {
            }

        protected:
            friend WSDraw;
            friend WeatherStationBase;

            virtual void deletePrevScreen(BaseScreen *&screen) {
                if (screen) {
                    delete screen;
                    screen = nullptr;
                }
            }
            virtual void initNextScreen();
            virtual uint32_t getTimeout() {
                return _nextScreen ? _timeout : 0;
            }
            virtual BaseScreen *getNextScreen() {
                return _nextScreen;
            }

            // called after clearing the entire screen
            virtual void drawScreen(WeatherStationCanvas &canvas) {
                canvas.fillScreen(COLOR_BACKGROUND);
                _wsDraw._drawTime();
            }
            // called to update the entire screen still having contents
            // after setting a new screen it is always cleared
            virtual void updateScreen(WeatherStationCanvas &canvas) {
                drawScreen(canvas);
            }
            // called to update screen partially
            virtual void updateTime(WeatherStationCanvas &canvas) {
                drawScreen(canvas);
            }

        protected:
            WSDraw &_wsDraw;
            ConfigType &_config;
            uint32_t _timeout;
            BaseScreen *_nextScreen;
        };

        class MainScreen : public BaseScreen {
        public:
            MainScreen() {}

        protected:
            virtual uint32_t getTimeout() override;
            virtual BaseScreen *getNextScreen() override;
        };

        class IndoorScreen : public BaseScreen {
        public:
            IndoorScreen() {}

        protected:
            virtual uint32_t getTimeout() override;
            virtual BaseScreen *getNextScreen() override;
        };

        class UpdateScreen : public BaseScreen {
        public:
            UpdateScreen() {}
        };

        class TextScreen : public BaseScreen {
        public:
            TextScreen(const String &message, uint32_t timeout = 0, BaseScreen *next = nullptr) : BaseScreen(timeout, next), _message(message), _prevScreen(nullptr) {}
            ~TextScreen() {
                if (_prevScreen) {
                    delete _prevScreen;
                }
            }

            // store previous screen
            virtual void deletePrevScreen(BaseScreen *&screen) override {
                if (screen) {
                    _prevScreen = screen;
                    screen = nullptr;
                }
            }

            // restore previous screen or goto main if none
            virtual BaseScreen *getNextScreen() override {
                if (!_prevScreen) {
                    return new MainScreen();
                }
                auto tmp =  _prevScreen;
                _prevScreen = nullptr;
                return tmp;
            }

        protected:
            String _message;
            BaseScreen *_prevScreen;
        };

        class MessageScreen : public TextScreen {
        public:
            MessageScreen(const String &title, const String &message, uint32_t timeout = 0, BaseScreen *next = nullptr) : TextScreen(message, timeout, next), _title(title) {}

        private:
            String _title;
        };

    }

}
