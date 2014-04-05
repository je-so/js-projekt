/* title: EGL-Window impl

   Implements <EGL-Window>.

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

   file: C-kern/api/platform/OpenGL/EGL/eglwindow.h
    Header file <EGL-Window>.

   file: C-kern/platform/OpenGL/EGL/eglwindow.c
    Implementation file <EGL-Window impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/platform/OpenGL/EGL/eglwindow.h"
#include "C-kern/api/err.h"
#include "C-kern/api/platform/OpenGL/EGL/egl.h"
#include "C-kern/api/platform/OpenGL/EGL/eglconfig.h"
#include "C-kern/api/platform/OpenGL/EGL/egldisplay.h"
#include "C-kern/api/platform/X11/x11window.h"
#include "C-kern/api/test/errortimer.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/test/resourceusage.h"
#include "C-kern/api/graphic/display.h"
#include "C-kern/api/graphic/gconfig.h"
#include "C-kern/api/graphic/gles2api.h"
#include "C-kern/api/graphic/windowconfig.h"
#include "C-kern/api/platform/task/thread.h"
#include "C-kern/api/platform/X11/x11.h"
#include "C-kern/api/platform/X11/x11display.h"
#include "C-kern/api/platform/X11/x11screen.h"
#endif
#include STR(C-kern/api/platform/KONFIG_OS/graphic/sysegl.h)


// section: eglwindow_t

// group: variable
#ifdef KONFIG_UNITTEST
static test_errortimer_t   s_eglwindow_errtimer = test_errortimer_INIT_FREEABLE ;
#endif

// group: lifetime

#if defined(KONFIG_USERINTERFACE_X11)
int initx11_eglwindow(/*out*/eglwindow_t * eglwin, egldisplay_t egldisp, eglconfig_t eglconf, struct x11window_t * x11win)
{
   int err;

   VALIDATE_INPARAM_TEST(x11win->sys_drawable != 0, ONABORT, );

   EGLint     attrib[] = { EGL_NONE };
   EGLSurface window   = eglCreateWindowSurface(egldisp, eglconf, x11win->sys_drawable, attrib);

   if (EGL_NO_SURFACE == window) {
      err = aserrcode_egl(eglGetError());
      goto ONABORT;
   }

   *eglwin = window;

   return 0;
ONABORT:
   TRACEABORT_ERRLOG(err);
   return err;
}
#endif

int free_eglwindow(eglwindow_t * eglwin, egldisplay_t egldisp)
{
   int err;

   if (*eglwin) {
      EGLBoolean isDestoyed = eglDestroySurface(egldisp, *eglwin);

      *eglwin = 0;

      if (EGL_FALSE == isDestoyed) {
         err = aserrcode_egl(eglGetError());
         goto ONABORT;
      }

      ONERROR_testerrortimer(&s_eglwindow_errtimer, &err, ONABORT);
   }

   return 0;
ONABORT:
   TRACEABORTFREE_ERRLOG(err);
   return err;
}

// group: update

int swapbuffer_eglwindow(eglwindow_t eglwin, egldisplay_t egldisp)
{
   int err;

   if (EGL_FALSE == eglSwapBuffers(egldisp, eglwin)) {
      err = aserrcode_egl(eglGetError());
      goto ONABORT;
   }

   return 0;
ONABORT:
   TRACEABORTFREE_ERRLOG(err);
   return err;
}


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

typedef struct native_types_t native_types_t;

#if defined(KONFIG_USERINTERFACE_X11)

#define INITOS_EGLWINDOW(...)    initx11_eglwindow(__VA_ARGS__)

struct native_types_t {
   display_t      display;
   x11window_t    freeoswindow;
   x11window_t    oswindow[5];
   eglconfig_t    eglconfig[5];
   int32_t        config_attr[5][5];
   EGLContext     eglcontext;
};

#define native_types_INIT_FREEABLE  \
         {  display_INIT_FREEABLE,  \
            x11window_INIT_FREEABLE, \
            { x11window_INIT_FREEABLE, x11window_INIT_FREEABLE, x11window_INIT_FREEABLE, x11window_INIT_FREEABLE, x11window_INIT_FREEABLE }, \
            { eglconfig_INIT_FREEABLE, eglconfig_INIT_FREEABLE, eglconfig_INIT_FREEABLE, eglconfig_INIT_FREEABLE, eglconfig_INIT_FREEABLE }, \
            {  { gconfig_BITS_BUFFER, 32, gconfig_NONE, 0, 0 },   \
               { gconfig_BITS_RED,    1,  gconfig_NONE, 0, 0 },   \
               { gconfig_BITS_DEPTH,  1,  gconfig_NONE, 0, 0 },   \
               { gconfig_BITS_ALPHA,  1,  gconfig_NONE, 0, 0 },   \
               { gconfig_TRANSPARENT_ALPHA, 1, gconfig_BITS_BUFFER, 32, gconfig_NONE },   \
            }, \
            EGL_NO_CONTEXT, \
         }

static int init_native(/*out*/native_types_t * native)
{
   uint32_t    snr = 0;
   gconfig_t   gconf = gconfig_INIT_FREEABLE;

   windowconfig_t winattr[] = {
      windowconfig_INIT_TITLE("egl-test-window"),
      windowconfig_INIT_SIZE(100, 100),
      windowconfig_INIT_POS(50, 100),
      windowconfig_INIT_NONE
   };

   TEST(0 == initdefault_display(&native->display));
   snr = defaultscreennr_display(&native->display);

   for (unsigned i = 0; i < lengthof(native->oswindow); ++i) {
      TEST(0 == init_gconfig(&gconf, &native->display, native->config_attr[i]));
      int32_t visualid;
      TEST(0 == visualid_gconfig(&gconf, &native->display, &visualid));
      TEST(0 == initvid_x11window(&native->oswindow[i], os_display(&native->display), snr, 0, (uint32_t)visualid, winattr));
      native->eglconfig[i] = gl_gconfig(&gconf);
   }

   native->eglcontext = eglCreateContext(gl_display(&native->display), native->eglconfig[0], EGL_NO_CONTEXT, 0);
   TEST(EGL_NO_CONTEXT != native->eglcontext);

   return 0;
ONABORT:
   return EINVAL;
}

static int free_native(native_types_t * native)
{
	TEST(EGL_TRUE == eglMakeCurrent(gl_display(&native->display), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
	TEST(EGL_TRUE == eglDestroyContext(gl_display(&native->display), native->eglcontext));
   for (unsigned i = 0; i < lengthof(native->oswindow); ++i) {
      TEST(0 == free_eglconfig(&native->eglconfig[i]));
      TEST(0 == free_x11window(&native->oswindow[i]));
   }
   TEST(0 == free_display(&native->display));
   return 0;
ONABORT:
   return EINVAL;
}

#define WAITFOR(disp, CONDITION) \
   XFlush(os_display(disp)->sys_display);          \
   for (int _count = 0; _count < 100; ++_count) {  \
      while (XPending(os_display(disp)->sys_display)) {  \
         dispatchevent_X11(os_display(disp));      \
      }                                            \
      if (CONDITION) break ;                       \
      sleepms_thread(5) ;                          \
   }

static int compare_color(x11window_t * x11win, uint32_t w, uint32_t h, bool isRed, bool isGreen, bool isBlue)
{
   XImage * ximg = 0 ;
   size_t   pixels ;
   uint32_t x, y ;

   Window root = RootWindow(x11win->display->sys_display, screen_x11window(x11win)) ;
   Window windummy ;
   int32_t x2, y2 ;
   XTranslateCoordinates(x11win->display->sys_display, x11win->sys_drawable, root, 0, 0, &x2, &y2, &windummy) ;
   ximg = XGetImage(x11win->display->sys_display, root, x2, y2, w, h, (unsigned long)-1, ZPixmap) ;

   for (pixels = 0, y = 0; y < h; ++y) {
      for (x = 0; x < w ; ++x) {
         unsigned long rgbcolor = XGetPixel(ximg, (int)x, (int)y) ;
         if (isRed == (0 != (rgbcolor & ximg->red_mask))
             && isGreen == (0 != (rgbcolor & ximg->green_mask))
             && isBlue  == (0 != (rgbcolor & ximg->blue_mask))) {
            ++ pixels ;
         }
      }
   }

   XDestroyImage(ximg) ;

   return (pixels > ((uint64_t)x*y/2)) ? 0 : EINVAL ;
}

static int test_draw(native_types_t * native)
{
   x11window_t *  oswin   = &native->oswindow[0];
   eglconfig_t    eglcfg  = native->eglconfig[0];
   eglwindow_t    eglwin  = eglwindow_INIT_FREEABLE;

   // prepare
   TEST(0 == INITOS_EGLWINDOW(&eglwin, gl_display(&native->display), eglcfg, oswin));
   TEST(0 == show_x11window(oswin));
   WAITFOR(&native->display, oswin->state == x11window_state_SHOWN);
   eglWaitNative(EGL_CORE_NATIVE_ENGINE);
   TEST(EGL_TRUE == eglMakeCurrent(gl_display(&native->display), eglwin, eglwin, native->eglcontext));
   glClearColor(1, 0, 1, 1);
   glClear(GL_COLOR_BUFFER_BIT);

   // TEST swapbuffer_eglwindow: makes content visible
   TEST(0 == swapbuffer_eglwindow(eglwin, gl_display(&native->display)));
   eglWaitGL();
   sleepms_thread(300); // wait for compositor
   TEST(0 == compare_color(oswin, 100, 100, 1, 0, 1));

   // unprepare
   TEST(0 == hide_x11window(oswin));
   WAITFOR(&native->display, oswin->state == x11window_state_HIDDEN);
   sleepms_thread(100); // wait for compositor
   TEST(EGL_TRUE == eglMakeCurrent(gl_display(&native->display), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
   TEST(0 == free_eglwindow(&eglwin, gl_display(&native->display)));

   return 0;
ONABORT:
   eglMakeCurrent(gl_display(&native->display), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
   free_eglwindow(&eglwin, gl_display(&native->display));
   return EINVAL;
}

#else

#error "Not implemented for this os specific windowing system"

#endif

static int test_initfree(native_types_t * native)
{
   eglwindow_t eglwin  = eglwindow_INIT_FREEABLE;
   eglconfig_t eglconf = eglconfig_INIT_FREEABLE;

   // TEST eglwindow_INIT_FREEABLE
   TEST(0 == eglwin);

   // TEST init_eglwindow: uninitialized display
   TEST(EINVAL == INITOS_EGLWINDOW(&eglwin, egldisplay_INIT_FREEABLE, native->eglconfig[0], &native->oswindow[0]));
   TEST(0 == eglwin);

   // TEST init_eglwindow: uninitialized config
   TEST(EINVAL == INITOS_EGLWINDOW(&eglwin, gl_display(&native->display), eglconfig_INIT_FREEABLE, &native->oswindow[0]));
   TEST(0 == eglwin);

   // TEST init_eglwindow: uninitialized os window
   TEST(EINVAL == INITOS_EGLWINDOW(&eglwin, gl_display(&native->display), native->eglconfig[0], &native->freeoswindow));
   TEST(0 == eglwin);

   // TEST init_eglwindow: config does not match
   const int32_t pixmap_attr[] = { gconfig_TYPE, gconfig_value_TYPE_PIXMAP_BIT, gconfig_NONE };
   TEST(0 == init_eglconfig(&eglconf, gl_display(&native->display), pixmap_attr));
   TEST(EINVAL == INITOS_EGLWINDOW(&eglwin, gl_display(&native->display), eglconf, &native->oswindow[0]));
   TEST(0 == eglwin);
   TEST(0 == free_eglconfig(&eglconf));

   for (unsigned i = 0; i < lengthof(native->oswindow); ++i) {
      // TEST init_eglwindow: different configurations
      TEST(0 == INITOS_EGLWINDOW(&eglwin, gl_display(&native->display), native->eglconfig[i], &native->oswindow[i]));
      TEST(0 != eglwin);

      // TEST free_eglwindow
      TEST(0 == free_eglwindow(&eglwin, gl_display(&native->display)));
      TEST(0 == eglwin);
      TEST(0 == free_eglwindow(&eglwin, gl_display(&native->display)));
      TEST(0 == eglwin);
   }

   // TEST free_eglwindow: ERROR
   TEST(0 == INITOS_EGLWINDOW(&eglwin, gl_display(&native->display), native->eglconfig[0], &native->oswindow[0]));
   TEST(0 != eglwin);
   init_testerrortimer(&s_eglwindow_errtimer, 1, ENOMEM);
   TEST(ENOMEM == free_eglwindow(&eglwin, gl_display(&native->display)));
   TEST(0 == eglwin);

   return 0;
ONABORT:
   free_eglconfig(&eglconf);
   free_eglwindow(&eglwin, gl_display(&native->display));
   return EINVAL;
}

static int childprocess_unittest(void)
{
   resourceusage_t   usage   = resourceusage_INIT_FREEABLE;
   native_types_t    native  = native_types_INIT_FREEABLE;

   TEST(0 == init_native(&native));

   if (test_draw(&native))       goto ONABORT;

   TEST(0 == init_resourceusage(&usage));

   if (test_initfree(&native))   goto ONABORT;
   if (test_draw(&native))       goto ONABORT;

   TEST(0 == same_resourceusage(&usage));
   TEST(0 == free_resourceusage(&usage));

   TEST(0 == free_native(&native));

   return 0;
ONABORT:
   (void) free_resourceusage(&usage);
   (void) free_native(&native);
   return EINVAL;
}

int unittest_platform_opengl_egl_eglwindow()
{
   int err;

   TEST(0 == execasprocess_unittest(&childprocess_unittest, &err));

   return err;
ONABORT:
   return EINVAL;
}

#endif
