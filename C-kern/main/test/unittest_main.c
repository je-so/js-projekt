/* title: Unittest-Main

   Main driver to execute all unittest.

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
