/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_LED_MATRIX == 0

#include <Arduino_compat.h>
#include "animation.h"
#include "seven_segment_display.h"

namespace Clock {

    namespace SevenSegment {

        const SegmentType segmentTypeTranslationTable[] PROGMEM = {
            SegmentType::DIGIT_0, SegmentType::DIGIT_1, SegmentType::DIGIT_2, SegmentType::DIGIT_3, SegmentType::DIGIT_4, SegmentType::DIGIT_5, SegmentType::DIGIT_6, SegmentType::DIGIT_7, SegmentType::DIGIT_8, SegmentType::DIGIT_9,
            SegmentType::DIGIT_A, SegmentType::DIGIT_B, SegmentType::DIGIT_C, SegmentType::DIGIT_D, SegmentType::DIGIT_E, SegmentType::DIGIT_F
        };

        const PixelAddressType digitsTranslationTable[] PROGMEM = { SEVEN_SEGMENT_DIGITS_TRANSLATION_TABLE };

        const PixelAddressType colonTranslationTable[] PROGMEM = { SEVEN_SEGMENT_COLON_TRANSLATION_TABLE };

        const PixelAddressType pixelAnimationOrder[] PROGMEM = { SEVEN_SEGMENT_PIXEL_ANIMATION_ORDER };

    }
}

#endif
