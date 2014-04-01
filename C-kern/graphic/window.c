/* title: Graphic-Window impl

   Implements <Graphic-Window>.

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

   file: C-kern/api/graphic/window.h
    Header file <Graphic-Window>.

   file: C-kern/graphic/window.c
    Implementation file <Graphic-Window impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/graphic/window.h"
#include "C-kern/api/err.h"
#include "C-kern/api/graphic/display.h"
#include "C-kern/api/graphic/surfaceconfig.h"
#include "C-kern/api/platform/OpenGL/EGL/eglconfig.h"
#include "C-kern/api/test/errortimer.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/test/resourceusage.h"
#include "C-kern/api/graphic/surfaceconfig.h"
#include "C-kern/api/graphic/windowconfig.h"
#include "C-kern/api/platform/X11/x11.h"
#include "C-kern/api/platform/task/thread.h"
#endif
#if defined(KONFIG_USERINTERFACE_EGL)
#include STR(C-kern/api/platform/KONFIG_OS/graphic/sysegl.h)
#include STR(C-kern/api/platform/KONFIG_OS/graphic/sysgles2.h)
#endif


// section: window_t

// group: variable
#ifdef KONFIG_UNITTEST
/* variable: s_window_errtimer
 * Allows to introduce artificial errors. */
static test_errortimer_t   s_window_errtimer = test_errortimer_INIT_FREEABLE;
#endif

// group: helper-macros

#if defined(KONFIG_USERINTERFACE_X11) && defined(KONFIG_USERINTERFACE_EGL)

/* define: INIT_OSWINDOW
 * Initializes the native window oswindow in an OS specific way. */
#define INIT_OSWINDOW(oswindow, disp, screennr, visualid, winattr) \
         initvid_x11window(oswindow, os_display(disp), (int32_t)screennr, 0, (uint32_t)visualid, winattr)

/* define: FREE_OSWINDOW
 * Frees oswindow in an OS specific way. */
#define FREE_OSWINDOW(oswindow, disp) \
         free_x11window(oswindow)

/* define: INIT_GLWINDOW
 * Initializes the OpenGL part of win in an OS specific way. */
#define INIT_GLWINDOW(win, disp, surfconf) \
         initx11_eglwindow(&(win)->glwindow, gl_display(disp), surfconf->config, &win->oswindow)

/* define: FREE_GLWINDOW
 * Frees the OpenGL part of the graphic display in an OS specific way. */
#define FREE_GLWINDOW(win, disp) \
         free_eglwindow(&(win)->glwindow, gl_display(disp))

/* define: ISFREE_OSWINDOW
 * Returns true in case the OS specific window is already freed. */
#define ISFREE_OSWINDOW(oswindow) \
         isfree_x11window(oswindow)

#else

#error "Not implemented"

#endif

// group: lifetime

int init_window(/*out*/window_t * win, display_t * disp, uint32_t screennr, surfaceconfig_t * surfconf, struct windowconfig_t * winattr)
{
   int      err;
   int      isinit = 0;
   int32_t  visualid;

   ONERROR_testerrortimer(&s_window_errtimer, &err, ONABORT);

   err = visualid_surfaceconfig(surfconf, disp, &visualid);
   if (err) goto ONABORT;

   err = INIT_OSWINDOW(&win->oswindow, disp, screennr, visualid, winattr);
   if (err) goto ONABORT;
   ++ isinit;

   ONERROR_testerrortimer(&s_window_errtimer, &err, ONABORT);

   err = INIT_GLWINDOW(win, disp, surfconf);
   if (err) goto ONABORT;

   return 0;
ONABORT:
   if (isinit) {
      FREE_OSWINDOW(&win->oswindow, disp);
   }
   return err;
}

int free_window(window_t * win, display_t * disp)
{
   int err;
   int err2;

   if (!ISFREE_OSWINDOW(&win->oswindow)) {
      err  = FREE_GLWINDOW(win, disp);
      SETONERROR_testerrortimer(&s_window_errtimer, &err);
      err2 = FREE_OSWINDOW(&win->oswindow, disp);
      SETONERROR_testerrortimer(&s_window_errtimer, &err2);
      if (err2) err = err2;
      if (err) goto ONABORT;
   }

   return 0;
ONABORT:
   TRACEABORTFREE_ERRLOG(err);
   return err;
}


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

#if defined(KONFIG_USERINTERFACE_X11) && defined(KONFIG_USERINTERFACE_EGL)

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

static int test_transparentalpha(display_t * disp)
{
   int32_t           surfattr[] = {
                        surfaceconfig_TRANSPARENT_ALPHA, 1,
                        surfaceconfig_BITS_BUFFER, 32, surfaceconfig_NONE
                     };
   windowconfig_t    winattr[]  = {
                        windowconfig_INIT_TITLE("test-graphic-window"),
                        windowconfig_INIT_SIZE(100, 100),
                        windowconfig_INIT_POS(50, 100),
                        windowconfig_INIT_NONE
                     };
   window_t          top      = window_INIT_FREEABLE;
   window_t          bottom   = window_INIT_FREEABLE;
   surfaceconfig_t   surfconf = surfaceconfig_INIT_FREEABLE;
   uint32_t          snr      = defaultscreennr_display(disp);
   EGLContext        eglcontext = EGL_NO_CONTEXT;

   // prepare
   TEST(0 == init_surfaceconfig(&surfconf, disp, surfattr));
   eglcontext = eglCreateContext(gl_display(disp), surfconf.config, EGL_NO_CONTEXT, 0);
   TEST(EGL_NO_CONTEXT != eglcontext);

   // TEST init_window: surfaceconfig_TRANSPARENT_ALPHA: draw overlay on top of bottom
   TEST(0 == init_window(&top, disp, snr, &surfconf, winattr));
   TEST(0 == init_window(&bottom, disp, snr, &surfconf, winattr));

   TEST(0 == show_x11window(os_window(&bottom)));
   WAITFOR(disp, os_window(&bottom)->state == x11window_state_SHOWN);
   TEST(EGL_TRUE == eglMakeCurrent(gl_display(disp), (void*)gl_window(&bottom), (void*)gl_window(&bottom), eglcontext));
   glClearColor(1, 0, 0, 1);
   glClear(GL_COLOR_BUFFER_BIT);
   eglSwapBuffers(gl_display(disp), (void*)gl_window(&bottom));
   eglWaitGL();
   sleepms_thread(100); // wait for compositor
   // red color
   TEST(0 == compare_color(os_window(&bottom), 100, 100, 1, 0, 0));
   TEST(0 == show_x11window(os_window(&top)));
   WAITFOR(disp, os_window(&top)->state == x11window_state_SHOWN);
   TEST(EGL_TRUE == eglMakeCurrent(gl_display(disp), (void*)gl_window(&top), (void*)gl_window(&top), eglcontext));
   glClearColor(0, 0, 1, 0); // transparent blue
   glClear(GL_COLOR_BUFFER_BIT);
   eglSwapBuffers(gl_display(disp), (void*)gl_window(&top));
   eglWaitGL();
   sleepms_thread(100); // wait for compositor
   // resulting color is the combination of red and blue
   TEST(0 == compare_color(os_window(&bottom), 100, 100, 1, 0, 1));

   // unprepare
	TEST(EGL_TRUE == eglMakeCurrent(gl_display(disp), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
	TEST(EGL_TRUE == eglDestroyContext(gl_display(disp), eglcontext));
   TEST(0 == free_surfaceconfig(&surfconf));
   TEST(0 == free_window(&bottom, disp));
   TEST(0 == free_window(&top, disp));
   WAITFOR(disp, false);
   eglReleaseThread();

   return 0;
ONABORT:
   eglMakeCurrent(gl_display(disp), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
   if (eglcontext != EGL_NO_CONTEXT) eglDestroyContext(gl_display(disp), eglcontext);
   free_surfaceconfig(&surfconf);
   free_window(&top, disp);
   free_window(&bottom, disp);
   return EINVAL;
}

static bool isfree_helper(const window_t * win)
{
   return   1 == isfree_x11window(&win->oswindow)
            && 0 == win->glwindow;
}

static void acceptleak_helper(resourceusage_t * usage)
{
   // EGL window / X11 window / EGLContext has resource leak
   acceptmallocleak_resourceusage(usage, 500);
}

#else
   #error "Not implemented"
#endif

static int test_initfree(display_t * disp)
{
   window_t          win      = window_INIT_FREEABLE;
   surfaceconfig_t   surfconf = surfaceconfig_INIT_FREEABLE;
   int32_t           surfattr[4][7] = {
         { surfaceconfig_TYPE, surfaceconfig_value_TYPE_WINDOW_BIT, surfaceconfig_BITS_RED, 8, surfaceconfig_NONE },
         { surfaceconfig_TYPE, surfaceconfig_value_TYPE_WINDOW_BIT, surfaceconfig_BITS_ALPHA, 1, surfaceconfig_BITS_STENCIL, 1, surfaceconfig_NONE },
         { surfaceconfig_TYPE, surfaceconfig_value_TYPE_WINDOW_BIT, surfaceconfig_BITS_BUFFER, 24, surfaceconfig_BITS_DEPTH, 1, surfaceconfig_NONE },
         { surfaceconfig_TYPE, surfaceconfig_value_TYPE_WINDOW_BIT, surfaceconfig_CONFORMANT, surfaceconfig_value_CONFORMANT_ES2_BIT|surfaceconfig_value_CONFORMANT_OPENGL_BIT, surfaceconfig_NONE },
   };
   windowconfig_t winattr[] = {
         windowconfig_INIT_FRAME, windowconfig_INIT_TRANSPARENCY(255), windowconfig_INIT_TITLE("name"),
         windowconfig_INIT_MINSIZE(10,10), windowconfig_INIT_MAXSIZE(1000,1000),
         windowconfig_INIT_SIZE(1000,1000), windowconfig_INIT_POS(333,444), windowconfig_INIT_NONE
   };

   // TEST window_INIT_FREEABLE
   TEST(1 == isfree_helper(&win));
   TEST(1 == ISFREE_OSWINDOW(&win.oswindow));

   for (unsigned i = 0; i < lengthof(surfattr); ++i) {
      TEST(0 == init_surfaceconfig(&surfconf, disp, surfattr[i]));

      // TEST init_window
      TEST(0 == init_window(&win, disp, defaultscreennr_display(disp), &surfconf, winattr));

      // TEST free_window
      TEST(0 == free_window(&win, disp));
      TEST(1 == isfree_helper(&win));
      TEST(1 == ISFREE_OSWINDOW(&win.oswindow));
      TEST(0 == free_window(&win, disp));
      TEST(1 == isfree_helper(&win));
      TEST(1 == ISFREE_OSWINDOW(&win.oswindow));

      TEST(0 == free_surfaceconfig(&surfconf));
   }

   // prepare
   TEST(0 == init_surfaceconfig(&surfconf, disp, surfattr[0]));

   // TEST init_window: E2BIG
   windowconfig_t winattr2big[3*windowconfig_NROFELEMENTS+2];
   for (unsigned i = 0; i < lengthof(winattr2big)-1; ++i) {
      winattr2big[i] = (windowconfig_t)windowconfig_INIT_FRAME;
   }
   winattr2big[lengthof(winattr2big)-1] = (windowconfig_t) windowconfig_INIT_NONE;
   TEST(E2BIG == init_window(&win, disp, defaultscreennr_display(disp), &surfconf, winattr2big));
   TEST(0 == free_window(&win, disp));
   TEST(1 == isfree_helper(&win));

   // TEST init_window: ERROR
   for (unsigned i = 1; i <= 2; ++i) {
      init_testerrortimer(&s_window_errtimer, i, ENOMEM);
      TEST(ENOMEM == init_window(&win, disp, defaultscreennr_display(disp), &surfconf, winattr));
      TEST(0 == free_window(&win, disp));
      TEST(1 == isfree_helper(&win));
   }

   // TEST free_window: ERROR
   for (unsigned i = 1; i <= 2; ++i) {
      TEST(0 == init_window(&win, disp, defaultscreennr_display(disp), &surfconf, winattr));
      init_testerrortimer(&s_window_errtimer, i, ENOMEM);
      TEST(ENOMEM == free_window(&win, disp));
      TEST(0 == free_window(&win, disp));
      TEST(1 == isfree_helper(&win));
   }

   // unprepare
   TEST(0 == free_surfaceconfig(&surfconf));

   return 0;
ONABORT:
   free_surfaceconfig(&surfconf);
   free_window(&win, disp);
   return EINVAL;
}

static int childprocess_unittest(void)
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE;
   display_t         disp  = display_INIT_FREEABLE;

   // prepare
   TEST(0 == initdefault_display(&disp))

   if (test_transparentalpha(&disp))   goto ONABORT;

   TEST(0 == init_resourceusage(&usage));

   if (test_initfree(&disp))           goto ONABORT;

   acceptleak_helper(&usage);

   TEST(0 == same_resourceusage(&usage));
   TEST(0 == free_resourceusage(&usage));

   // unprepare
   TEST(0 == free_display(&disp))

   return 0;
ONABORT:
   (void) free_resourceusage(&usage);
   (void) free_display(&disp);
   return EINVAL;
}

int unittest_graphic_window()
{
   int err;

   TEST(0 == execasprocess_unittest(&childprocess_unittest, &err));

   return err;
ONABORT:
   return EINVAL;
}

#endif
