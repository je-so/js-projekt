/* title: X11-Screen impl

   Implements <X11-Screen>.

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

   file: C-kern/api/platform/X11/x11screen.h
    Header file <X11-Screen>.

   file: C-kern/platform/X11/x11screen.c
    Implementation file <X11-Screen impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/platform/X11/x11screen.h"
#include "C-kern/api/err.h"
#include "C-kern/api/platform/X11/x11display.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/resourceusage.h"
#include "C-kern/api/test/unittest.h"
#endif
#include STR(C-kern/api/platform/KONFIG_OS/graphic/sysx11.h)


// section: x11screen_t

// group: lifetime

int init_x11screen(/*out*/x11screen_t * x11screen, x11display_t * display, int32_t nrscreen)
{
   int err ;

   VALIDATE_INPARAM_TEST(0 <= nrscreen && nrscreen < nrofscreens_x11display(display), ONABORT, ) ;

   x11screen->display  = display ;
   x11screen->nrscreen = nrscreen ;

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

// group: query

bool isequal_x11screen(const x11screen_t * lx11screen, const x11screen_t * rx11screen)
{
   return   lx11screen->display     == rx11screen->display
            && lx11screen->nrscreen == rx11screen->nrscreen ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(x11display_t * x11disp)
{
   x11screen_t x11screen = x11screen_INIT_FREEABLE ;

   // TEST x11screen_INIT_FREEABLE
   TEST(0 == x11screen.display) ;
   TEST(0 == x11screen.nrscreen) ;

   // TEST init_x11screen
   x11screen.nrscreen = 1 ;
   TEST(0 == init_x11screen(&x11screen, x11disp, 0)) ;
   TEST(x11screen.display  == x11disp) ;
   TEST(x11screen.nrscreen == 0) ;

   // TEST init_x11screen: EINVAL
   TEST(EINVAL == init_x11screen(&x11screen, x11disp, nrofscreens_x11display(x11disp))) ;
   TEST(EINVAL == init_x11screen(&x11screen, x11disp, -1)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_query(void)
{
   x11screen_t lx11screen = x11screen_INIT_FREEABLE ;
   x11screen_t rx11screen = x11screen_INIT_FREEABLE ;

   // TEST display_x11screen
   for (uintptr_t i = 0; i < 15; ++i) {
      x11screen_t dummy ;
      dummy.display = (x11display_t*) i ;
      TEST(display_x11screen(&dummy) == dummy.display) ;
   }

   // TEST number_x11screen
   for (uint16_t i = 0; i < 15; ++i) {
      x11screen_t dummy ;
      dummy.nrscreen = i ;
      TEST(number_x11screen(&dummy) == dummy.nrscreen) ;
   }

   // TEST isequal_x11screen
   lx11screen.display  = (x11display_t*)1 ;
   TEST(!isequal_x11screen(&lx11screen, &rx11screen)) ;
   lx11screen.display  = (x11display_t*)0 ;
   TEST(isequal_x11screen(&rx11screen, &lx11screen)) ;
   lx11screen.nrscreen = 1 ;
   TEST(!isequal_x11screen(&lx11screen, &rx11screen)) ;
   lx11screen.nrscreen = 0 ;
   TEST(isequal_x11screen(&rx11screen, &lx11screen)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int childprocess_unittest(void)
{
   resourceusage_t   usage   = resourceusage_INIT_FREEABLE ;
   x11display_t      x11disp = x11display_INIT_FREEABLE ;

   TEST(0 == init_x11display(&x11disp, ":0.0")) ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree(&x11disp))     goto ONABORT ;
   if (test_query())                goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   TEST(0 == free_x11display(&x11disp)) ;

   return 0 ;
ONABORT:
   (void) free_x11display(&x11disp) ;
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

int unittest_platform_X11_x11screen()
{
   int err;

   TEST(0 == execasprocess_unittest(&childprocess_unittest, &err));

   return err;
ONABORT:
   return EINVAL;
}

#endif
