/* title: TextPosition impl

   Implements <TextPosition>.

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

   file: C-kern/api/io/reader/util/textpos.h
    Header file <TextPosition>.

   file: C-kern/io/reader/util/textpos.c
    Implementation file <TextPosition impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/io/reader/util/textpos.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif



// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   textpos_t txtpos = textpos_INIT_FREEABLE ;

   // TEST textpos_INIT_FREEABLE
   TEST(0 == txtpos.column) ;
   TEST(0 == txtpos.line) ;

   // TEST textpos_INIT
   txtpos = (textpos_t) textpos_INIT ;
   TEST(0 == txtpos.column) ;
   TEST(1 == txtpos.line) ;

   // TEST init_textpos, free_textpos
   for (unsigned i = 0; i < 15; ++i) {
      init_textpos(&txtpos, 2*i, i+1) ;
      TEST(2*i == txtpos.column) ;
      TEST(i+1 == txtpos.line) ;
      free_textpos(&txtpos) ;
      TEST(0 == txtpos.column) ;
      TEST(0 == txtpos.line) ;
   }

   // TEST column_textpos, line_textpos
   for (unsigned i = 0; i < 15; ++i) {
      txtpos.column = i ;
      txtpos.line   = i+1 ;
      TEST(i   == column_textpos(&txtpos)) ;
      TEST(i+1 == line_textpos(&txtpos)) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_change(void)
{
   textpos_t txtpos = textpos_INIT_FREEABLE ;

   // TEST addcolumn_textpos
   for (unsigned i = 1; i < 15; ++i) {
      size_t oldline = line_textpos(&txtpos) ;
      size_t oldcol  = column_textpos(&txtpos) ;
      TEST(oldcol+i == addcolumn_textpos(&txtpos, i)) ;
      TEST(oldcol+i == column_textpos(&txtpos)) ;
      TEST(oldline  == line_textpos(&txtpos)) ;
   }

   // TEST nextcolumn_textpos
   for (unsigned i = 0; i < 15; ++i) {
      size_t oldline = line_textpos(&txtpos) ;
      size_t oldcol  = column_textpos(&txtpos) ;
      nextcolumn_textpos(&txtpos) ;
      TEST(oldcol+1 == column_textpos(&txtpos)) ;
      TEST(oldline  == line_textpos(&txtpos)) ;
   }

   // TEST nextline_textpos
   for (unsigned i = 0; i < 15; ++i) {
      size_t old = line_textpos(&txtpos) ;
      txtpos.column = 100+i ;
      nextline_textpos(&txtpos) ;
      TEST(0     == column_textpos(&txtpos)) ;
      TEST(old+1 == line_textpos(&txtpos)) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

int unittest_io_reader_util_textpos()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())    goto ONABORT ;
   if (test_change())      goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
