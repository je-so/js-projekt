/* title: Graphic-Window impl

   Implements <Graphic-Window>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

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
#include "C-kern/api/graphic/gconfig.h"
#include "C-kern/api/test/errortimer.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/test/resourceusage.h"
#include "C-kern/api/graphic/gconfig.h"
#include "C-kern/api/graphic/gles2api.h"
#include "C-kern/api/graphic/windowconfig.h"
#include "C-kern/api/platform/X11/x11.h"
#include "C-kern/api/platform/task/thread.h"
#endif
#if defined(KONFIG_USERINTERFACE_EGL)
#include STR(C-kern/api/platform/KONFIG_OS/graphic/sysegl.h)
#endif


// section: window_t

// group: static variables

#ifdef KONFIG_UNITTEST
/* variable: s_window_errtimer
 * Allows to introduce artificial errors during test. */
static test_errortimer_t   s_window_errtimer = test_errortimer_FREE;
#endif

// group: helper-macros

#if defined(KONFIG_USERINTERFACE_X11) && defined(KONFIG_USERINTERFACE_EGL)

/* define: INIT_OSWINDOW
 * Initializes the native window oswindow in an OS specific way. */
#define INIT_OSWINDOW(oswindow, disp, screennr, eventhandler, visualid, winattr) \
         initvid_x11window(oswindow, os_display(disp), screennr, cast_x11windowevh(eventhandler, window_t), (uint32_t)visualid, winattr)

/* define: FREE_OSWINDOW
 * Frees oswindow in an OS specific way. */
#define FREE_OSWINDOW(oswindow) \
         free_x11window(oswindow)

/* define: INIT_GLWINDOW
 * Initializes the OpenGL part of win in an OS specific way. */
#define INIT_GLWINDOW(glwin, disp, gconf, oswindow) \
         init_eglwindow(glwin, gl_display(disp), gl_gconfig(gconf), syswindow_x11window(oswindow))

/* define: FREE_GLWINDOW
 * Frees the OpenGL part of the graphic display in an OS specific way. */
#define FREE_GLWINDOW(glwin, disp) \
         free_eglwindow(glwin, gl_display(disp))

/* define: ISFREE_OSWINDOW
 * Returns true in case the OS specific window is already freed. */
#define ISFREE_OSWINDOW(oswindow) \
         isfree_x11window(oswindow)

#else

#error "Not implemented"

#endif

// group: lifetime

int init_window(
      /*out*/window_t       * win,
      display_t             * disp,
      uint32_t                screennr,
      const
      struct window_evh_t   * eventhandler,
      gconfig_t             * gconf,
      struct windowconfig_t * winattr)
{
   int      err;
   int      isinit = 0;
   int32_t  visualid;

   static_assert(win == (void*)&win->oswindow, "window_t is subtype of oswindow type");

   ONERROR_testerrortimer(&s_window_errtimer, &err, ONERR);

   err = visualid_gconfig(gconf, disp, &visualid);
   if (err) goto ONERR;

   err = INIT_OSWINDOW(os_window(win), disp, screennr, eventhandler, visualid, winattr);
   if (err) goto ONERR;
   ++ isinit;

   ONERROR_testerrortimer(&s_window_errtimer, &err, ONERR);

   err = INIT_GLWINDOW(&gl_window(win), disp, gconf, os_window(win));
   if (err) goto ONERR;

   return 0;
ONERR:
   if (isinit) {
      FREE_OSWINDOW(&win->oswindow);
   }
   return err;
}

int free_window(window_t * win)
{
   int err;
   int err2;

   if (0 != display_window(win)) {
      err  = FREE_GLWINDOW(&gl_window(win), display_window(win));
      SETONERROR_testerrortimer(&s_window_errtimer, &err);

      err2 = FREE_OSWINDOW(os_window(win));
      SETONERROR_testerrortimer(&s_window_errtimer, &err2);
      if (err2) err = err2;

      if (err) goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

#define WINPOS_INIT_X 250
#define WINPOS_INIT_Y 100

#if defined(KONFIG_USERINTERFACE_X11) && defined(KONFIG_USERINTERFACE_EGL)

/* define: WAITFOR
 * Reads and handles window events until CONDITION is met.
 * Parameter disp must be of type <display_t>. */
#define WAITFOR(disp, CONDITION) \
   XFlush(os_display(disp)->sys_display);          \
   for (int _count = 0; _count < 100; ++_count) {  \
      while (XPending(os_display(disp)->sys_display)) {  \
         dispatchevent_X11(os_display(disp));      \
      }                                            \
      if (CONDITION) break;                        \
      sleepms_thread(5);                           \
   }

static int compare_color2(x11window_t * x11win, uint32_t w, uint32_t h, bool isRed, bool isGreen, bool isBlue)
{
   XImage * ximg = 0;
   size_t   pixels;
   uint32_t x, y;

   Window root = RootWindow(x11win->display->sys_display, screen_x11window(x11win));
   Window windummy;
   int32_t x2, y2;
   XTranslateCoordinates(x11win->display->sys_display, x11win->sys_drawable, root, 0, 0, &x2, &y2, &windummy);
   ximg = XGetImage(x11win->display->sys_display, root, x2, y2, w, h, (unsigned long)-1, ZPixmap);

   for (pixels = 0, y = 0; y < h; ++y) {
      for (x = 0; x < w; ++x) {
         unsigned long rgbcolor = XGetPixel(ximg, (int)x, (int)y);
         if (isRed == (0 != (rgbcolor & ximg->red_mask))
             && isGreen == (0 != (rgbcolor & ximg->green_mask))
             && isBlue  == (0 != (rgbcolor & ximg->blue_mask))) {
            ++ pixels;
         }
      }
   }

   XDestroyImage(ximg);

   return (pixels > ((uint64_t)x*y/2)) ? 0 : EINVAL;
}

static int compare_color(x11window_t * x11win, uint32_t w, uint32_t h, bool isRed, bool isGreen, bool isBlue)
{
   for (unsigned i = 0; i < 20; ++i) {
      sleepms_thread(10); // wait for compositor
      if (0 == compare_color2(x11win, w, h, isRed, isGreen, isBlue))
         return 0;
   }

   return EINVAL;
}

static int test_transparentalpha(display_t * disp)
{
   int32_t           surfattr[] = {
                        gconfig_TRANSPARENT_ALPHA, 1,
                        gconfig_BITS_BUFFER, 32, gconfig_NONE
                     };
   windowconfig_t    winattr[]  = {
                        windowconfig_INIT_TITLE("test-graphic-window"),
                        windowconfig_INIT_SIZE(100, 100),
                        windowconfig_INIT_POS(50, 100),
                        windowconfig_INIT_NONE
                     };
   window_t    top      = window_FREE;
   window_t    bottom   = window_FREE;
   gconfig_t   gconf = gconfig_FREE;
   uint32_t    snr      = defaultscreennr_display(disp);
   EGLContext  eglcontext = EGL_NO_CONTEXT;

   // prepare
   TEST(0 == init_gconfig(&gconf, disp, surfattr));
   eglcontext = eglCreateContext(gl_display(disp), gl_gconfig(&gconf), EGL_NO_CONTEXT, 0);
   TEST(EGL_NO_CONTEXT != eglcontext);

   // TEST init_window: gconfig_TRANSPARENT_ALPHA: draw overlay on top of bottom
   TEST(0 == init_window(&top, disp, snr, 0, &gconf, winattr));
   TEST(0 == init_window(&bottom, disp, snr, 0, &gconf, winattr));

   // TEST swapbuffer_window: bottom window opaque color
   TEST(0 == show_x11window(os_window(&bottom)));
   WAITFOR(disp, os_window(&bottom)->state == x11window_state_SHOWN);
   TEST(EGL_TRUE == eglMakeCurrent(gl_display(disp), (void*)gl_window(&bottom), (void*)gl_window(&bottom), eglcontext));
   glClearColor(1, 0, 0, 1);
   glClear(GL_COLOR_BUFFER_BIT);
   TEST(0 == swapbuffer_window(&bottom, disp));
   eglWaitGL();
   // red color
   TEST(0 == compare_color(os_window(&bottom), 100, 100, 1, 0, 0));

   // TEST swapbuffer_window: top window with transparent value
   TEST(0 == show_x11window(os_window(&top)));
   WAITFOR(disp, os_window(&top)->state == x11window_state_SHOWN);
   TEST(EGL_TRUE == eglMakeCurrent(gl_display(disp), (void*)gl_window(&top), (void*)gl_window(&top), eglcontext));
   glClearColor(0, 0, 1, 0); // transparent blue
   glClear(GL_COLOR_BUFFER_BIT);
   TEST(0 == swapbuffer_window(&top, disp));
   eglWaitGL();
   // resulting color is the combination of red and blue
   TEST(0 == compare_color(os_window(&bottom), 100, 100, 1, 0, 1));

   // unprepare
	TEST(EGL_TRUE == eglMakeCurrent(gl_display(disp), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
	TEST(EGL_TRUE == eglDestroyContext(gl_display(disp), eglcontext));
   TEST(0 == free_gconfig(&gconf));
   TEST(0 == free_window(&bottom));
   TEST(0 == free_window(&top));
   WAITFOR(disp, false);
   eglReleaseThread();

   return 0;
ONERR:
   eglMakeCurrent(gl_display(disp), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
   if (eglcontext != EGL_NO_CONTEXT) eglDestroyContext(gl_display(disp), eglcontext);
   free_gconfig(&gconf);
   free_window(&top);
   free_window(&bottom);
   return EINVAL;
}

static bool isfree_helper(const window_t * win)
{
   return   1 == isfree_x11window(&win->oswindow)
            && 1 == isfree_surface(win);
}

static void acceptleak_helper(resourceusage_t * usage)
{
   // EGL window / X11 window / EGLContext has resource leak
   acceptmallocleak_resourceusage(usage, 500);
}

static int init_test_window(/*out*/window_t * win, /*out*/EGLContext * eglcontext, display_t * disp, window_evh_t * evhandler)
{
   gconfig_t gconf      = gconfig_FREE;
   int32_t   surfattr[] = {
                  gconfig_BITS_BUFFER, 32, gconfig_BITS_ALPHA, 1,
                  gconfig_TYPE, gconfig_value_TYPE_WINDOW_BIT,
                  gconfig_NONE
             };
   windowconfig_t winattr[] = {
                     windowconfig_INIT_POS(WINPOS_INIT_X, WINPOS_INIT_Y),
                     windowconfig_INIT_SIZE(100, 100),
                     windowconfig_INIT_FRAME,
                     windowconfig_INIT_NONE
                  };

   TEST(0 == init_gconfig(&gconf, disp, surfattr));
   TEST(0 == init_window(win, disp, defaultscreennr_display(disp), evhandler, &gconf, winattr));
   *eglcontext = eglCreateContext(gl_display(disp), gl_gconfig(&gconf), EGL_NO_CONTEXT, (EGLint[]){EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE});
   TEST(EGL_NO_CONTEXT != *eglcontext);
   TEST(EGL_TRUE == eglMakeCurrent(gl_display(disp), (void*)gl_window(win), (void*)gl_window(win), *eglcontext));

   return 0;
ONERR:
   free_gconfig(&gconf);
   return EINVAL;
}

#else
   #error "Not implemented"
#endif

static int test_initfree(display_t * disp)
{
   window_t    win   = window_FREE;
   gconfig_t   gconf = gconfig_FREE;
   int32_t     surfattr[4][7] = {
         { gconfig_TYPE, gconfig_value_TYPE_WINDOW_BIT, gconfig_BITS_RED, 8, gconfig_NONE },
         { gconfig_TYPE, gconfig_value_TYPE_WINDOW_BIT, gconfig_BITS_ALPHA, 1, gconfig_BITS_STENCIL, 1, gconfig_NONE },
         { gconfig_TYPE, gconfig_value_TYPE_WINDOW_BIT, gconfig_BITS_BUFFER, 24, gconfig_BITS_DEPTH, 1, gconfig_NONE },
         { gconfig_TYPE, gconfig_value_TYPE_WINDOW_BIT, gconfig_CONFORMANT, gconfig_value_CONFORMANT_ES2_BIT|gconfig_value_CONFORMANT_OPENGL_BIT, gconfig_NONE },
   };
   windowconfig_t winattr[] = {
         windowconfig_INIT_FRAME, windowconfig_INIT_TRANSPARENCY(255), windowconfig_INIT_TITLE("name"),
         windowconfig_INIT_MINSIZE(10,10), windowconfig_INIT_MAXSIZE(1000,1000),
         windowconfig_INIT_SIZE(1000,1000), windowconfig_INIT_POS(333,444), windowconfig_INIT_NONE
   };

   // TEST window_FREE
   TEST(1 == isfree_helper(&win));
   TEST(1 == ISFREE_OSWINDOW(&win.oswindow));

   for (unsigned i = 0; i < lengthof(surfattr); ++i) {
      TEST(0 == init_gconfig(&gconf, disp, surfattr[i]));

      // TEST init_window
      TEST(0 == init_window(&win, disp, defaultscreennr_display(disp), 0, &gconf, winattr));
      TEST(0 != gl_window(&win));

      // TEST free_window
      TEST(0 == free_window(&win));
      TEST(1 == isfree_helper(&win));
      TEST(1 == ISFREE_OSWINDOW(&win.oswindow));
      TEST(0 == free_window(&win));
      TEST(1 == isfree_helper(&win));
      TEST(1 == ISFREE_OSWINDOW(&win.oswindow));

      TEST(0 == free_gconfig(&gconf));
   }

   // prepare
   TEST(0 == init_gconfig(&gconf, disp, surfattr[0]));

   // TEST init_window: E2BIG
   windowconfig_t winattr2big[3*windowconfig_NROFELEMENTS+2];
   for (unsigned i = 0; i < lengthof(winattr2big)-1; ++i) {
      winattr2big[i] = (windowconfig_t)windowconfig_INIT_FRAME;
   }
   winattr2big[lengthof(winattr2big)-1] = (windowconfig_t) windowconfig_INIT_NONE;
   TEST(E2BIG == init_window(&win, disp, defaultscreennr_display(disp), 0, &gconf, winattr2big));
   TEST(0 == free_window(&win));
   TEST(1 == isfree_helper(&win));

   // TEST init_window: ERROR
   for (unsigned i = 1; i <= 2; ++i) {
      init_testerrortimer(&s_window_errtimer, i, ENOMEM);
      TEST(ENOMEM == init_window(&win, disp, defaultscreennr_display(disp), 0, &gconf, winattr));
      TEST(0 == free_window(&win));
      TEST(1 == isfree_helper(&win));
   }

   // TEST free_window: ERROR
   for (unsigned i = 1; i <= 2; ++i) {
      TEST(0 == init_window(&win, disp, defaultscreennr_display(disp), 0, &gconf, winattr));
      init_testerrortimer(&s_window_errtimer, i, ENOMEM);
      TEST(ENOMEM == free_window(&win));
      TEST(0 == free_window(&win));
      TEST(1 == isfree_helper(&win));
   }

   // unprepare
   TEST(0 == free_gconfig(&gconf));

   return 0;
ONERR:
   free_gconfig(&gconf);
   free_window(&win);
   return EINVAL;
}

static int test_showhide(window_t * win, display_t * disp)
{
   // TEST show_window
   TEST(0 == isvisible_window(win));
   TEST(0 == show_window(win));
   WAITFOR(disp, isvisible_window(win));
   TEST(1 == isvisible_window(win));

   // TEST hide_window
   TEST(1 == isvisible_window(win));
   TEST(0 == hide_window(win));
   WAITFOR(disp, ! isvisible_window(win));
   TEST(0 == isvisible_window(win));

   return 0;
ONERR:
   return EINVAL;
}

static int test_position(window_t * win, display_t * disp)
{
   int32_t x;
   int32_t y;
   int32_t dx; // stores the offset in case of windowsconfig_FRAME
   int32_t dy; // stores the offset in case of windowsconfig_FRAME

   // prepare
   TEST(0 == show_window(win));
   WAITFOR(disp, isvisible_window(win));
   TEST(0 == setpos_window(win, WINPOS_INIT_X+10, WINPOS_INIT_Y+10));
   TEST(0 == setpos_window(win, WINPOS_INIT_X, WINPOS_INIT_Y));
   WAITFOR(disp, pos_window(win, &x, &y) == 0 && x >= WINPOS_INIT_X+10);

   TEST(0 == pos_window(win, &x, &y));
   dx = x - WINPOS_INIT_X;
   dy = y - WINPOS_INIT_Y;
   TEST(0 <= dx && dx <= 10);
   TEST(0 <= dy && dy <= 30);

   // TEST pos_window
   TEST(0 == pos_window(win, &x, &y));
   TEST(WINPOS_INIT_X == x - dx);
   TEST(WINPOS_INIT_Y == y - dy);

   // TEST setpos_window
   TEST(0 == setpos_window(win, 200, 180));
   WAITFOR(disp, pos_window(win, &x, &y) == 0 && x == 200 + dx);
   WAITFOR(disp, pos_window(win, &x, &y) == 0 && y == 180 + dy);
   TEST(0 == pos_window(win, &x, &y));
   TEST(200 == x - dx);
   TEST(180 == y - dy);
   TEST(0 == setpos_window(win, WINPOS_INIT_X, WINPOS_INIT_Y));
   WAITFOR(disp, pos_window(win, &x, &y) == 0 && x == WINPOS_INIT_X + dx);
   TEST(0 == pos_window(win, &x, &y));
   TEST(WINPOS_INIT_X == x - dx);
   TEST(WINPOS_INIT_Y == y - dy);

   return 0;
ONERR:
   return EINVAL;
}

static int test_resize(window_t * win, display_t * disp)
{
   uint32_t w;
   uint32_t h;

   // prepare
   TEST(0 == show_window(win));
   WAITFOR(disp, isvisible_window(win));

   // TEST size_window
   TEST(0 == size_window(win, &w, &h));
   TEST(100 == w);
   TEST(100 == h);

   // TEST resize_window
   TEST(0 == resize_window(win, 200, 150));
   WAITFOR(disp, size_window(win, &w, &h) == 0 && w == 200);
   TEST(0 == size_window(win, &w, &h));
   TEST(200 == w);
   TEST(150 == h);
   TEST(0 == resize_window(win, 100, 100));
   WAITFOR(disp, size_window(win, &w, &h) == 0 && w == 100);
   TEST(0 == size_window(win, &w, &h));
   TEST(100 == w);
   TEST(100 == h);

   // unprepare
   TEST(0 == hide_window(win));
   WAITFOR(disp, ! isvisible_window(win));
   TEST(0 == isvisible_window(win));

   return 0;
ONERR:
   return EINVAL;
}

static int childprocess_unittest(void)
{
   resourceusage_t   usage = resourceusage_FREE;
   display_t         disp  = display_FREE;
   window_t          win   = window_FREE;
   window_evh_t      evhandler = window_evh_INIT_NULL;
   EGLContext        eglcontext = EGL_NO_CONTEXT;

   // prepare
   TEST(0 == initdefault_display(&disp))
   TEST(0 == init_test_window(&win, &eglcontext, &disp, &evhandler));

   if (test_transparentalpha(&disp))   goto ONERR;

   TEST(0 == init_resourceusage(&usage));

   if (test_initfree(&disp))           goto ONERR;

   acceptleak_helper(&usage);
   TEST(0 == same_resourceusage(&usage));
   TEST(0 == free_resourceusage(&usage));

   WAITFOR(&disp, false);
   size_t    logsize;
   uint8_t * logbuffer;
   GETBUFFER_ERRLOG(&logbuffer, &logsize);
   for (unsigned i = 0; i <= 2; ++i) {
      TEST(0 == init_resourceusage(&usage));

      if (test_showhide(&win, &disp))     goto ONERR;
      if (test_position(&win, &disp))     goto ONERR;
      if (test_resize(&win, &disp))       goto ONERR;

      WAITFOR(&disp, false);
      if (0 == same_resourceusage(&usage)) break;
      TEST(0 == free_resourceusage(&usage));
      TRUNCATEBUFFER_ERRLOG(logsize);
   }
   TEST(0 == same_resourceusage(&usage));
   TEST(0 == free_resourceusage(&usage));

   // unprepare
   TEST(0 == free_window(&win));
   TEST(0 == free_display(&disp));

   return 0;
ONERR:
   (void) free_resourceusage(&usage);
   (void) free_window(&win);
   (void) free_display(&disp);
   return EINVAL;
}

int unittest_graphic_window()
{
   int err;

   TEST(0 == execasprocess_unittest(&childprocess_unittest, &err));

   return err;
ONERR:
   return EINVAL;
}

#endif
