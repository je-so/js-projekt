/* title: X11-Drawable impl

   Implements <X11-Drawable>.

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

   file: C-kern/api/platform/X11/x11drawable.h
    Header file <X11-Drawable>.

   file: C-kern/platform/X11/x11drawable.c
    Implementation file <X11-Drawable impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/platform/X11/x11drawable.h"
#include "C-kern/api/err.h"
#include "C-kern/api/platform/X11/x11display.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#endif
#include "C-kern/api/platform/X11/x11syskonfig.h"


// section: x11drawable_t

// group: lifetime


// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   x11drawable_t x11draw = x11drawable_INIT_FREEABLE ;

   // TEST x11drawable_INIT_FREEABLE
   TEST(x11draw.display      == 0) ;
   TEST(x11draw.sys_drawable == 0) ;
   TEST(x11draw.sys_colormap == 0) ;

   // TEST x11drawable_INIT
   x11draw = (x11drawable_t) x11drawable_INIT((x11display_t*)1, 2, 3) ;
   TEST(x11draw.display      == (x11display_t*)1) ;
   TEST(x11draw.sys_drawable == 2) ;
   TEST(x11draw.sys_colormap == 3) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

int unittest_platform_X11_x11drawable()
{
   if (test_initfree())       goto ONABORT ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

#endif
