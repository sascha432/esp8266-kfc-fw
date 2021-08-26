/**
  Author: sascha_lammers@gmx.de
*/

#if AT_MODE_SUPPORTED

#include <Arduino_compat.h>
#include <stl_ext/algorithm.h>
#include "at_mode.h"
#include "kfc_fw_config.h"
#include <Form.h>

#if DEBUG_AT_MODE
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

void AtModeArgs::ok() const
{
    if (config.isSafeMode()) {
        _output.println(F("OK - SAFE MODE"));
    }
    else {
        _output.println(FSPGM(OK));
    }
}

#endif
