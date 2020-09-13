/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#ifndef STL_STD_EXT_NAMESPACE
#define STL_STD_EXT_NAMESPACE           std
#endif

#ifndef STL_STD_EXT_NAMESPACE_EX
#define STL_STD_EXT_NAMESPACE_EX        std_ex
#endif


#define __HAS_CPP11                     (__cplusplus >= 201103L) || (defined(_MSC_VER) && (__cplusplus == 199711L || __cplusplus >= 201103L))
#define __HAS_CPP14                     (__cplusplus >= 201402L) || (defined(_MSC_VER) && (__cplusplus == 199711L || __cplusplus >= 201402L))
#define __HAS_CPP17                     (__cplusplus >= 201703L) || (defined(_MSC_VER) && (__cplusplus == 199711L || __cplusplus >= 201703L))
