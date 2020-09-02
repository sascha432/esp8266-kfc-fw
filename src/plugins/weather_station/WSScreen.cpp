/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "WSScreen.h"

using namespace WeatherStation::Screen;

void BaseScreen::initNextScreen()
{
    auto &wsDraw = reinterpret_cast<WSDraw &>(__weatherStationGetPlugin());
    _timeout = getTimeout();
    if (_timeout) {
        _Timer(wsDraw._screenTimer).add(Event::milliseconds(_timeout), false, [this, &wsDraw](Event::CallbackTimerPtr timer) {
            wsDraw.setScreen(getNextScreen());
        });
    } else {
        _Timer(wsDraw._screenTimer).remove();
    }
}


uint32_t MainScreen::getTimeout()
{
    return _config.screenTimer[1] * 1000U;
}

BaseScreen *MainScreen::getNextScreen()
{
    return new IndoorScreen();
}

uint32_t IndoorScreen::getTimeout()
{
    return _config.screenTimer[0] * 1000U;
}

BaseScreen *IndoorScreen::getNextScreen()
{
    return new MainScreen();
}
