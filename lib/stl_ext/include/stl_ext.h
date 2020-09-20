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

#if _MSC_VER
#include <vcruntime.h>
#endif

#if (__cplusplus > 201703L) || (_HAS_CXX20)
#define __HAS_CPP20		1
#define __HAS_CPP17		1
#define __HAS_CPP14		1
#define __HAS_CPP11		1
#elif (__cplusplus >= 201703L) || (_HAS_CXX17)
#define __HAS_CPP20		0
#define __HAS_CPP17		1
#define __HAS_CPP14		1
#define __HAS_CPP11		1
#elif (__cplusplus >= 201402L) || (_HAS_CXX14)
#define __HAS_CPP20		0
#define __HAS_CPP17		0
#define __HAS_CPP14		1
#define __HAS_CPP11		1
#elif (__cplusplus >= 201103L) || (_HAS_CXX11)
#define __HAS_CPP20		0
#define __HAS_CPP17		0
#define __HAS_CPP14		0
#define __HAS_CPP11		1
#endif
