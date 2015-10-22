/* title: Performance-Test
   Runs all tests which measures time.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2014 Jörg Seebohn

   file: C-kern/main/test/perftest_main.c
    Implements main() of <Performance-Test>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/test/run/run_perftest.h"


int main(int argc, const char* argv[])
{
   int err;

   err = initrun_maincontext(maincontext_CONSOLE, &run_perftest, 0, argc, argv);

   return err;
}
