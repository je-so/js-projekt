/* title: CompiletimeTest StdTypes
   Test size and signedness of standard types like size_t, int16_t, ...

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2011 Jörg Seebohn

   file: C-kern/test/compiletime/stdtypes.h
    Header file of <CompiletimeTest StdTypes>.

   file: C-kern/api/test/compiletime.h
    Included from <Compiletime-Tests>.
*/
#ifndef CKERN_TEST_COMPILETIME_STANDARDTYPES_HEADER
#define CKERN_TEST_COMPILETIME_STANDARDTYPES_HEADER

static inline void compiletime_tests_standardtypes(void)
{

   /* about: sizeof(char) Test
    * Asserts that sizeof(char) is 1 byte.
    * Some components managing space rely uppon
    * the fact that sizeof(cahr) is one.
    * Mostly used as a "+ 1" in sizeof calculations for storage size of strings.
    * Or in calculations where a string ius allocated after the
    * a structure:
    * > size_t memsize = sizeof(type_t) + strlen(string) + 1 */
   static_assert( sizeof(char) == 1, "calculations depends on char == 8bit") ;
   static_assert( CHAR_BIT     == 8, "calculations depends on char == 8bit") ;

   /* about: int_t Test
    * Asserts that all integer standard types
    * have the correct size and signedness. */
   static_assert( sizeof(int8_t) == 1,   "wrong size") ;
   static_assert( sizeof(uint8_t) == 1,  "wrong size") ;
   static_assert( sizeof(int16_t) == 2,  "wrong size") ;
   static_assert( sizeof(uint16_t) == 2, "wrong size") ;
   static_assert( sizeof(int32_t) == 4,  "wrong size") ;
   static_assert( sizeof(uint32_t) == 4, "wrong size") ;
   static_assert( sizeof(int64_t) == 8,  "wrong size") ;
   static_assert( sizeof(uint64_t) == 8, "wrong size") ;
   static_assert( sizeof(uintptr_t) == sizeof(void*), "must be size of a pointer") ;
   static_assert( sizeof(intptr_t) == sizeof(void*), "must be size of a pointer") ;
   static_assert( ((int8_t)-1) < 0,  "must be signed") ;
   static_assert( ((uint8_t)-1) > 0, "must be unsigned") ;
   static_assert( ((uint8_t)-1) == 255, "must be unsigned") ;
   static_assert( ((int16_t)-1) < 0,   "must be signed") ;
   static_assert( ((uint16_t)-1) > 0,  "must be unsigned") ;
   static_assert( ((uint16_t)-1) == 65535,  "must be unsigned") ;
   static_assert( ((int32_t)-1) < 0,   "must be signed") ;
   static_assert( ((uint32_t)-1) > 0,  "must be unsigned") ;
   static_assert( ((uint32_t)-1) == 4294967295,  "must be unsigned") ;
   static_assert( ((int64_t)-1) < 0,   "must be signed") ;
   static_assert( ((uint64_t)-1) > 0,  "must be unsigned") ;
   static_assert( ((uint64_t)-1) == 0xffffffffffffffff,  "must be unsigned") ;
   static_assert( ((intptr_t)-1) < 0,  "must be signed") ;
   static_assert( ((uintptr_t)-1) > 0, "must be unsigned") ;
   // uintptrf_t
   static_assert( ((uintptrf_t)-1) > 0,  "must be unsigned") ;
   static_assert( sizeof(uintptrf_t) == sizeof(void(*)(void)), "can hold a function pointer") ;
   // off_t
   static_assert( sizeof(off_t) == sizeof(int64_t), "need 64bit file-system support");
   static_assert( sizeof(off_t) >= sizeof(size_t), "memory is not bigger than max filesize") ;
   static_assert( ((off_t)-1) < 0, "off_t must be signed") ;
   static_assert( (ramsize_t)-1 > 0,  "must be unsigned") ;
   // ramsize_t
   static_assert( sizeof(ramsize_t) >= sizeof(size_t),   "must be >= size_t") ;
   static_assert( sizeof(ramsize_t) >= sizeof(uint64_t), "must support more than 4GB of ram") ;
   // size_t
   static_assert( ((size_t)-1) > 0, "must be unsigned") ;
   // ssize_t
   static_assert( ((ssize_t)-1) < 0, "must be signed") ;
   static_assert( sizeof(ssize_t) == sizeof(size_t), "ssize_t is same type as size_t but signed") ;

   /* about: limit Test
    * Asserts that all application defined limits (<OFF_MAX>)
    * of the standard types have the correct value. */
   static_assert( OFF_MAX == INT64_MAX, "OFF_MAX has correct value") ;

}
#endif
