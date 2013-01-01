/* title: CompiletimeTest StdTypes
   Test size and signedness of standard types like size_t, int16_t, ...

   about: Copyright
   This program is free software.
   You can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   Author:
   (C) 2011 Jörg Seebohn

   file: C-kern/test/compiletime/stdtypes.h
    Header file of <CompiletimeTest StdTypes>.

   file: C-kern/api/test/compiletime.h
    Included from <Compiletime-Tests>.
*/
#ifndef CKERN_TEST_COMPILETIME_STANDARDTYPES_HEADER
#define CKERN_TEST_COMPILETIME_STANDARDTYPES_HEADER

static inline void compiletime_tests_standardtypes(void) {

   /* about: sizeof(char) Test
    * Asserts that sizeof(char) is 1 byte.
    * Some components managing space rely uppon
    * the fact that sizeof(cahr) is one.
    * Mostly used as a "+ 1" in sizeof calculations for storage size of strings.
    * Or in calculations where a string ius allocated after the
    * a structure:
    * > size_t memsize = sizeof(type_t) + strlen(string) + 1 */
   static_assert( sizeof(char) == 1, "wrong size") ;

   /* about: sizeof(intXX_t) Test
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
   static_assert( ((int8_t)-1) == -1,  "must be signed") ;
   static_assert( ((uint8_t)-1) == 255, "must be unsigned") ;
   static_assert( ((int16_t)-1) < 0,   "must be signed") ;
   static_assert( ((uint16_t)-1) > 0,  "must be unsigned") ;
   static_assert( ((int32_t)-1) < 0,   "must be signed") ;
   static_assert( ((uint32_t)-1) > 0,  "must be unsigned") ;
   static_assert( ((int64_t)-1) < 0,   "must be signed") ;
   static_assert( ((uint64_t)-1) > 0,  "must be unsigned") ;
   static_assert( ((uintptr_t)-1) > 0, "must be unsigned") ;
   static_assert( ((intptr_t)-1) < 0,  "must be signed") ;

   /* about: size_t Test
    * Test size_t is unsigned. */
   static_assert( ((size_t)-1) > 0, "must be unsigned") ;
   /* about: ssize_t Test
    * Test ssize_t is signed and same size as size_t. */
   static_assert( ((ssize_t)-1) < 0, "must be signed") ;
   static_assert( sizeof(ssize_t) == sizeof(size_t), "ssize_t is same type as size_t but signed") ;

   /* about: FileIO-64bit Test
    * Test that file I/O operations support 64 bit file size.
    * Uses standard type off_t for the check. */
   static_assert( sizeof(off_t) == sizeof(int64_t), "need 64bit file-system support");
   static_assert( sizeof(off_t) >= sizeof(size_t), "memory is not bigger than max filesize") ;
   static_assert( ((off_t)-1) < 0, "off_t must be signed") ;

}
#endif
