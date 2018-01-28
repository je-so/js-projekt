/* title: CompiletimeTest StdMacros
   Test size and signedness of standard types like size_t, int16_t, ...

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2018 Jörg Seebohn

   file: C-kern/test/compiletime/stdmacros.h
    Header file of <CompiletimeTest StdMacros>.

   file: C-kern/api/stdtypes/stdmacros.h
    Header file <Standard-Macros>.
*/
#ifndef CKERN_TEST_COMPILETIME_STANDARDMACROS_HEADER
#define CKERN_TEST_COMPILETIME_STANDARDMACROS_HEADER

static inline void compiletime_tests_standardmacros(void)
{
   // TEST nrargsof
   static_assert(9 == nrargsof(1,2,3,4,5,6,7,8,9), "nrargsof with 9 parmaeters");
   static_assert(8 == nrargsof(1,2,3,4,5,6,7,8), "nrargsof with 8 parameters");
   static_assert(7 == nrargsof(1,2,3,4,5,6,7), "nrargsof with 7 parameters");
   static_assert(6 == nrargsof(1,2,3,4,5,6), "nrargsof with 6 parameters");
   static_assert(5 == nrargsof(1,2,3,4,5), "nrargsof with 5 parameters");
   static_assert(4 == nrargsof(1,2,3,4), "nrargsof with 4 parameters");
   static_assert(3 == nrargsof(1,2,3), "nrargsof with 3 parameters");
   static_assert(2 == nrargsof(1,2), "nrargsof with 2 parameters");
   static_assert(1 == nrargsof(1), "nrargsof with 1 parameter");
   static_assert(0 == nrargsof(), "nrargsof with 0 parameters");
}

#endif