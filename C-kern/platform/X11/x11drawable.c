/* title: X11-Drawable impl

   Implements <X11-Drawable>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

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
#include STR(C-kern/api/platform/KONFIG_OS/graphic/sysx11.h)


// section: x11drawable_t

// group: helper

static inline void compiletime_assert(void)
{
   static_assert(sizeof(((x11drawable_t*)0)->sys_drawable) == sizeof(Window),
                 "external visible handle has same size as internal X11 Window handle");
   static_assert(sizeof(((x11drawable_t*)0)->sys_colormap) == sizeof(Colormap),
                 "external visible handle has same size as internal X11 Colormap handle");
}



// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   x11drawable_t x11draw = x11drawable_FREE;

   // TEST x11drawable_FREE
   TEST(x11draw.display      == 0);
   TEST(x11draw.sys_drawable == 0);
   TEST(x11draw.sys_colormap == 0);

   // TEST x11drawable_INIT
   x11draw = (x11drawable_t) x11drawable_INIT((x11display_t*)1, 2, 3);
   TEST(x11draw.display      == (x11display_t*)1);
   TEST(x11draw.sys_drawable == 2);
   TEST(x11draw.sys_colormap == 3);

   return 0;
ONERR:
   return EINVAL;
}

static int test_query(void)
{
   x11drawable_t x11draw = x11drawable_FREE;

   // TEST genericcast_x11drawable
   TEST(&x11draw == genericcast_x11drawable(&x11draw));

   return 0;
ONERR:
   return EINVAL;
}

int unittest_platform_X11_x11drawable()
{
   if (test_initfree())    goto ONERR;
   if (test_query())       goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
