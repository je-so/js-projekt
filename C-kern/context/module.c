/* title: Module impl

   Implements <Module>.

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
   (C) 2013 Jörg Seebohn

   file: C-kern/api/context/module.h
    Header file <Module>.

   file: C-kern/context/module.c
    Implementation file <Module impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/context/module.h"
#include "C-kern/api/err.h"
#include "C-kern/api/io/accessmode.h"
#include "C-kern/api/io/filesystem/directory.h"
#include "C-kern/api/io/filesystem/mmfile.h"
#include "C-kern/api/memory/memblock.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/memory/vm.h"
#include "C-kern/main/test/helper/testmodule_helper1.h"
#endif


// section: module_t

// group: static configuration

/* define: module_DIRECTORY
 * The name of the directory the loadable module binary blobs are stored.
 * Use a relative name which depends on the working directory
 * if you want to be independant of the current installation directory.
 * This value can be overwritten in C-kern/resource/config/modulevalues. */
#define module_DIRECTORY  "bin/mod/"

// TEXTDB:SELECT('#undef  module_'name\n'#define module_'name'  "'value'"')FROM("C-kern/resource/config/modulevalues")WHERE(module=="module_t")
#undef  module_DIRECTORY
#define module_DIRECTORY  "bin/mod/"
// TEXTDB:END

// group: lifetime

int init_module(/*out*/module_t * mod, const char * modulename)
{
   int err ;
   directory_t *  dir    = 0 ;
   mmfile_t       mmfile = mmfile_INIT_FREEABLE ;

   err = new_directory(&dir, module_DIRECTORY, 0) ;
   if (err) goto ONABORT ;
   err = init_mmfile(&mmfile, modulename, 0, 0, accessmode_RDEX_SHARED, dir) ;
   if (err) goto ONABORT ;
   err = delete_directory(&dir) ;
   if (err) goto ONABORT ;

   initmove_mmfile(genericcast_mmfile(mod, code_, ), &mmfile) ;

   return 0 ;
ONABORT:
   free_mmfile(&mmfile) ;
   delete_directory(&dir) ;
   TRACEABORT_LOG(err) ;
   return err ;
}

int free_module(module_t * mod)
{
   int err ;

   err = free_mmfile(genericcast_mmfile(mod, code_, )) ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   module_t       mod    = module_INIT_FREEABLE ;
   const char *   name[] = { "testmodule", "testmodule_Debug" } ;
   directory_t *  dir ;

   // prepare
   TEST(0 == new_directory(&dir, module_DIRECTORY, 0)) ;

   // TEST module_INIT_FREEABLE
   TEST(0 == mod.code_addr) ;
   TEST(0 == mod.code_size) ;

   for (unsigned i = 0; i < lengthof(name); ++i) {
      off_t code_size = 0 ;
      TEST(0 == filesize_directory(dir, name[i], &code_size)) ;

      // TEST init_module
      TEST(0 == init_module(&mod, name[i])) ;
      TEST(mod.code_addr != 0) ;
      TEST(mod.code_size == code_size) ;
      TEST(1 == ismapped_vm(genericcast_vmpage(&mod, code_), accessmode_RDEX_SHARED)) ;

      // TEST free_module
      TEST(0 == free_module(&mod)) ;
      TEST(0 == mod.code_addr) ;
      TEST(0 == mod.code_size) ;
      TEST(1 == isunmapped_vm(genericcast_vmpage(&mod, code_))) ;
      TEST(0 == free_module(&mod)) ;
      TEST(0 == mod.code_addr) ;
      TEST(0 == mod.code_size) ;
      TEST(1 == isunmapped_vm(genericcast_vmpage(&mod, code_))) ;
   }

   // unprepare
   TEST(0 == delete_directory(&dir)) ;

   return 0 ;
ONABORT:
   delete_directory(&dir) ;
   free_module(&mod) ;
   return EINVAL ;
}

static int test_query(void)
{
   module_t mod ;

   // TEST codeaddr_module
   for (intptr_t i = 10; i >= 0; --i) {
      mod.code_addr = (uint8_t*) i ;
      TEST(i == (intptr_t)codeaddr_module(&mod)) ;
   }

   // TEST codesize_module
   for (size_t i = 10; i <= 10; --i) {
      mod.code_size = i ;
      TEST(i == codesize_module(&mod)) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_generic(void)
{
   module_t mod ;

   // TEST genericcast_memblock: code_addr, code_size compatible with memblock_t
   TEST((memblock_t*)&mod == genericcast_memblock(&mod, code_)) ;

   // TEST genericcast_mmfile: code_addr, code_size compatible with mmfile_t
   TEST((mmfile_t*)&mod == genericcast_mmfile(&mod, code_,)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_exec(void)
{
   module_t       mod    = module_INIT_FREEABLE ;
   const char *   name[] = { "testmodule", "testmodule_Debug" } ;


   for (unsigned i = 0; i < lengthof(name); ++i) {
      // prepare
      TEST(0 == init_module(&mod, name[i])) ;
      TEST(0 != mod.code_addr) ;
      TEST(0 != mod.code_size) ;

      // TEST module_main: execute
      typedef int (*module_main_f) (testmodule_functable_t * functable, threadcontext_t * tcontext) ;

      module_main_f           mmain = (module_main_f) (uintptr_t) codeaddr_module(&mod) ;
      testmodule_functable_t  table = { 0, 0, 0 } ;

      TEST(0 == mmain(&table, tcontext_maincontext())) ;
      TEST(codeaddr_module(&mod) < (uint8_t*)(uintptr_t)table.add && (uint8_t*)(uintptr_t)table.add < codeaddr_module(&mod) + codesize_module(&mod)) ;
      TEST(codeaddr_module(&mod) < (uint8_t*)(uintptr_t)table.sub && (uint8_t*)(uintptr_t)table.sub < codeaddr_module(&mod) + codesize_module(&mod)) ;
      TEST(codeaddr_module(&mod) < (uint8_t*)(uintptr_t)table.mult && (uint8_t*)(uintptr_t)table.mult < codeaddr_module(&mod) + codesize_module(&mod)) ;

      // TEST testmodule_functable_t: execute functions
      TEST(5 == table.add(3,2)) ;
      TEST(9 == table.sub(11,2)) ;
      TEST(8 == table.mult(2,4)) ;

      // unprepare
      TEST(0 == free_module(&mod)) ;
      TEST(0 == mod.code_addr) ;
      TEST(0 == mod.code_size) ;
   }

   return 0 ;
ONABORT:
   free_module(&mod) ;
   return EINVAL ;
}

int unittest_context_module()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())    goto ONABORT ;
   if (test_query())       goto ONABORT ;
   if (test_generic())     goto ONABORT ;
   if (test_exec())        goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
