/* title: Graphic-Context impl

   Implements <Graphic-Context>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2014 Jörg Seebohn

   file: C-kern/api/graphic/gcontext.h
    Header file <Graphic-Context>.

   file: C-kern/graphic/gcontext.c
    Implementation file <Graphic-Context impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/graphic/gcontext.h"
#include "C-kern/api/err.h"
#include "C-kern/api/graphic/display.h"
#include "C-kern/api/graphic/gconfig.h"
#include "C-kern/api/test/errortimer.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/test/resourceusage.h"
#include "C-kern/api/graphic/gles2api.h"
#include "C-kern/api/graphic/pixelbuffer.h"
#include "C-kern/api/platform/task/thread.h"
#endif


// section: gcontext_t

// group: static variables

#ifdef KONFIG_UNITTEST
/* variable: s_gcontext_errtimer
 * Allows to introduce artificial errors during test. */
static test_errortimer_t   s_gcontext_errtimer = test_errortimer_FREE ;
#endif

// group: lifetime

int init_gcontext(/*out*/gcontext_t * cont, struct display_t * disp, struct gconfig_t * gconf, uint8_t api)
{
   int err;

#if defined(KONFIG_USERINTERFACE_EGL)
   err = init_eglcontext(&gl_gcontext(cont), gl_display(disp), gl_gconfig(gconf), api);
   if (err) goto ONERR;
#else
   #error "No implementation defined"
#endif

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int free_gcontext(gcontext_t * cont, struct display_t * disp)
{
   int err;

   if (cont->glcontext) {
#if defined(KONFIG_USERINTERFACE_EGL)
      err = free_eglcontext(&gl_gcontext(cont), gl_display(disp));
#else
   #error "No implementation defined"
#endif

      if  (err) goto ONERR;
      ONERROR_testerrortimer(&s_gcontext_errtimer, &err, ONERR);
   }

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}



// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(display_t * disp)
{
   gcontext_t cont = gcontext_FREE;
   gconfig_t  conf = gconfig_FREE;
   int32_t    confattr[] = { gconfig_CONFORMANT, gconfig_value_CONFORMANT_ES2_BIT, gconfig_NONE };
   int32_t    bits[] = { gconfig_value_CONFORMANT_ES2_BIT, gconfig_value_CONFORMANT_OPENVG_BIT, gconfig_value_CONFORMANT_OPENGL_BIT };
   gcontext_api_e api[] = { gcontext_api_OPENGLES, gcontext_api_OPENVG, gcontext_api_OPENGL };

   // TEST gcontext_FREE
   TEST(0 == cont.glcontext);

   for (unsigned i = 0; i < lengthof(bits); ++i) {
      // prepare
      confattr[1] = bits[i];
      TEST(0 == init_gconfig(&conf, disp, confattr));

      // TEST init_gcontext
      TEST(0 == init_gcontext(&cont, disp, &conf, api[i]));
      TEST(0 != cont.glcontext);

      // TEST free_gcontext
      TEST(0 == free_gcontext(&cont, disp));
      TEST(0 == cont.glcontext);
      TEST(0 == free_gcontext(&cont, disp));
      TEST(0 == cont.glcontext);

      // unprepare
      TEST(0 == free_gconfig(&conf))
   }

   // prepare
   confattr[1] = gconfig_value_CONFORMANT_ES2_BIT;
   TEST(0 == init_gconfig(&conf, disp, confattr));

   // TEST init_gcontext: ERROR
   TEST(EINVAL == init_gcontext(&cont, &(display_t) display_FREE, &conf, gcontext_api_OPENGLES));
   TEST(0 == cont.glcontext);

   // TEST free_gcontext: ERROR
   TEST(0 == init_gcontext(&cont, disp, &conf, gcontext_api_OPENGLES));
   TEST(0 != cont.glcontext);
   init_testerrortimer(&s_gcontext_errtimer, 1, 7);
   TEST(7 == free_gcontext(&cont, disp));
   TEST(0 == cont.glcontext);

   // unprepare
   TEST(0 == free_gconfig(&conf))

   return 0;
ONERR:
   free_gconfig(&conf);
   free_gcontext(&cont, disp);
   return EINVAL;
}

static int test_query(display_t * disp)
{
   gcontext_t cont = gcontext_FREE;
   gconfig_t  conf = gconfig_FREE;
   int32_t    confattr[] = { gconfig_CONFORMANT, gconfig_value_CONFORMANT_ES2_BIT, gconfig_NONE };
   int32_t    bits[] = { gconfig_value_CONFORMANT_ES2_BIT, gconfig_value_CONFORMANT_OPENVG_BIT, gconfig_value_CONFORMANT_OPENGL_BIT };
   gcontext_api_e api[] = { gcontext_api_OPENGLES, gcontext_api_OPENVG, gcontext_api_OPENGL };

   // TEST gl_gcontext
   for (uintptr_t i = 1; i; i <<= 1) {
      cont.glcontext = (void*)i;
      TEST((void*)i == gl_gcontext(&cont));
   }
   cont.glcontext = 0;
   TEST(0 == gl_gcontext(&cont));

   for (unsigned i = 0; i < lengthof(bits); ++i) {
      // prepare
      confattr[1] = bits[i];
      TEST(0 == init_gconfig(&conf, disp, confattr));
      TEST(0 == init_gcontext(&cont, disp, &conf, api[i]));

      // TEST api_gcontext
      uint8_t api2 = gcontext_api_NROFELEMENTS;
      TEST(0 == api_gcontext(&cont, disp, &api2));
      TEST(api2 == api[i]);

      // TEST configid_gcontext
      uint32_t id1 = 0;
      uint32_t id2 = 1;
      TEST(0 == configid_gcontext(&cont, disp, &id1));
      TEST(0 == configid_gconfig(&conf, disp, &id2));
      TEST(id1 == id2);

      // unprepare
      TEST(0 == free_gcontext(&cont, disp));
      TEST(0 == free_gconfig(&conf))
   }

   return 0;
ONERR:
   free_gconfig(&conf);
   free_gcontext(&cont, disp);
   return EINVAL;
}

static int test_current(display_t * disp)
{
   gconfig_t      gconf = gconfig_FREE;
   gcontext_t     gcont = gcontext_FREE;
   pixelbuffer_t  pbuf  = pixelbuffer_FREE;
   pixelbuffer_t  pbuf2 = pixelbuffer_FREE;
   int32_t        confattr[] = {
                     gconfig_TYPE, gconfig_value_TYPE_PBUFFER_BIT, gconfig_BITS_BUFFER, 32, gconfig_NONE
                  };
   uint32_t       pixels[32*32] = { 0 };
   uint32_t       rgba;
   struct {
      struct opengl_context_t * cont;
      struct opengl_display_t * disp;
      struct opengl_surface_t * drawsurf;
      struct opengl_surface_t * readsurf;
   } current;

   // prepare
   TEST(0 == init_gconfig(&gconf, disp, confattr));
   TEST(0 == init_gcontext(&gcont, disp, &gconf, gcontext_api_OPENGLES));
   TEST(0 == init_pixelbuffer(&pbuf, disp, &gconf, 32, 32));
   TEST(0 == init_pixelbuffer(&pbuf2, disp, &gconf, 32, 32));

   // TEST current_gcontext: returns 0 in case no context is set
   memset(&current, 0xff, sizeof(current));
   current_gcontext(&current.cont, &current.disp, &current.drawsurf, &current.readsurf);
   TEST(0 == current.cont);
   TEST(0 == current.disp);
   TEST(0 == current.drawsurf);
   TEST(0 == current.readsurf);

   // TEST setcurrent_gcontext: drawing into pixel buffer and reading it back
   TEST(0 == setcurrent_gcontext(&gcont, disp, &pbuf, &pbuf2));

   // TEST setcurrent_gcontext: drawing into pixel buffer and reading it back
   TEST(0 == setcurrent_gcontext(&gcont, disp, &pbuf, &pbuf));
   glClearColor(0, 1, 0, 1);
   glClear(GL_COLOR_BUFFER_BIT);
   glReadPixels(0, 0, 32, 32, GL_RGBA, GL_UNSIGNED_BYTE, &pixels);
   rgba = 0;
   ((uint8_t*)&rgba)[1] = 0xff;
   ((uint8_t*)&rgba)[3] = 0xff;
   for (unsigned i = 0; i < lengthof(pixels); ++i) {
      TEST(rgba == pixels[i]);
   }

   // TEST current_gcontext: returns current set context is set
   current_gcontext(&current.cont, &current.disp, &current.drawsurf, &current.readsurf);
   TEST(current.cont == gl_gcontext(&gcont));
   TEST(current.disp == gl_display(disp));
   TEST(current.drawsurf == gl_pixelbuffer(&pbuf));
   TEST(current.readsurf == gl_pixelbuffer(&pbuf));

   // TEST setcurrent_gcontext: releasing previous
   TEST(0 == setcurrent_gcontext(&gcont, disp, &pbuf2, &pbuf));
   current_gcontext(&current.cont, &current.disp, &current.drawsurf, &current.readsurf);
   TEST(current.cont == gl_gcontext(&gcont));
   TEST(current.disp == gl_display(disp));
   TEST(current.drawsurf == gl_pixelbuffer(&pbuf2));
   TEST(current.readsurf == gl_pixelbuffer(&pbuf));

   // TEST releasecurrent_gcontext: reading back does no more work
   TEST(0 == releasecurrent_gcontext(disp));
   memset(pixels, 0, sizeof(pixels));
   glReadPixels(0, 0, 32, 32, GL_RGBA, GL_UNSIGNED_BYTE, &pixels);
   for (unsigned i = 0; i < lengthof(pixels); ++i) {
      TEST(0 == pixels[i]);
   }

   // TEST current_gcontext: returns 0 after released
   current_gcontext(&current.cont, &current.disp, &current.drawsurf, &current.readsurf);
   TEST(0 == current.cont);
   TEST(0 == current.disp);
   TEST(0 == current.drawsurf);
   TEST(0 == current.readsurf);

   // TEST setcurrent_gcontext: reattaching surface preserves content of frame buffer
   TEST(0 == setcurrent_gcontext(&gcont, disp, &pbuf, &pbuf));
   glReadPixels(0, 0, 32, 32, GL_RGBA, GL_UNSIGNED_BYTE, &pixels);
   for (unsigned i = 0; i < lengthof(pixels); ++i) {
      TEST(rgba == pixels[i]);
   }

   // unprepare
   TEST(0 == releasecurrent_gcontext(disp));
   TEST(0 == free_gconfig(&gconf));
   TEST(0 == free_pixelbuffer(&pbuf, disp));
   TEST(0 == free_pixelbuffer(&pbuf2, disp));
   TEST(0 == free_gcontext(&gcont, disp));

   return 0;
ONERR:
   (void) releasecurrent_gcontext(disp);
   (void) free_gconfig(&gconf);
   (void) free_pixelbuffer(&pbuf, disp);
   (void) free_pixelbuffer(&pbuf2, disp);
   (void) free_gcontext(&gcont, disp);
   return EINVAL;
}


struct threadarg_t {
   display_t    * disp;
   gcontext_t   * gcont;
   pixelbuffer_t* pbuf;
   uint32_t     (* pixels)[32*32];
   uint32_t       rgba;
};

static int thread_setcurrent_notok(struct threadarg_t * arg)
{
   // setcurrent_gcontext: EACCES (other thread already uses gcont)
   TEST(EACCES == setcurrent_gcontext(arg->gcont, arg->disp, arg->pbuf, arg->pbuf));
   // no gl commands work
   glReadPixels(0, 0, 32, 32, GL_RGBA, GL_UNSIGNED_BYTE, arg->pixels);
   for (unsigned i = 0; i < lengthof(*arg->pixels); ++i) {
      TEST(0 == (*arg->pixels)[i]);
   }

   struct {
      struct opengl_context_t * cont;
      struct opengl_display_t * disp;
      struct opengl_surface_t * drawsurf;
      struct opengl_surface_t * readsurf;
   } current;

   // TEST current_gcontext: returns 0 in case no context is set
   memset(&current, 0xff, sizeof(current));
   current_gcontext(&current.cont, &current.disp, &current.drawsurf, &current.readsurf);
   TEST(0 == current.cont);
   TEST(0 == current.disp);
   TEST(0 == current.drawsurf);
   TEST(0 == current.readsurf);

   CLEARBUFFER_ERRLOG();
   return 0;
ONERR:
   return EINVAL;
}

static int thread_setcurrent_ok(struct threadarg_t * arg)
{
   // setcurrent_gcontext: 0 (other thread released context)
   TEST(0 == setcurrent_gcontext(arg->gcont, arg->disp, arg->pbuf, arg->pbuf));
   // gl commands work
   glReadPixels(0, 0, 32, 32, GL_RGBA, GL_UNSIGNED_BYTE, arg->pixels);
   for (unsigned i = 0; i < lengthof(*arg->pixels); ++i) {
      TEST(arg->rgba == (*arg->pixels)[i]);
   }

   struct {
      struct opengl_context_t * cont;
      struct opengl_display_t * disp;
      struct opengl_surface_t * drawsurf;
      struct opengl_surface_t * readsurf;
   } current;

   // TEST current_gcontext: returns 0 in case no context is set
   memset(&current, 0, sizeof(current));
   current_gcontext(&current.cont, &current.disp, &current.drawsurf, &current.readsurf);
   TEST(current.cont == gl_gcontext(arg->gcont));
   TEST(current.disp == gl_display(arg->disp));
   TEST(current.drawsurf == gl_pixelbuffer(arg->pbuf));
   TEST(current.readsurf == gl_pixelbuffer(arg->pbuf));

   TEST(0 == releasecurrent_gcontext(arg->disp));

   return 0;
ONERR:
   return EINVAL;
}

static int test_thread(display_t * disp)
{
   gconfig_t      gconf = gconfig_FREE;
   gcontext_t     gcont = gcontext_FREE;
   pixelbuffer_t  pbuf  = pixelbuffer_FREE;
   thread_t     * thread = 0;
   int32_t        confattr[] = {
                     gconfig_TYPE, gconfig_value_TYPE_PBUFFER_BIT, gconfig_BITS_BUFFER, 32, gconfig_NONE
                  };
   uint32_t       pixels[32*32] = { 0 };
   uint32_t       rgba;
   resourceusage_t usage = resourceusage_FREE;

   // prepare
   TEST(0 == init_gconfig(&gconf, disp, confattr));
   TEST(0 == init_gcontext(&gcont, disp, &gconf, gcontext_api_OPENGLES));
   TEST(0 == init_pixelbuffer(&pbuf, disp, &gconf, 32, 32));

   // TEST setcurrent_gcontext: context is locked by this thread
   TEST(0 == setcurrent_gcontext(&gcont, disp, &pbuf, &pbuf));
   glClearColor(0, 1, 1, 1);
   glClear(GL_COLOR_BUFFER_BIT);
   rgba = 0xffffffff;
   ((uint8_t*)&rgba)[0] = 0;
   struct threadarg_t args = { disp, &gcont, &pbuf, &pixels, rgba };
   TEST(0 == newgeneric_thread(&thread, &thread_setcurrent_notok, &args));
   TEST(0 == join_thread(thread));
   TEST(0 == returncode_thread(thread));
   TEST(0 == delete_thread(&thread));
   // test resources are freed (first time thread allocates resources)
   TEST(0 == init_resourceusage(&usage));
   TEST(0 == newgeneric_thread(&thread, &thread_setcurrent_notok, &args));
   TEST(0 == join_thread(thread));
   TEST(0 == returncode_thread(thread));
   TEST(0 == delete_thread(&thread));
   TEST(0 == same_resourceusage(&usage));
   TEST(0 == free_resourceusage(&usage));

   // TEST releasecurrent_gcontext: context is unlocked
   TEST(0 == releasecurrent_gcontext(disp));
   TEST(0 == newgeneric_thread(&thread, &thread_setcurrent_ok, &args));
   TEST(0 == join_thread(thread));
   TEST(0 == returncode_thread(thread));
   TEST(0 == delete_thread(&thread));
   // test resources are freed (first time thread allocates resources)
   TEST(0 == init_resourceusage(&usage));
   TEST(0 == newgeneric_thread(&thread, &thread_setcurrent_ok, &args));
   TEST(0 == join_thread(thread));
   TEST(0 == returncode_thread(thread));
   TEST(0 == delete_thread(&thread));
   TEST(0 == same_resourceusage(&usage));
   TEST(0 == free_resourceusage(&usage));
   // other thread released context
   TEST(0 == setcurrent_gcontext(&gcont, disp, &pbuf, &pbuf));

   // unprepare
   TEST(0 == releasecurrent_gcontext(disp));
   TEST(0 == free_gconfig(&gconf));
   TEST(0 == free_pixelbuffer(&pbuf, disp));
   TEST(0 == free_gcontext(&gcont, disp));

   return 0;
ONERR:
   (void) delete_thread(&thread);
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

   if (test_initfree(&disp))     goto ONERR;
   if (test_query(&disp))        goto ONERR;
   if (test_current(&disp))      goto ONERR;
   if (test_thread(&disp))       goto ONERR;

   TEST(0 == init_resourceusage(&usage));

   // no other test

   TEST(0 == same_resourceusage(&usage));
   TEST(0 == free_resourceusage(&usage));

   // unprepare
   TEST(0 == free_display(&disp));

   return 0;
ONERR:
   (void) free_resourceusage(&usage);
   (void) free_display(&disp);
   return EINVAL;
}

int unittest_graphic_gcontext()
{
   int err;

   TEST(0 == execasprocess_unittest(&childprocess_unittest, &err));

   return err;
ONERR:
   return EINVAL;
}

#endif
