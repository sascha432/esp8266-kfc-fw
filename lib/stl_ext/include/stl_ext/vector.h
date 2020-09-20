/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "../stl_ext.h"
#include <algorithm>
#include <vector>

namespace STL_STD_EXT_NAMESPACE_EX {

    template<typename _Ta>
    void remove_duplicates(_Ta &v)
    {
        auto end = v.end();
        for (auto it = v.begin(); it != end; ++it) {
            end = std::remove(it + 1, end, *it);
        }
        v.erase(end, v.end());
    }

}
