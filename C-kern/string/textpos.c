/* title: TextPosition impl

   Implements <TextPosition>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2013 Jörg Seebohn

   file: C-kern/api/string/textpos.h
    Header file <TextPosition>.

   file: C-kern/string/textpos.c
    Implementation file <TextPosition impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/string/textpos.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#endif



// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   textpos_t txtpos = textpos_FREE ;

   // TEST textpos_FREE
   TEST(0 == txtpos.column) ;
   TEST(0 == txtpos.line) ;
   TEST(0 == txtpos.prevlastcolumn) ;

   // TEST textpos_INIT
   txtpos = (textpos_t) textpos_INIT ;
   TEST(0 == txtpos.column) ;
   TEST(1 == txtpos.line) ;
   TEST(0 == txtpos.prevlastcolumn) ;

   // TEST init_textpos, free_textpos
   for (unsigned i = 0; i < 15; ++i) {
      init_textpos(&txtpos, 2*i, i+1) ;
      TEST(2*i == txtpos.column) ;
      TEST(i+1 == txtpos.line) ;
      TEST(0   == txtpos.prevlastcolumn) ;
      txtpos.prevlastcolumn = 1 ;
      free_textpos(&txtpos) ;
      TEST(0 == txtpos.column) ;
      TEST(0 == txtpos.line) ;
      TEST(0 == txtpos.prevlastcolumn) ;
   }

   // TEST column_textpos, line_textpos
   for (unsigned i = 0; i < 15; ++i) {
      txtpos.column = i ;
      txtpos.line   = i+1 ;
      txtpos.prevlastcolumn = i+2 ;
      TEST(i   == column_textpos(&txtpos)) ;
      TEST(i+1 == line_textpos(&txtpos)) ;
      TEST(i+2 == prevlastcolumn_textpos(&txtpos)) ;
   }

   return 0 ;
ONERR:
   return EINVAL ;
}

static int test_change(void)
{
   textpos_t txtpos = textpos_FREE ;

   // TEST addcolumn_textpos
   for (unsigned i = 1; i < 15; ++i) {
      size_t oldline = line_textpos(&txtpos) ;
      size_t oldcol  = column_textpos(&txtpos) ;
      TEST(oldcol+i == addcolumn_textpos(&txtpos, i)) ;
      TEST(oldcol+i == column_textpos(&txtpos)) ;
      TEST(oldline  == line_textpos(&txtpos)) ;
   }

   // TEST incrcolumn_textpos
   for (unsigned i = 0; i < 15; ++i) {
      size_t oldline = line_textpos(&txtpos) ;
      size_t oldcol  = column_textpos(&txtpos) ;
      incrcolumn_textpos(&txtpos) ;
      TEST(oldcol+1 == column_textpos(&txtpos)) ;
      TEST(oldline  == line_textpos(&txtpos)) ;
   }

   // TEST incrline_textpos
   for (unsigned i = 0; i < 15; ++i) {
      size_t old = line_textpos(&txtpos) ;
      txtpos.column = 100+i ;
      incrline_textpos(&txtpos) ;
      TEST(0     == column_textpos(&txtpos)) ;
      TEST(old+1 == line_textpos(&txtpos)) ;
      TEST(100+i == prevlastcolumn_textpos(&txtpos)) ;
   }

   return 0 ;
ONERR:
   return EINVAL ;
}

int unittest_string_textpos()
{
   if (test_initfree())    goto ONERR;
   if (test_change())      goto ONERR;

   return 0 ;
ONERR:
   return EINVAL ;
}

#endif
