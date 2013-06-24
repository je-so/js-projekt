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

   file: C-kern/platform/shared/X11/x11screen.c
    Implementation file <X11-Screen impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/platform/X11/x11screen.h"
#include "C-kern/api/err.h"
#include "C-kern/api/platform/X11/x11display.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif
#include "C-kern/api/platform/X11/x11syskonfig.h"


// section: x11display_t

// group: query

uint16_t nrofscreens_x11display(const struct x11display_t * x11disp)
{
   return (uint16_t) ScreenCount(x11disp->sys_display) ;
}

x11screen_t defaultscreen_x11display(struct x11display_t * x11disp)
{
   return (x11screen_t) x11screen_INIT(x11disp, (uint16_t)DefaultScreen(x11disp->sys_display)) ;
}

uint16_t defaultnrscreen_x11display(const struct x11display_t * x11disp)
{
   return (uint16_t) DefaultScreen(x11disp->sys_display) ;
}

// section: x11screen_t

// group: lifetime

int init_x11screen(/*out*/x11screen_t * x11screen, x11display_t * display, uint16_t nrscreen)
{
   int err ;

   VALIDATE_INPARAM_TEST(nrscreen < nrofscreens_x11display(display), ONABORT, ) ;

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

static int test_ext_x11disp(x11display_t * x11disp)
{
   uint16_t nrofscreens ;

   // TEST nrofscreens_x11display
   nrofscreens = nrofscreens_x11display(x11disp) ;
   TEST(nrofscreens > 0) ;
   TEST(nrofscreens < 5/*could be more*/) ;

   // TEST defaultscreen_x11display
   x11screen_t x11screen = defaultscreen_x11display(x11disp) ;
   TEST(x11screen.display  == x11disp) ;
   TEST(x11screen.nrscreen == defaultnrscreen_x11display(x11disp)) ;

   // TEST defaultnrscreen_x11display
   TEST(0 == defaultnrscreen_x11display(x11disp)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

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

int unittest_platform_X11_x11screen()
{
   resourceusage_t   usage   = resourceusage_INIT_FREEABLE ;
   x11display_t      x11disp = x11display_INIT_FREEABLE ;

   TEST(0 == init_x11display(&x11disp, ":0.0")) ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_ext_x11disp(&x11disp))  goto ONABORT ;
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

#endif
