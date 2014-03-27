/* title: Graphic-Window-Configuration impl

   Implements <Graphic-Window-Configuration>.

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
   (C) 2014 Jörg Seebohn

   file: C-kern/api/graphic/windowconfig.h
    Header file <Graphic-Window-Configuration>.

   file: C-kern/graphic/windowconfig.c
    Implementation file <Graphic-Window-Configuration impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/graphic/windowconfig.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#endif



// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   const char * title = "test-title";
   windowconfig_t winconf[] = {
      windowconfig_INIT_FRAME,
      windowconfig_INIT_MINSIZE(1, 2),
      windowconfig_INIT_MAXSIZE(3, 4),
      windowconfig_INIT_POS(123, 456),
      windowconfig_INIT_POS(-123, -456),
      windowconfig_INIT_SIZE(234, 567),
      windowconfig_INIT_TITLE(title),
      windowconfig_INIT_TRANSPARENCY(255),
      windowconfig_INIT_NONE
   };

   for (unsigned i = 0; i < lengthof(winconf); ++i) {
      switch (i) {
      // TEST windowconfig_INIT_FRAME
      case 0:  TEST(winconf[i].i32 == windowconfig_FRAME);  break;
      // TEST windowconfig_INIT_MINSIZE
      case 1:  TEST(winconf[i].i32 == windowconfig_MINSIZE); break;
      case 2:  TEST(winconf[i].u16 == 1);                   break;
      case 3:  TEST(winconf[i].u16 == 2);                   break;
      // TEST windowconfig_INIT_MAXSIZE
      case 4:  TEST(winconf[i].i32 == windowconfig_MAXSIZE); break;
      case 5:  TEST(winconf[i].u16 == 3);                   break;
      case 6:  TEST(winconf[i].u16 == 4);                   break;
      // TEST windowconfig_INIT_POS
      case 7:  TEST(winconf[i].i32 == windowconfig_POS);    break;
      case 8:  TEST(winconf[i].i32 == 123);                 break;
      case 9:  TEST(winconf[i].i32 == 456);                 break;
      case 10: TEST(winconf[i].i32 == windowconfig_POS);    break;
      case 11: TEST(winconf[i].i32 == -123);                break;
      case 12: TEST(winconf[i].i32 == -456);                break;
      // TEST windowconfig_INIT_SIZE
      case 13: TEST(winconf[i].i32 == windowconfig_SIZE);   break;
      case 14: TEST(winconf[i].u16 == 234);                 break;
      case 15: TEST(winconf[i].u16 == 567);                 break;
      // TEST windowconfig_INIT_TITLE
      case 16: TEST(winconf[i].i32 == windowconfig_TITLE);  break;
      case 17: TEST(winconf[i].str == title);               break;
      // TEST windowconfig_INIT_TRANSPARENCY
      case 18: TEST(winconf[i].i32 == windowconfig_TRANSPARENCY); break;
      case 19: TEST(winconf[i].u8  == 255);                 break;
      // TEST windowconfig_INIT_NONE
      case 20: TEST(winconf[i].i32 == windowconfig_NONE);
               static_assert(lengthof(winconf) == 21, "encoding uses that many entries");
               break;
      default: TEST(false); break;
      }
   }

   return 0;
ONABORT:
   return EINVAL;
}

static int test_query(void)
{
   const char * title = "test-title";
   const windowconfig_t winconf[] = {
      windowconfig_INIT_FRAME,
      windowconfig_INIT_TRANSPARENCY(127),
      windowconfig_INIT_POS(1, 2),
      windowconfig_INIT_TITLE(title),
      windowconfig_INIT_SIZE(5, 6),
      windowconfig_INIT_MINSIZE(3, 4),
      windowconfig_INIT_MAXSIZE(9, 8),
      windowconfig_INIT_NONE
   };
   uint_fast8_t ai = 0;

   // TEST readtype_windowconfig
   TEST(windowconfig_FRAME == readtype_windowconfig(winconf, &ai));
   TEST(1 == ai);

   // TEST readtransparency_windowconfig
   TEST(windowconfig_TRANSPARENCY == readtype_windowconfig(winconf, &ai));
   TEST(2 == ai);
   TEST(127 == readtransparency_windowconfig(winconf, &ai));
   TEST(3 == ai);

   // TEST readpos_windowconfig
   TEST(windowconfig_POS == readtype_windowconfig(winconf, &ai));
   TEST(4 == ai);
   int32_t x = 0;
   int32_t y = 0;
   readpos_windowconfig(winconf, &ai, &x, &y);
   TEST(6 == ai);
   TEST(1 == x);
   TEST(2 == y);

   // TEST readtitle_windowconfig
   TEST(windowconfig_TITLE == readtype_windowconfig(winconf, &ai));
   TEST(7 == ai);
   TEST(title == readtitle_windowconfig(winconf, &ai));
   TEST(8 == ai);

   // TEST readsize_windowconfig
   TEST(windowconfig_SIZE == readtype_windowconfig(winconf, &ai));
   TEST(9 == ai);
   uint32_t width = 0;
   uint32_t height = 0;
   readsize_windowconfig(winconf, &ai, &width, &height);
   TEST(11 == ai);
   TEST(5 == width);
   TEST(6 == height);

   // TEST readminsize_windowconfig
   TEST(windowconfig_MINSIZE == readtype_windowconfig(winconf, &ai));
   TEST(12 == ai);
   readminsize_windowconfig(winconf, &ai, &width, &height);
   TEST(14 == ai);
   TEST(3 == width);
   TEST(4 == height);

   // TEST readmaxsize_windowconfig
   TEST(windowconfig_MAXSIZE == readtype_windowconfig(winconf, &ai));
   TEST(15 == ai);
   readmaxsize_windowconfig(winconf, &ai, &width, &height);
   TEST(17 == ai);
   TEST(9 == width);
   TEST(8 == height);

   // TEST readtype_windowconfig
   TEST(windowconfig_NONE == readtype_windowconfig(winconf, &ai));
   TEST(18 == ai);

   return 0;
ONABORT:
   return EINVAL;
}

int unittest_graphic_windowconfig()
{
   if (test_initfree())    goto ONABORT;
   if (test_query())       goto ONABORT;

   return 0;
ONABORT:
   return EINVAL;
}

#endif
