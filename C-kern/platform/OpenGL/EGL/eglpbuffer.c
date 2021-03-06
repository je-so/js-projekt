/* title: EGl-PBuffer impl

   Implements <EGl-PBuffer>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

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
#include "C-kern/api/graphic/gles2api.h"
#include "C-kern/api/platform/OpenGL/EGL/eglconfig.h"
#include "C-kern/api/platform/OpenGL/EGL/egldisplay.h"
#endif
#include STR(C-kern/api/platform/KONFIG_OS/graphic/sysegl.h)


// section: eglpbuffer_t

// group: static variables

#ifdef KONFIG_UNITTEST
/* variable: s_eglpbuffer_errtimer
 * Allows to introduce artificial errors during test. */
static test_errortimer_t   s_eglpbuffer_errtimer = test_errortimer_FREE;
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
      goto ONERR;
   }

   *eglpbuf = surface;

   return 0;
ONERR:
   err = convert2errno_egl(eglGetError());
   TRACEEXIT_ERRLOG(err);
   return err;
}

int free_eglpbuffer(eglpbuffer_t * eglpbuf, struct opengl_display_t * egldisp)
{
   int err;

   if (*eglpbuf) {
      EGLBoolean isDestoyed = eglDestroySurface(egldisp, *eglpbuf);

      *eglpbuf = 0;

      if (EGL_FALSE == isDestoyed) {
         err = convert2errno_egl(eglGetError());
         goto ONERR;
      }

      if (PROCESS_testerrortimer(&s_eglpbuffer_errtimer, &err)) goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

// group: query

int size_eglpbuffer(const eglpbuffer_t eglpbuf, struct opengl_display_t * egldisp, /*out*/uint32_t * width, /*out*/uint32_t * height)
{
   int err;
   EGLint attr_value;
   static_assert(sizeof(EGLint) <= sizeof(uint32_t), "attribute type can be converted to returned type");
   if (!eglQuerySurface(egldisp, eglpbuf, EGL_WIDTH, &attr_value)) {
      goto ONERR;
   }
   *width = (uint32_t) attr_value;
   if (!eglQuerySurface(egldisp, eglpbuf, EGL_HEIGHT, &attr_value)) {
      goto ONERR;
   }
   *height = (uint32_t) attr_value;

   return 0;
ONERR:
   err = convert2errno_egl(eglGetError());
   TRACEEXIT_ERRLOG(err);
   return err;
}

int configid_eglpbuffer(const eglpbuffer_t eglpbuf, struct opengl_display_t * egldisp, /*out*/uint32_t * configid)
{
   int err;
   EGLint value;
   static_assert(sizeof(EGLint) <= sizeof(uint32_t), "attribute type can be converted to returned type");

   if (!eglQuerySurface(egldisp, eglpbuf, EGL_CONFIG_ID, &value)) {
      goto ONERR;
   }

   *configid = (uint32_t) value;

   return 0;
ONERR:
   err = convert2errno_egl(eglGetError());
   TRACEEXIT_ERRLOG(err);
   return err;
}


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(egldisplay_t disp)
{
   eglpbuffer_t pbuf = eglpbuffer_FREE;
   eglconfig_t  conf = eglconfig_FREE;
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

   // TEST eglpbuffer_FREE
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
ONERR:
   (void) free_eglpbuffer(&pbuf, disp);
   (void) free_eglconfig(&conf);
   return EINVAL;
}

static int test_query(egldisplay_t disp)
{
   eglpbuffer_t pbuf = eglpbuffer_FREE;
   eglconfig_t  conf = eglconfig_FREE;
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
   TEST(EINVAL == size_eglpbuffer(pbuf, egldisplay_FREE, &width, &height));
   TEST(0 == width && 0 == height);
   TEST(EINVAL == size_eglpbuffer(eglpbuffer_FREE, disp, &width, &height));
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
   TEST(EINVAL == configid_eglpbuffer(pbuf, egldisplay_FREE, &id));
   TEST(INT32_MAX == id);
   TEST(EINVAL == configid_eglpbuffer(eglpbuffer_FREE, disp, &id));
   TEST(INT32_MAX == id);
   TEST(0 == free_eglpbuffer(&pbuf, disp));

   // unprepare
   TEST(0 == free_eglconfig(&conf));

   return 0;
ONERR:
   (void) free_eglpbuffer(&pbuf, disp);
   (void) free_eglconfig(&conf);
   return EINVAL;
}

static int test_draw(egldisplay_t disp)
{
   eglpbuffer_t pbuf = eglpbuffer_FREE;
   eglconfig_t  conf = eglconfig_FREE;
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
ONERR:
   (void) eglMakeCurrent(disp, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
   (void) free_eglpbuffer(&pbuf, disp);
   (void) free_eglconfig(&conf);
   return EINVAL;
}

static int childprocess_unittest(void)
{
   resourceusage_t   usage = resourceusage_FREE;
   egldisplay_t      disp  = egldisplay_FREE;
   uint8_t         * logbuffer;
   size_t            logsize;

   // prepare
   TEST(0 == initdefault_egldisplay(&disp));
   if (test_initfree(disp))   goto ONERR;
   if (test_query(disp))      goto ONERR;

   TEST(0 == init_resourceusage(&usage));

   GETBUFFER_ERRLOG(&logbuffer, &logsize);
   if (test_initfree(disp))   goto ONERR;
   if (test_query(disp))      goto ONERR;
   TRUNCATEBUFFER_ERRLOG(logsize);

   TEST(0 == same_resourceusage(&usage));
   TEST(0 == free_resourceusage(&usage));

   if (test_draw(disp))       goto ONERR;

   // unprepare
   TEST(0 == free_egldisplay(&disp));

   return 0;
ONERR:
   (void) free_resourceusage(&usage);
   (void) free_egldisplay(&disp);
   return EINVAL;
}

int unittest_platform_opengl_egl_eglpbuffer()
{
   int err;

   TEST(0 == execasprocess_unittest(&childprocess_unittest, &err));

   return err;
ONERR:
   return EINVAL;
}

#endif
