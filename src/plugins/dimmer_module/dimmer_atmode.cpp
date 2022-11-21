#if AT_MODE_SUPPORTED

/**
 * Author: sascha_lammers@gmx.de
 */

#include "dimmer_base.h"
#include "dimmer_module.h"
#include "dimmer_plugin.h"

#if DEBUG_IOT_DIMMER_MODULE
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

using namespace Dimmer;

#include "at_mode.h"

#define DIMMER_COMMANDS "reset|weeprom|info|print|write|factory|zc,value"
#undef DISPLAY

enum class DimmerCommandType {
    INVALID = -1,
    RESET,
    WRITE_EEPROM,
    INFO,
    PRINT,
    WRITE,
    FACTORY,
    ZERO_CROSSING
};

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(DIMG, "DIMG", "Get level(s)");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(DIMS, "DIMS", "<channel>,<level>[,<time>]", "Set level");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(DIMCF, "DIMCF", "<" DIMMER_COMMANDS ">", "Configure dimmer firmware");

#if AT_MODE_HELP_SUPPORTED

ATModeCommandHelpArrayPtr Base::atModeCommandHelp(size_t &size) const
{
    static ATModeCommandHelpArray tmp PROGMEM = {
        PROGMEM_AT_MODE_HELP_COMMAND(DIMG),
        PROGMEM_AT_MODE_HELP_COMMAND(DIMS),
        PROGMEM_AT_MODE_HELP_COMMAND(DIMCF),
    };
    size = sizeof(tmp) / sizeof(tmp[0]);
    return tmp;
}

#endif

bool Base::atModeHandler(AtModeArgs &args, const Base &dimmer, int32_t maxLevel)
{
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(DIMG))) {
        for(uint8_t i = 0; i < dimmer.getChannelCount(); i++) {
            args.print(F("%u: %d (%s)"), i, getChannel(i), getChannelState(i) ? PSTR("on") : PSTR("off"), dimmer_plugin.getChannels().at(i).getStorededBrightness());
        }
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(DIMS))) {
        if (args.requireArgs(2, 3)) {
            uint8_t channel = args.toInt(0);
            if (channel < dimmer.getChannelCount()) {
                int level = args.toIntMinMax(1, 0, maxLevel);
                float time = args.toFloat(2, -1);
                args.print(F("Set %u: %d (time %.2f)"), channel, level, time);
                setChannel(channel, level, time);
            }
            else {
                args.print(F("Invalid channel"));
            }
        }
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(DIMCF))) {
        auto commands = F(DIMMER_COMMANDS);
        auto index = static_cast<DimmerCommandType>(stringlist_find_P(commands, args.get(0), '|'));
        switch(index) {
            case DimmerCommandType::RESET: {
                    args.print(F("Pulling GPIO%u low for 10ms"), STK500V1_RESET_PIN);
                    dimmer.resetDimmerMCU();
                    args.print(F("GPIO%u set to input"), STK500V1_RESET_PIN);
                }
                break;
            case DimmerCommandType::FACTORY: {
                    _wire.restoreFactory();
                    _wire.writeEEPROM();
                    args.ok();
                }
                break;
            case DimmerCommandType::ZERO_CROSSING: {
                    _wire.setZeroCrossing(args.toIntMinMax(1, 0, 0xffff));
                    args.ok();
                }
                break;
            case DimmerCommandType::INFO: {
                    _wire.printInfo();
                    args.ok();
                }
                break;
            case DimmerCommandType::PRINT: {
                    _wire.printConfig();
                    args.ok();
                }
                break;
            case DimmerCommandType::WRITE: {
                    _wire.writeConfig();
                    args.ok();
                }
                break;
            case DimmerCommandType::WRITE_EEPROM: {
                    _wire.writeEEPROM();
                    args.ok();
                }
                break;
            case DimmerCommandType::INVALID:
            // default:
                args.print(F("Invalid command: %s: expected: <%s>"), args.toString(0).c_str(), commands);
                break;
        }
        return true;
    }
    return false;
}

#endif
