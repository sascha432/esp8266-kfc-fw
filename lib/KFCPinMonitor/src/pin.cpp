/**
  Author: sascha_lammers@gmx.de
*/

#include "pin_monitor.h"
#include "pin.h"

using namespace PinMonitor;

Pin::Pin(ConfigType config, void *arg) :
    _config(config),
    _arg(arg),
    _state(_config._isActive(digitalRead(config.getPin())))
{
}

Pin::~Pin()
{
}

ConfigType &Pin::configure()
{
    return _config;
}

void Pin::event(bool state, TimeType now)
{

}
