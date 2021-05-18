/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include "IOExpander.h"

#if IOEXPANDER_INCLUDE_HPP == 0
#define IOEXPANDER_INLINE
#include "IOExpander.hpp"
#endif


// TinyPwm is a 3 pin IO expander with one PWM pin and 2 ADC pins
// pinMode is fixed and digtalRead/Write is not supported
