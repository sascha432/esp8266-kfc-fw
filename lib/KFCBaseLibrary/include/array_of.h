/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

template <typename V, typename... T>
constexpr auto array_of(T&&... t)
    -> std::array < V, sizeof...(T) >
{
    return {{ (V)std::forward<T>(t)... }};
}
