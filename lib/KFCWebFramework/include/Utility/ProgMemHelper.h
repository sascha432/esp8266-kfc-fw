
/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <pgmspace.h>
#include <boost/preprocessor/arithmetic/div.hpp>
#include <boost/preprocessor/arithmetic/mul.hpp>
#include <boost/preprocessor/comparison/greater.hpp>
#include <boost/preprocessor/control/expr_if.hpp>
#include <boost/preprocessor/punctuation/comma_if.hpp>
#include <boost/preprocessor/repetition/repeat_from_to.hpp>
#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/variadic/size.hpp>
#include <boost/preprocessor/variadic/to_seq.hpp>
#include <boost/preprocessor/variadic/to_tuple.hpp>

// utility for creating arrays with variable names for forms in PROGMEM

// PROGMEM_DEF_LOCAL_VARNAME_ARRAY(_VAR_, id, 3)
// const char *_VAR_id[] = { PSTR("id_0"), PSTR("id_1"), PSTR("id_2") };
#define PROGMEM_DEF_LOCAL_VARNAME_ARRAY(prefix, name, num)      const char *prefix##name[] = { PSTR_NAME_ARRAY_INDEXED(0,num,"_",name) };
//PROGMEM_DEF_LOCAL_VARNAME_ARRAY(_VAR_, id, 3);

// PROGMEM_DEF_LOCAL_VARNAMES(_VAR_, 2, aa, bb, cc)
// const char *_VAR_aa[] = { PSTR("aa_0"), PSTR("aa_1") };
// const char *_VAR_bb[] = { PSTR("bb_0"), PSTR("bb_1") };
// const char *_VAR_cc[] = { PSTR("cc_0"), PSTR("cc_1") };
//#define PROGMEM_DEF_LOCAL_VARNAMES(prefix, num, ...)			        BOOST_PP_REPEAT_FROM_TO(0,BOOST_PP_VARIADIC_SIZE(__VA_ARGS__),__LOCAL_VARNAME_SEQ_DECL,(prefix,num,BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__)))
#define PROGMEM_DEF_LOCAL_VARNAMES(prefix, num, ...)			        BOOST_PP_SEQ_FOR_EACH(__LOCAL_VARNAME_SEQ_1,(prefix,num),BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__))
//PROGMEM_DEF_LOCAL_VARNAMES(_VAR_, 2, aa, bb, cc);

// helpers to get the variable names
// for PROGMEM_DEF_LOCAL_VARNAMES(_VAR_, 2, aa, bb)
// F_VAR(aa, 1) would return _VAR_aa[1] == (const __FlashStringHelper *)("aa_1")
#define F_VAR(name, num)									    reinterpret_cast<const __FlashStringHelper *>(BOOST_PP_CAT(_VAR_,name)[num])
#define PSTR_VAR(name, num)									    reinterpret_cast<const char *>(BOOST_PP_CAT(_VAR_,name)[num])

// helper for creating local PSTR arrays
// PROGMEM_DEF_LOCAL_PSTR_ARRAY(my_list, "1", "2", "3");
#define PROGMEM_DEF_LOCAL_PSTR_ARRAY(name, ...)			        static constexpr uint8_t k##name##Size = BOOST_PP_VARIADIC_SIZE(__VA_ARGS__); \
                                                                const char *name[] = { __VA_ARGS__ };
//PROGMEM_DEF_LOCAL_PSTR_ARRAY(my_list, "1", "2", "3");


#define PSTR_NAME_ARRAY_INDEXED(from, to, sep, text)            BOOST_PP_REPEAT_FROM_TO(from,to,__NAME_WITH_INDEX_DECL,BOOST_PP_VARIADIC_TO_TUPLE(from,to,BOOST_PP_SUB(to,from),sep,text))
//PSTR_NAME_ARRAY_INDEXED(5, 7, "_", GPIO);

#define PROGMEM_DEF_LOCAL_PSTR_ARRAY_INDEXED(name, from, to, separator, text) \
                                                                PROGMEM_DEF_LOCAL_PSTR_ARRAY(name,PSTR_NAME_ARRAY_INDEXED(from,BOOST_PP_INC(to),separator,text))
//PROGMEM_DEF_LOCAL_PSTR_ARRAY_INDEXED(pinNames, 0, 17, "", GPIO);




// helpers macros

#define __NAME_WITH_INDEX(n, s, t)                              PSTR(_STRINGIFY(t) s _STRINGIFY(n))
#define __NAME_WITH_INDEX_COMMA(n, d)                           BOOST_PP_COMMA_IF(BOOST_PP_SUB(n, BOOST_PP_TUPLE_ELEM(0, d)))
#define __NAME_WITH_INDEX_DECL(z, n, d)                         __NAME_WITH_INDEX_COMMA(n,d) __NAME_WITH_INDEX(n,BOOST_PP_TUPLE_ELEM(3,d),BOOST_PP_TUPLE_ELEM(4,d))

#define __LOCAL_VARNAME_SEQ_2(p, n, c)                          PROGMEM_DEF_LOCAL_VARNAME_ARRAY(p,n,c)
#define __LOCAL_VARNAME_SEQ_1(r, d, e)                          __LOCAL_VARNAME_SEQ_2(BOOST_PP_TUPLE_ELEM(0,d),e,BOOST_PP_TUPLE_ELEM(1,d))
//#define __LOCAL_VARNAME_SEQ_DECL(r, n, d)                       BOOST_PP_SEQ_FOR_EACH(__LOCAL_VARNAME_SEQ_1,(BOOST_PP_TUPLE_ELEM(0,d),BOOST_PP_TUPLE_ELEM(1,d)),BOOST_PP_TUPLE_ELEM(2,d))


