/* title: Graphic-Pixel-Buffer impl

   Implements <Graphic-Pixel-Buffer>.

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

   file: C-kern/api/graphic/pixelbuffer.h
    Header file <Graphic-Pixel-Buffer>.

   file: C-kern/graphic/pixelbuffer.c
    Implementation file <Graphic-Pixel-Buffer impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/graphic/pixelbuffer.h"
#include "C-kern/api/err.h"
#include "C-kern/api/graphic/display.h"
#include "C-kern/api/graphic/gconfig.h"
#include "C-kern/api/graphic/gcontext.h"
#include "C-kern/api/test/errortimer.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/test/resourceusage.h"
#include "C-kern/api/graphic/gles2api.h"
#endif


// section: pixelbuffer_t

// group: variable
#ifdef KONFIG_UNITTEST
/* variable: s_pixelbuffer_errtimer
 * Allows to introduce artificial errors. */
static test_errortimer_t   s_pixelbuffer_errtimer = test_errortimer_FREE;
#endif

// group: lifetime

int init_pixelbuffer(/*out*/pixelbuffer_t * pbuf, struct display_t * disp, struct gconfig_t * gconf, uint32_t width, uint32_t height)
{
   int err;
   uint32_t maxwidth  = 0;
   uint32_t maxheight = 0;
   uint32_t maxpixels = 0;

   err = maxpbuffer_gconfig(gconf, disp, &maxwidth, &maxheight, &maxpixels);
   if (err) goto ONABORT;

   if (width > maxwidth || height > maxheight || (width > 0 && maxpixels/width < height)) {
      err = EALLOC;
      goto ONABORT;
   }

#ifdef KONFIG_USERINTERFACE_EGL
   err = init_eglpbuffer(&gl_pixelbuffer(pbuf), gl_display(disp), gl_gconfig(gconf), width, height);
   if (err) goto ONABORT;
#else
   #error "Not implemented"
#endif

   return 0;
ONABORT:
   TRACEABORT_ERRLOG(err);
   return err;
}

int free_pixelbuffer(pixelbuffer_t * pbuf, struct display_t * disp)
{
   int err;

   if (! isfree_surface(pbuf)) {

#ifdef KONFIG_USERINTERFACE_EGL
      err = free_eglpbuffer(&gl_pixelbuffer(pbuf), gl_display(disp));
#else
      #error "Not implemented"
#endif

      if (err) goto ONABORT;
      ONERROR_testerrortimer(&s_pixelbuffer_errtimer, &err, ONABORT);
   }

   return 0;
ONABORT:
   TRACEABORTFREE_ERRLOG(err);
   return err;
}


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(display_t * disp)
{
   gconfig_t      gconf = gconfig_FREE;
   pixelbuffer_t  pbuf  = pixelbuffer_FREE;
   int32_t        confattr[] = {
                     gconfig_TYPE, gconfig_value_TYPE_PBUFFER_BIT, gconfig_NONE
                  };
   uint32_t       maxwidth  = 0;
   uint32_t       maxheight = 0;
   uint32_t       maxpixels = 0;
   uint32_t       configid;

   // prepare
   TEST(0 == init_gconfig(&gconf, disp, confattr));
   TEST(0 == configid_gconfig(&gconf, disp, &configid));
   TEST(0 == maxpbuffer_gconfig(&gconf, disp, &maxwidth, &maxheight, &maxpixels));

   // TEST pixelbuffer_FREE
   TEST(1 == isfree_surface(&pbuf));

   for (uint32_t width = 16; width <= maxwidth; width *= 2) {
      uint32_t height  = 2*width > maxheight ? maxheight : 2*width;
      if (maxpixels / width < height) height = maxpixels / width;

      // TEST init_pixelbuffer
      TEST(0 == init_pixelbuffer(&pbuf, disp, &gconf, width, height));
      TEST(0 == isfree_surface(&pbuf));

      // TEST size_pixelbuffer
      uint32_t width2  = 0;
      uint32_t height2 = 0;
      TEST(0 == size_pixelbuffer(&pbuf, disp, &width2, &height2));
      TEST(width  == width2);
      TEST(height == height2);

      // TEST configid_pixelbuffer
      uint32_t id2 = INT32_MAX;
      TEST(0 == configid_pixelbuffer(&pbuf, disp, &id2));
      TEST(id2 == configid);

      // TEST free_pixelbuffer
      TEST(0 == free_pixelbuffer(&pbuf, disp));
      TEST(1 == isfree_surface(&pbuf));
      TEST(0 == free_pixelbuffer(&pbuf, disp));
      TEST(1 == isfree_surface(&pbuf));
   }

   // TEST init_pixelbuffer: EALLOC
   TEST(EALLOC == init_pixelbuffer(&pbuf, disp, &gconf, maxwidth+1, 1));
   TEST(1 == isfree_surface(&pbuf));
   TEST(EALLOC == init_pixelbuffer(&pbuf, disp, &gconf, 1, maxheight+1));
   TEST(1 == isfree_surface(&pbuf));
   TEST(EALLOC == init_pixelbuffer(&pbuf, disp, &gconf, maxwidth, (maxpixels/maxwidth)+1));
   TEST(1 == isfree_surface(&pbuf));

   // TEST free_pixelbuffer: ERROR
   TEST(0 == init_pixelbuffer(&pbuf, disp, &gconf, 16, 16));
   TEST(0 == isfree_surface(&pbuf));
   init_testerrortimer(&s_pixelbuffer_errtimer, 1, 3);
   TEST(3 == free_pixelbuffer(&pbuf, disp));
   TEST(1 == isfree_surface(&pbuf));

   // unprepare
   TEST(0 == free_gconfig(&gconf));

   return 0;
ONABORT:
   (void) free_gconfig(&gconf);
   return EINVAL;
}

static int test_query(void)
{
   pixelbuffer_t pbuf = pixelbuffer_FREE;

   // TEST gl_pixelbuffer
   for (uintptr_t i = 1; i; i <<= 1) {
      pbuf.glsurface = (void*)i;
      TEST((void*)i == gl_pixelbuffer(&pbuf));
   }
   pbuf.glsurface = 0;
   TEST(0 == gl_pixelbuffer(&pbuf));

   return 0;
ONABORT:
   return EINVAL;
}

static int test_draw(display_t * disp)
{
   gconfig_t      gconf = gconfig_FREE;
   gcontext_t     gcont = gcontext_FREE;
   pixelbuffer_t  pbuf  = pixelbuffer_FREE;
   int32_t        confattr[] = {
                     gconfig_TYPE, gconfig_value_TYPE_PBUFFER_BIT, gconfig_BITS_BUFFER, 32, gconfig_NONE
                  };
   uint32_t       pixels[32*32] = { 0 };
   uint32_t       rgba;

   // prepare
   TEST(0 == init_gconfig(&gconf, disp, confattr));
   TEST(0 == init_gcontext(&gcont, disp, &gconf, gcontext_api_OPENGLES));

   // TEST init_pixelbuffer: drawing into pixel buffer and reading it back
   TEST(0 == init_pixelbuffer(&pbuf, disp, &gconf, 32, 32));
   TEST(0 == setcurrent_gcontext(&gcont, disp, &pbuf, &pbuf));
   glClearColor(1, 0, 1, 0);
   glClear(GL_COLOR_BUFFER_BIT);
   glReadPixels(0, 0, 32, 32, GL_RGBA, GL_UNSIGNED_BYTE, &pixels);
   rgba = 0;
   ((uint8_t*)&rgba)[0] = 0xff;
   ((uint8_t*)&rgba)[2] = 0xff;
   for (unsigned i = 0; i < lengthof(pixels); ++i) {
      TEST(rgba == pixels[i]);
   }

   // unprepare
   TEST(0 == releasecurrent_gcontext(disp));
   TEST(0 == free_gconfig(&gconf));
   TEST(0 == free_pixelbuffer(&pbuf, disp));
   TEST(0 == free_gcontext(&gcont, disp));

   return 0;
ONABORT:
   (void) releasecurrent_gcontext(disp);
   (void) free_gconfig(&gconf);
   (void) free_pixelbuffer(&pbuf, disp);
   (void) free_gcontext(&gcont, disp);
   return EINVAL;
}

static int childprocess_unittest(void)
{
   resourceusage_t   usage = resourceusage_FREE;
   display_t         disp  = display_FREE;

   // prepare
   TEST(0 == initdefault_display(&disp));

   TEST(0 == init_resourceusage(&usage));

   if (test_initfree(&disp))  goto ONABORT;
   if (test_query())          goto ONABORT;

   TEST(0 == same_resourceusage(&usage));
   TEST(0 == free_resourceusage(&usage));

   if (test_draw(&disp))      goto ONABORT;

   // unprepare
   TEST(0 == free_display(&disp));

   return 0;
ONABORT:
   (void) free_resourceusage(&usage);
   (void) free_display(&disp);
   return EINVAL;
}

int unittest_graphic_pixelbuffer()
{
   int err;

   TEST(0 == execasprocess_unittest(&childprocess_unittest, &err));

   return err;
ONABORT:
   return EINVAL;
}

#endif
