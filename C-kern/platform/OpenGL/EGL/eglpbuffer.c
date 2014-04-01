/* title: EGl-PBuffer impl

   Implements <EGl-PBuffer>.

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

   file: C-kern/api/platform/OpenGL/EGL/eglpbuffer.h
    Header file <EGl-PBuffer>.

   file: C-kern/platform/OpenGL/EGL/eglpbuffer.c
    Implementation file <EGl-PBuffer impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/platform/OpenGL/EGL/eglpbuffer.h"
#include "C-kern/api/err.h"
#include "C-kern/api/platform/OpenGL/EGL/egl.h"
#include "C-kern/api/test/errortimer.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/test/resourceusage.h"
#include "C-kern/api/graphic/gconfig.h"
#include "C-kern/api/platform/OpenGL/EGL/eglconfig.h"
#include "C-kern/api/platform/OpenGL/EGL/egldisplay.h"
#endif
#include STR(C-kern/api/platform/KONFIG_OS/graphic/sysegl.h)
#include STR(C-kern/api/platform/KONFIG_OS/graphic/sysgles2.h)


// section: eglpbuffer_t

// group: variable
#ifdef KONFIG_UNITTEST
static test_errortimer_t   s_eglpbuffer_errtimer = test_errortimer_INIT_FREEABLE ;
#endif

// group: lifetime

int init_eglpbuffer(/*out*/eglpbuffer_t * eglpbuf, struct opengl_display_t * egldisp, struct opengl_config_t * eglconf, uint32_t width, uint32_t height)
{
   int err;
   EGLint attr[] = {
      EGL_HEIGHT, height > INT32_MAX ? INT32_MAX : (int32_t)height,
      EGL_WIDTH,  width  > INT32_MAX ? INT32_MAX : (int32_t)width,
      EGL_NONE
   };
   EGLSurface surface = eglCreatePbufferSurface(egldisp, eglconf, attr);

   if (EGL_NO_SURFACE == surface) {
      goto ONABORT;
   }

   *eglpbuf = surface;

   return 0;
ONABORT:
   err = aserrcode_egl(eglGetError());
   TRACEABORT_ERRLOG(err);
   return err;
}

int free_eglpbuffer(eglpbuffer_t * eglpbuf, struct opengl_display_t * egldisp)
{
   int err;

   if (*eglpbuf) {
      EGLBoolean isDestoyed = eglDestroySurface(egldisp, *eglpbuf);

      *eglpbuf = 0;

      if (EGL_FALSE == isDestoyed) {
         err = aserrcode_egl(eglGetError());
         goto ONABORT;
      }

      ONERROR_testerrortimer(&s_eglpbuffer_errtimer, &err, ONABORT);
   }

   return 0;
ONABORT:
   TRACEABORTFREE_ERRLOG(err);
   return err;
}

// group: query

int size_eglpbuffer(const eglpbuffer_t eglpbuf, struct opengl_display_t * egldisp, /*out*/uint32_t * width, /*out*/uint32_t * height)
{
   int err;
   EGLint attr_value;
   static_assert(sizeof(EGLint) <= sizeof(uint32_t), "attribute type can be converted to returned type");
   if (!eglQuerySurface(egldisp, eglpbuf, EGL_WIDTH, &attr_value)) {
      goto ONABORT;
   }
   *width = (uint32_t) attr_value;
   if (!eglQuerySurface(egldisp, eglpbuf, EGL_HEIGHT, &attr_value)) {
      goto ONABORT;
   }
   *height = (uint32_t) attr_value;

   return 0;
ONABORT:
   err = aserrcode_egl(eglGetError());
   TRACEABORT_ERRLOG(err);
   return err;
}

int configid_eglpbuffer(const eglpbuffer_t eglpbuf, struct opengl_display_t * egldisp, /*out*/uint32_t * configid)
{
   int err;
   EGLint value;
   static_assert(sizeof(EGLint) <= sizeof(uint32_t), "attribute type can be converted to returned type");

   if (!eglQuerySurface(egldisp, eglpbuf, EGL_CONFIG_ID, &value)) {
      goto ONABORT;
   }

   *configid = (uint32_t) value;

   return 0;
ONABORT:
   err = aserrcode_egl(eglGetError());
   TRACEABORT_ERRLOG(err);
   return err;
}


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(egldisplay_t disp)
{
   eglpbuffer_t pbuf = eglpbuffer_INIT_FREEABLE;
   eglconfig_t  conf = eglconfig_INIT_FREEABLE;
   uint32_t maxwidth;
   uint32_t maxheight;
   uint32_t maxpixels;
   uint32_t maxsize;

   EGLint width;
   EGLint height;

   // prepare
   TEST(0 == init_eglconfig(&conf, disp, (const int32_t[]) {
      gconfig_TYPE, gconfig_value_TYPE_PBUFFER_BIT, gconfig_BITS_BUFFER, 32, gconfig_NONE
   }));
   {
      TEST(0 == maxpbuffer_eglconfig(conf, disp, &maxwidth, &maxheight, &maxpixels));
      for (maxsize = (maxwidth <= maxheight) ? maxwidth : maxheight; maxsize * maxsize > maxpixels; maxsize >>= 1)
         ;
   }

   // TEST eglpbuffer_INIT_FREEABLE
   TEST(0 == pbuf);

   for (unsigned i = 8; i <= maxsize; i *= 2) {
      // TEST init_eglpbuffer
      TEST(0 == init_eglpbuffer(&pbuf, disp, conf, i, i));
      TEST(0 != pbuf);
      TEST(0 != eglQuerySurface(disp, pbuf, EGL_WIDTH, &width));
      TEST(0 != eglQuerySurface(disp, pbuf, EGL_HEIGHT, &height));
      TEST(i == (unsigned) width);
      TEST(i == (unsigned) height);

      // TEST free_eglpbuffer
      TEST(0 == free_eglpbuffer(&pbuf, disp));
      TEST(0 == pbuf);
      TEST(0 == free_eglpbuffer(&pbuf, disp));
      TEST(0 == pbuf);
   }

   // TEST init_eglpbuffer: size too big
   // bug in mesa implementation / radeon driver prints error but mesa egl implementation returns OK
   // TEST(EALLOC == init_eglpbuffer(&pbuf, disp, conf, INT32_MAX, INT32_MAX));
   TEST(0 == pbuf);

   // TEST init_eglpbuffer: id of config == configid of pbuffer
   EGLint    listsize = 0;
   EGLConfig conflist[256];
   TEST(0 != eglChooseConfig(disp, (const EGLint[]){EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_NONE},
                     conflist, lengthof(conflist), &listsize));
   for (int i = 0; i < listsize; ++i) {
      EGLint id1 = 0;
      EGLint id2 = 1;
      TEST(0 == init_eglpbuffer(&pbuf, disp, conflist[i], 16, 16));
      TEST(0 != pbuf);
      TEST(0 != eglGetConfigAttrib(disp, conflist[i], EGL_CONFIG_ID, &id1));
      TEST(0 != eglQuerySurface(disp, pbuf, EGL_CONFIG_ID, &id2));
      TEST(id1 == id2);
      TEST(0 == free_eglpbuffer(&pbuf, disp));
      TEST(0 == pbuf);
   }

   // TEST free_eglpbuffer: ERROR
   TEST(0 == init_eglpbuffer(&pbuf, disp, conf, 16, 16));
   TEST(0 != pbuf);
   init_testerrortimer(&s_eglpbuffer_errtimer, 1, EACCES);
   TEST(EACCES == free_eglpbuffer(&pbuf, disp));
   TEST(0 == pbuf);

   // unprepare
   TEST(0 == free_eglconfig(&conf));

   return 0;
ONABORT:
   (void) free_eglpbuffer(&pbuf, disp);
   (void) free_eglconfig(&conf);
   return EINVAL;
}

static int test_query(egldisplay_t disp)
{
   eglpbuffer_t pbuf = eglpbuffer_INIT_FREEABLE;
   eglconfig_t  conf = eglconfig_INIT_FREEABLE;
   uint32_t     id   = 0;
   uint32_t     width  = 0;
   uint32_t     height = 0;

   // prepare
   TEST(0 == init_eglconfig(&conf, disp, (const int32_t[]) {
      gconfig_TYPE, gconfig_value_TYPE_PBUFFER_BIT, gconfig_BITS_BUFFER, 32, gconfig_NONE
   }));

   // TEST size_eglpbuffer
   for (unsigned i = 8; i <= 256; i *= 2) {
      TEST(0 == init_eglpbuffer(&pbuf, disp, conf, i, i));
      TEST(0 == size_eglpbuffer(pbuf, disp, &width, &height));
      TEST(i == width);
      TEST(i == height);
      TEST(0 == free_eglpbuffer(&pbuf, disp));

      TEST(0 == init_eglpbuffer(&pbuf, disp, conf, i/2, i));
      TEST(0 == size_eglpbuffer(pbuf, disp, &width, &height));
      TEST(i == width*2);
      TEST(i == height);
      TEST(0 == free_eglpbuffer(&pbuf, disp));

      TEST(0 == init_eglpbuffer(&pbuf, disp, conf, i, i/2));
      TEST(0 == size_eglpbuffer(pbuf, disp, &width, &height));
      TEST(i == width);
      TEST(i == height*2);
      TEST(0 == free_eglpbuffer(&pbuf, disp));
   }

   // size_eglpbuffer: EINVAL
   width = height = 0;
   TEST(0 == init_eglpbuffer(&pbuf, disp, conf, 16, 16));
   TEST(EINVAL == size_eglpbuffer(pbuf, egldisplay_INIT_FREEABLE, &width, &height));
   TEST(0 == width && 0 == height);
   TEST(EINVAL == size_eglpbuffer(eglpbuffer_INIT_FREEABLE, disp, &width, &height));
   TEST(0 == width && 0 == height);
   TEST(0 == free_eglpbuffer(&pbuf, disp));

   // TEST configid_eglpbuffer
   EGLint    listsize = 0;
   EGLConfig conflist[256];
   TEST(0 != eglChooseConfig(disp, (const EGLint[]){EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_NONE},
                     conflist, lengthof(conflist), &listsize));
   for (int i = 0; i < listsize; ++i) {
      uint32_t id2 = id + 1;
      TEST(0 == init_eglpbuffer(&pbuf, disp, conflist[i], 16, 16));
      TEST(0 == configid_eglconfig(conflist[i], disp, &id2));
      TEST(0 == configid_eglpbuffer(pbuf, disp, &id));
      TEST(id == id2);
      TEST(0 == free_eglpbuffer(&pbuf, disp));
   }

   // TEST configid_eglpbuffer: EINVAL
   id = INT32_MAX;
   TEST(0 == init_eglpbuffer(&pbuf, disp, conf, 16, 16));
   TEST(EINVAL == configid_eglpbuffer(pbuf, egldisplay_INIT_FREEABLE, &id));
   TEST(INT32_MAX == id);
   TEST(EINVAL == configid_eglpbuffer(eglpbuffer_INIT_FREEABLE, disp, &id));
   TEST(INT32_MAX == id);
   TEST(0 == free_eglpbuffer(&pbuf, disp));

   // unprepare
   TEST(0 == free_eglconfig(&conf));

   return 0;
ONABORT:
   (void) free_eglpbuffer(&pbuf, disp);
   (void) free_eglconfig(&conf);
   return EINVAL;
}

static int test_draw(egldisplay_t disp)
{
   eglpbuffer_t pbuf = eglpbuffer_INIT_FREEABLE;
   eglconfig_t  conf = eglconfig_INIT_FREEABLE;
   EGLContext   ctx  = EGL_NO_CONTEXT;
   uint32_t     rgba;
   uint32_t     pixels[32*32] = { 0 };

   // prepare
   TEST(0 == init_eglconfig(&conf, disp, (const int32_t[]) {
      gconfig_TYPE, gconfig_value_TYPE_PBUFFER_BIT, gconfig_BITS_BUFFER, 32, gconfig_NONE
   }));
   TEST(0 == init_eglpbuffer(&pbuf, disp, conf, 32, 32));
   ctx = eglCreateContext(disp, conf, EGL_NO_CONTEXT, 0);
   TEST(EGL_NO_CONTEXT != ctx);
   TEST(0 != eglMakeCurrent(disp, pbuf, pbuf, ctx));

   // TEST clearing pixel buffer and reading back values
   glClearColor(1.0, 1.0, 0, 0);
   glClear(GL_COLOR_BUFFER_BIT);
   glReadPixels(0, 0, 32, 32, GL_RGBA, GL_UNSIGNED_BYTE, &pixels);
   rgba = 0;
   ((uint8_t*)&rgba)[0] = 0xff;
   ((uint8_t*)&rgba)[1] = 0xff;
   for (unsigned i = 0; i < lengthof(pixels); ++i) {
      TEST(rgba == pixels[i]);
   }
   glClearColor(0, 0, 1, 1);
   glClear(GL_COLOR_BUFFER_BIT);
   glReadPixels(0, 0, 32, 32, GL_RGBA, GL_UNSIGNED_BYTE, &pixels);
   rgba = 0;
   ((uint8_t*)&rgba)[2] = 0xff;
   ((uint8_t*)&rgba)[3] = 0xff;
   for (unsigned i = 0; i < lengthof(pixels); ++i) {
      TEST(rgba == pixels[i]);
   }

   // unprepare
   TEST(0 != eglMakeCurrent(disp, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
   TEST(0 == free_eglpbuffer(&pbuf, disp));
   TEST(0 == free_eglconfig(&conf));

   return 0;
ONABORT:
   (void) eglMakeCurrent(disp, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
   (void) free_eglpbuffer(&pbuf, disp);
   (void) free_eglconfig(&conf);
   return EINVAL;
}

static int childprocess_unittest(void)
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE;
   egldisplay_t      disp  = egldisplay_INIT_FREEABLE;

   // prepare
   TEST(0 == initdefault_egldisplay(&disp));

   TEST(0 == init_resourceusage(&usage));

   if (test_initfree(disp))   goto ONABORT;
   if (test_query(disp))      goto ONABORT;

   TEST(0 == same_resourceusage(&usage));
   TEST(0 == free_resourceusage(&usage));

   if (test_draw(disp))       goto ONABORT;

   // unprepare
   TEST(0 == free_egldisplay(&disp));

   return 0;
ONABORT:
   (void) free_resourceusage(&usage);
   (void) free_egldisplay(&disp);
   return EINVAL;
}

int unittest_platform_opengl_egl_eglpbuffer()
{
   int err;

   TEST(0 == execasprocess_unittest(&childprocess_unittest, &err));

   return err;
ONABORT:
   return EINVAL;
}

#endif
