/* title: CompiletimeTest SysTypes

   Test values of system types like <sys_ioblock_SIZE>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2014 Jörg Seebohn

   file: C-kern/test/compiletime/systypes.h
    Header file of <CompiletimeTest SysTypes>.

   file: C-kern/api/test/compiletime.h
    Included from <Compiletime-Tests>.
*/
#ifndef CKERN_TEST_COMPILETIME_SYSTEMTYPES_HEADER
#define CKERN_TEST_COMPILETIME_SYSTEMTYPES_HEADER

static inline void compiletime_tests_systemtypes(void)
{
   static_assert(0 == ((sys_ioblock_SIZE-1)&sys_ioblock_SIZE), "sys_ioblock_SIZE is power of two");
}

#endif