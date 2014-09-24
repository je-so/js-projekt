/* title: TestModule

   Simple module which is loaded during runtime.
   Certain restrictions apply to what you can do and what not.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2013 Jörg Seebohn

   file: C-kern/main/test/testmodule_main.c
    Implementation file <TestModule>.
*/

#include "C-kern/konfig.h"
#include "C-kern/main/test/helper/testmodule_helper1.h"


int main_module(/*out*/testmodule_functable_t * functable, threadcontext_t * tcontext)
{
   int err ;

   if (tcontext_maincontext() != tcontext) {
      err = EINVAL ;
      goto ONERR;
   }

   err = init_testmodulefunctable(functable) ;
   if (err) goto ONERR;

   return 0 ;
ONERR:
   return err ;
}
