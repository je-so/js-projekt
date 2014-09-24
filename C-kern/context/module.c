/* title: Module impl

   Implements <Module>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

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
#include "C-kern/api/test/unittest.h"
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
#define module_DIRECTORY "bin/mod/"

// TEXTDB:SELECT('#undef  module_'name\n'#define module_'name'  "'value'"')FROM("C-kern/resource/config/modulevalues")WHERE(module=="module_t")
#undef  module_DIRECTORY
#define module_DIRECTORY  "bin/mod/"
// TEXTDB:END

// group: lifetime

int init_module(/*out*/module_t * mod, const char * modulename)
{
   int err ;
   directory_t *  dir    = 0 ;
   mmfile_t       mmfile = mmfile_FREE ;

   err = new_directory(&dir, module_DIRECTORY, 0) ;
   if (err) goto ONERR;
   err = init_mmfile(&mmfile, modulename, 0, 0, accessmode_RDEX_SHARED, dir) ;
   if (err) goto ONERR;
   err = delete_directory(&dir) ;
   if (err) goto ONERR;

   initmove_mmfile(cast_mmfile(mod, code_, ), &mmfile) ;

   return 0 ;
ONERR:
   free_mmfile(&mmfile) ;
   delete_directory(&dir) ;
   TRACEEXIT_ERRLOG(err);
   return err ;
}

int free_module(module_t * mod)
{
   int err ;

   err = free_mmfile(cast_mmfile(mod, code_, )) ;
   if (err) goto ONERR;

   return 0 ;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   module_t       mod    = module_FREE ;
   const char *   name[] = { "testmodule", "testmodule_Debug" } ;
   directory_t *  dir ;

   // prepare
   TEST(0 == new_directory(&dir, module_DIRECTORY, 0)) ;

   // TEST module_FREE
   TEST(0 == mod.code_addr) ;
   TEST(0 == mod.code_size) ;

   for (unsigned i = 0; i < lengthof(name); ++i) {
      off_t code_size = 0 ;
      TEST(0 == filesize_directory(dir, name[i], &code_size)) ;

      // TEST init_module
      TEST(0 == init_module(&mod, name[i])) ;
      TEST(mod.code_addr != 0) ;
      TEST(mod.code_size == code_size) ;
      TEST(1 == ismapped_vm(cast_vmpage(&mod, code_), accessmode_RDEX_SHARED)) ;

      // TEST free_module
      TEST(0 == free_module(&mod)) ;
      TEST(0 == mod.code_addr) ;
      TEST(0 == mod.code_size) ;
      TEST(1 == isunmapped_vm(cast_vmpage(&mod, code_))) ;
      TEST(0 == free_module(&mod)) ;
      TEST(0 == mod.code_addr) ;
      TEST(0 == mod.code_size) ;
      TEST(1 == isunmapped_vm(cast_vmpage(&mod, code_))) ;
   }

   // unprepare
   TEST(0 == delete_directory(&dir)) ;

   return 0 ;
ONERR:
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
ONERR:
   return EINVAL ;
}

static int test_generic(void)
{
   module_t mod ;

   // TEST cast_memblock: code_addr, code_size compatible with memblock_t
   TEST((memblock_t*)&mod == cast_memblock(&mod, code_)) ;

   // TEST cast_mmfile: code_addr, code_size compatible with mmfile_t
   TEST((mmfile_t*)&mod == cast_mmfile(&mod, code_,)) ;

   return 0 ;
ONERR:
   return EINVAL ;
}

static int test_exec(void)
{
   module_t       mod    = module_FREE ;
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
ONERR:
   free_module(&mod) ;
   return EINVAL ;
}

int unittest_context_module()
{
   if (test_initfree())    goto ONERR;
   if (test_query())       goto ONERR;
   if (test_generic())     goto ONERR;
   if (test_exec())        goto ONERR;

   return 0 ;
ONERR:
   return EINVAL ;
}

#endif
