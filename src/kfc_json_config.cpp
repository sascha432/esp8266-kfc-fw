// /**
// * Author: sascha_lammers@gmx.de
// */

// #include <Arduino_compat.h>
// #include <ArduinoJson.h>
// #include <PrintString.h>
// #include "kfc_json_config.h"

// #include "debug_helper_enable.h"

// // TODO PROGMEM
// static const char *mainConfigFile = { "/.cfg/main_cfg.json" };
// static const char *stateConfigFile = { "/.cfg/state_cfg.json" };

// KFCJsonConfig::KFCJsonConfig() :
//     _json(1024),
//     _configType(0),
//     _modified(false)
// {
// }

// void KFCJsonConfig::begin(const __FlashStringHelper *subConfig)
// {
//     if (_modified) {
//         __LDBG_printf("modified state set, calling end");
//         end();
//     }
//     _modified = false;
//     _configType = pgm_read_byte(subConfig);
//     String realFilename = _configType == 'm' ? mainConfigFile : stateConfigFile;
//     String filename = PrintString(F("%c%08x.json"), _configType, crc32(realFilename.c_str(), realFilename.length()));
//     _file = KFCFS.open(filename, FileOpenMode::writeplus);
//     if (_file) {
//         // read config from file
//         deserializeMsgPack(_json, _file);
//         if (!_json["rfn"]) {
//             _json["rfn"] = realFilename;
//             _modified = true;
//         }
//     }
// }

// void KFCJsonConfig::end()
// {
//     switch(_configType) {
//         case 'm':
//             __LDBG_printf("end type main");
//             break;
//         case 's':
//             __LDBG_printf("end type state");
//             break;
//         default:
//             __LDBG_printf("end without type");
//             break;
//     }
//     if (_modified) {
//         // save changes
//         _file.truncate(0);
//         serializeJson(_json, _file);
//     }
//     _modified = false;
//     _configType = 0;
//     _file.close();
// }
