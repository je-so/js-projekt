/* title: Unittest-Main

   Main driver to execute all unittest.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2012 Jörg Seebohn

   file: C-kern/main/test/unittest_main.c
    Implementation file of <Unittest-Main>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/platform/init.h"
// *** DO COMPILETIME tests (include is enough) ***
#include "C-kern/api/test/assert.h"
#include "C-kern/api/test/compiletime.h"
// *** ***
#include "C-kern/api/test/run/run_unittest.h"

int main(int argc, const char* argv[])
{
   (void) argc;
   int err;
   err = init_platform(run_unittest, argv);
   return err;
}
