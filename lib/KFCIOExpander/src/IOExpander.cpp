    /**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include "IOExpander.h"

#if HAVE_IOEXPANDER

#if IOEXPANDER_INCLUDE_HPP == 0
#define IOEXPANDER_INLINE
#include "IOExpander.hpp"
#endif

namespace IOExpander {

    #define ST <
    #define GT >

    ConfigIterator<IOEXPANDER_DEVICE_CONFIG> config;

    #undef ST
    #undef GT

}

#endif
