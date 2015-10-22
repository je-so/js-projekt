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

   maincontext_startparam_t startparam = maincontext_startparam_INIT(
      maincontext_CONSOLE, argc, argv, &run_perftest, 0
   );

   err = initrun_maincontext(&startparam);

   return err;
}
