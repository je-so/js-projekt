/* title: TestModule

   Simple module which is loaded during runtime.
   Certain restrictions apply to what you can do and what not.

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
      goto ONABORT ;
   }

   err = init_testmodulefunctable(functable) ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   return err ;
}
