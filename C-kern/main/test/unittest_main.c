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
#include "C-kern/api/test/run/unittest.h"

int main(int argc, char* argv[])
{
   (void) argc ;
   (void) argv ;
   return run_unittest() ;
}
