
/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <pgmspace.h>
#include <boost/preprocessor/repetition/repeat_from_to.hpp>
#include <boost/preprocessor/punctuation/comma_if.hpp>
#include <boost/preprocessor/seq/for_each.hpp>

// utility for creating arrays with variable names for forms in PROGMEM

#define __VARNAME_WITH_INDEX(name, index)                       PSTR(_STRINGIFY(BOOST_PP_CAT(BOOST_PP_CAT(name, _), index)))
#define __VARNAME_WITH_INDEX_DECL(z, n, text)                   BOOST_PP_COMMA_IF(n) __VARNAME_WITH_INDEX(text, n)

// PROGMEM_DEF_LOCAL_VARNAME_ARRAY(_VAR_, id, 3)
// const char *_VAR__id[3] = { PSTR("id_0"), PSTR("id_1"), PSTR("id_2") };
#define PROGMEM_DEF_LOCAL_VARNAME_ARRAY(prefix, name, num)      const char *BOOST_PP_CAT(prefix, name)[num] = { BOOST_PP_REPEAT_FROM_TO(0, num, __VARNAME_WITH_INDEX_DECL, name) };

// PROGMEM_DEF_PROGMEM_VARNAME_ARRAY(_VAR_, id, 3)
// static const char *_VAR_id[3] PROGMEM = { PSTR("id_0"), PSTR("id_1"), PSTR("id_2") };
#define PROGMEM_DEF_PROGMEM_VARNAME_ARRAY(prefix, name, num)    static const char *BOOST_PP_CAT(prefix, name)[num] __attribute__((__aligned__(4))) PROGMEM = { BOOST_PP_REPEAT_FROM_TO(0, num, __VARNAME_WITH_INDEX_DECL, name) };

#define __LOCAL_VARNAME_SEQ_DECL(r, data, name)                 PROGMEM_DEF_LOCAL_VARNAME_ARRAY(BOOST_PP_TUPLE_ELEM(0, data), name, BOOST_PP_TUPLE_ELEM(1, data))
#define __PROGMEM_VARNAME_SEQ_DECL(r, data, name)               PROGMEM_DEF_PROGMEM_VARNAME_ARRAY(BOOST_PP_TUPLE_ELEM(0, data), name, BOOST_PP_TUPLE_ELEM(1, data))

// PROGMEM_DEF_LOCAL_VARNAMES(_VAR_, 2, (aa)(bb))
// const char *_VAR_id[2] = { PSTR("aa_0"), PSTR("aa_1") };
// const char *_VAR_id[2] = { PSTR("bb_0"), PSTR("bb_1") };
#define PROGMEM_DEF_LOCAL_VARNAMES(prefix, num, names)          BOOST_PP_SEQ_FOR_EACH(__LOCAL_VARNAME_SEQ_DECL, (prefix, num), names)

// same as PROGMEM_DEF_LOCAL_VARNAMES but static and PROGMEM
#define PROGMEM_DEF_PROGMEM_VARNAMES(prefix, num, names)        BOOST_PP_SEQ_FOR_EACH(__PROGMEM_VARNAME_SEQ_DECL, (prefix, num), names)

// helpers to get the variable names
// for PROGMEM_DEF_LOCAL_VARNAMES(_VAR_, 2, (aa)(bb))
// F_VAR(aa, 1) would return _VAR_aa[1] == (const __FlashStringHelper *)("aa_1")
#define F_VAR(name, num)									    reinterpret_cast<const __FlashStringHelper *>(BOOST_PP_CAT(_VAR_,name)[num])
#define PSTR_VAR(name, num)									    reinterpret_cast<const char *>(BOOST_PP_CAT(_VAR_,name)[num])

// version for PROGMEM arrays
#define F_PVAR(name, num)									    reinterpret_cast<const __FlashStringHelper *>( pgm_read_ptr_aligned(BOOST_PP_CAT(_VAR_,name)[num]) )
#define PSTR_PVAR(name, num)	    						    reinterpret_cast<const char *>( pgm_read_ptr_aligned(BOOST_PP_CAT(_VAR_,name)[num]) )
