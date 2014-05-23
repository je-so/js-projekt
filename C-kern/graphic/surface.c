/* title: Graphic-Surface impl

   Implements <Graphic-Surface>.

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

   file: C-kern/api/graphic/surface.h
    Header file <Graphic-Surface>.

   file: C-kern/graphic/surface.c
    Implementation file <Graphic-Surface impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/graphic/surface.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#endif


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   surface_t surf = surface_FREE;

   // TEST surface_FREE
   TEST(0 == surf.glsurface);

   // TEST gl_surface: glsurface == 0
   TEST(0 == gl_surface(&surf));

   // TEST isfree_surface: glsurface == 0
   TEST(1 == isfree_surface(&surf));

   for (uintptr_t i = 1; i; i <<= 1) {
      // TEST gl_surface: glsurface != 0
      surf.glsurface = (struct opengl_surface_t*)i;
      TEST((void*)i == gl_surface(&surf));

      // TEST isfree_surface: glsurface != 0
      TEST(0 == isfree_surface(&surf));
   }

   return 0;
ONABORT:
   return EINVAL;
}

static int test_generic(void)
{
   struct {
      int x;
      surface_EMBED;
      int y;
   } surf = { 0, surface_FREE_EMBEDDED, 0 };

   // TEST surface_FREE_EMBEDDED
   TEST(0 == surf.x);
   TEST(0 == surf.glsurface);
   TEST(0 == surf.y);

   // TEST gl_surface
   TEST(0 == gl_surface(&surf));
   for (uintptr_t i = 1; i; i <<= 1) {
      surf.glsurface = (struct opengl_surface_t*)i;
      TEST((void*)i == gl_surface(&surf));
      TEST(0 == surf.x);
      TEST(0 == surf.y);
   }

   return 0;
ONABORT:
   return EINVAL;
}

int unittest_graphic_surface()
{
   if (test_initfree())    goto ONABORT;
   if (test_generic())     goto ONABORT;

   return 0;
ONABORT:
   return EINVAL;
}

#endif
