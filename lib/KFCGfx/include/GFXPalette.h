/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "GFXCanvasConfig.h"
#include <vector>

#if DEBUG_GFXCANVAS
#    include "debug_helper_enable.h"
#else
#    include "debug_helper_disable.h"
#endif

namespace GFXCanvas {


    // --------------------------------------------------------------------
    // Base class ColorPalette
    // Single colors must not be deleted, only new colors can be pushed
    // at the end
    // clear() can be used to remove all colors from the palette
    // --------------------------------------------------------------------

    class ColorPalette {
    public:
        static constexpr uint32_t kColorsMax = 0;

        using ColorPaletteVector = std::vector<ColorType>;
        using ColorPaletteIterator = ColorPaletteVector::iterator;

    public:
        void clear();
        ColorPaletteIterator add(ColorType color);
        ColorPaletteIterator get(ColorIndexType index);
        ColorPaletteIterator find(ColorType color);
        bool isValid(const ColorPaletteIterator &iter);
        ColorIndexType getIndex(const ColorPaletteIterator &iter);
        ColorType getColor(const ColorPaletteIterator &iter);
        ColorIndexType _size();

    public:
        virtual ~ColorPalette();

        virtual size_t capacity() const;
        virtual size_t size() const;
        virtual ColorPaletteVector *getColorPalette();

        virtual uint8_t bits() const;
        virtual uint32_t maxColors() const;

        // virtual void fillPaletteColors(BitmapHeaderType &_fileHeader) {}
    };

    inline void ColorPalette::clear()
    {
        auto _palette = getColorPalette();
        _palette->clear();
    }

    inline ColorPalette::ColorPaletteIterator ColorPalette::add(ColorType color)
    {
        auto _palette = getColorPalette();
        auto iterator = std::find(_palette->begin(), _palette->end(), color);
        if (iterator == _palette->end()) {
            _palette->emplace_back(color);
            return std::prev(_palette->end());
        }
        return iterator;
    }

    inline ColorPalette::ColorPaletteIterator ColorPalette::get(ColorIndexType index)
    {
        auto _palette = getColorPalette();
        if (index >= 0 && index < _size()) {
            auto target = _palette->at(index);
            for(auto iter = _palette->begin(); iter != _palette->end(); ++iter) {
                if (*iter == target) {
                    return iter;
                }
            }
        }
        return _palette->end();
    }

    inline ColorPalette::ColorPaletteIterator ColorPalette::find(ColorType color)
    {
        auto _palette = getColorPalette();
        return std::find(_palette->begin(), _palette->end(), color);
    }

    inline bool ColorPalette::isValid(const ColorPaletteIterator &iter)
    {
        auto _palette = getColorPalette();
        for(auto iter2 = _palette->begin(); iter2 != _palette->end(); ++iter2) {
            if (iter == iter2 && *iter == *iter2) {
                return true;
            }
        }
        return false;
    }

    inline ColorIndexType ColorPalette::getIndex(const ColorPaletteIterator &iter)
    {
        auto _palette = getColorPalette();
        ColorIndexType count = 0;
        for(auto iter2 = _palette->begin(); iter2 != _palette->end(); ++iter2) {
            if (iter == iter2 && *iter == *iter2) {
                return count;
            }
            count++;
        }
        return -1;
    }

    inline ColorType ColorPalette::getColor(const ColorPaletteIterator &iter)
    {
        auto _palette = getColorPalette();
        for(auto iter2 = _palette->begin(); iter2 != _palette->end(); ++iter2) {
            if (iter == iter2 && *iter == *iter2) {
                return *iter;
            }
        }
        return 0;
    }

    inline ColorIndexType ColorPalette::_size()
    {
        return getColorPalette()->size();
    }


    inline ColorPalette::~ColorPalette()
    {
    }

    inline size_t ColorPalette::capacity() const
    {
        return 0;
    }

    inline size_t ColorPalette::size() const
    {
        return 0;
    }

    inline ColorPalette::ColorPaletteVector *ColorPalette::getColorPalette()
    {
        return nullptr;
    }

    inline uint8_t ColorPalette::bits() const
    {
        return 0;
    }

    inline uint32_t ColorPalette::maxColors() const
    {
        return 0;
    }

    // --------------------------------------------------------------------
    // ColorPalette with 4 bit / 16 colors
    // --------------------------------------------------------------------

    class ColorPalette4 : public ColorPalette
    {
    public:
        static constexpr uint32_t kColorsMax = 16;

    public:
        virtual size_t capacity() const;
        virtual size_t size() const;
        virtual ColorPaletteVector *getColorPalette();

        virtual uint8_t bits() const;
        virtual uint32_t maxColors() const;

    private:
        ColorPaletteVector _palette;
    };

    inline size_t ColorPalette4::capacity() const
    {
        return kColorsMax;
    }

    inline size_t ColorPalette4::size() const
    {
        return _palette.size();
    }

    inline ColorPalette4::ColorPaletteVector *ColorPalette4::getColorPalette()
    {
        return &_palette;
    }

    inline uint8_t ColorPalette4::bits() const
    {
        return 4;
    }

    inline uint32_t ColorPalette4::maxColors() const
    {
        return kColorsMax;
    }

}

#if DEBUG_GFXCANVAS
#    include "debug_helper_disable.h"
#endif
