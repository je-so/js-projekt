/* title: EGL-Window impl

   Implements <EGL-Window>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

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

// group: static variables

#ifdef KONFIG_UNITTEST
/* variable: s_eglwindow_errtimer
 * Allows to introduce artificial errors during test. */
static test_errortimer_t   s_eglwindow_errtimer = test_errortimer_FREE ;
#endif

// group: lifetime

int init_eglwindow(/*out*/eglwindow_t * eglwin, egldisplay_t egldisp, eglconfig_t eglconf, struct sys_window_t * syswin)
{
   int err;

   VALIDATE_INPARAM_TEST(syswin != 0, ONERR, );

   EGLint     attrib[] = { EGL_NONE };
   EGLSurface window   = eglCreateWindowSurface(egldisp, eglconf, (EGLNativeWindowType) syswin, attrib);

   if (EGL_NO_SURFACE == window) {
      err = convert2errno_egl(eglGetError());
      goto ONERR;
   }

   *eglwin = window;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int free_eglwindow(eglwindow_t * eglwin, egldisplay_t egldisp)
{
   int err;

   if (*eglwin) {
      EGLBoolean isDestoyed = eglDestroySurface(egldisp, *eglwin);

      *eglwin = 0;

      if (EGL_FALSE == isDestoyed) {
         err = convert2errno_egl(eglGetError());
         goto ONERR;
      }

      ONERROR_testerrortimer(&s_eglwindow_errtimer, &err, ONERR);
   }

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

// group: update

int swapbuffer_eglwindow(eglwindow_t eglwin, egldisplay_t egldisp)
{
   int err;

   if (EGL_FALSE == eglSwapBuffers(egldisp, eglwin)) {
      err = convert2errno_egl(eglGetError());
      goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

typedef struct native_types_t native_types_t;

#if defined(KONFIG_USERINTERFACE_X11)

struct native_types_t {
   display_t      display;
   x11window_t    freeoswindow;
   x11window_t    oswindow[4];
   eglconfig_t    eglconfig[4];
   struct {
      x11window_t    oswindow;
      eglconfig_t    eglconfig;
      EGLContext     eglcontext;
      eglwindow_t    eglwin;
   } draw;
};

#define native_types_FREE  \
         {  display_FREE,  \
            x11window_FREE, \
            { x11window_FREE, x11window_FREE, x11window_FREE, x11window_FREE }, \
            { eglconfig_FREE, eglconfig_FREE, eglconfig_FREE, eglconfig_FREE }, \
            { x11window_FREE, eglconfig_FREE, EGL_NO_CONTEXT, eglwindow_FREE }, \
         }

static int init_native(/*out*/native_types_t * native)
{
   int32_t     config_attr[4][5] = {
      { gconfig_BITS_RED,    1,  gconfig_NONE, 0, 0 },
      { gconfig_BITS_DEPTH,  1,  gconfig_NONE, 0, 0 },
      { gconfig_BITS_ALPHA,  1,  gconfig_NONE, 0, 0 },
      { gconfig_TRANSPARENT_ALPHA, 1, gconfig_BITS_BUFFER, 32, gconfig_NONE },
   };
   uint32_t    snr = 0;
   gconfig_t   gconf = gconfig_FREE;
   int32_t     visualid;

   windowconfig_t winattr[] = {
      windowconfig_INIT_TITLE("egl-test-window"),
      windowconfig_INIT_SIZE(100, 100),
      windowconfig_INIT_POS(50, 100),
      windowconfig_INIT_NONE
   };

   TEST(0 == initdefault_display(&native->display));
   snr = defaultscreennr_display(&native->display);

   for (unsigned i = 0; i < lengthof(native->oswindow); ++i) {
      TEST(0 == init_gconfig(&gconf, &native->display, config_attr[i]));
      TEST(0 == visualid_gconfig(&gconf, &native->display, &visualid));
      TEST(0 == initvid_x11window(&native->oswindow[i], os_display(&native->display), snr, 0, (uint32_t)visualid, winattr));
      native->eglconfig[i] = gl_gconfig(&gconf);
   }

   TEST(0 == init_gconfig(&gconf, &native->display, (int32_t[]){ gconfig_BITS_BUFFER, 32, gconfig_NONE }));
   TEST(0 == visualid_gconfig(&gconf, &native->display, &visualid));
   TEST(0 == initvid_x11window(&native->draw.oswindow, os_display(&native->display), snr, 0, (uint32_t)visualid, winattr));
   native->draw.eglconfig  = gl_gconfig(&gconf);
   native->draw.eglcontext = eglCreateContext(gl_display(&native->display), native->draw.eglconfig, EGL_NO_CONTEXT, 0);
   TEST(EGL_NO_CONTEXT != native->draw.eglcontext);

   TEST(0 == init_eglwindow(&native->draw.eglwin, gl_display(&native->display), native->draw.eglconfig, syswindow_x11window(&native->draw.oswindow)));

   TEST(EGL_TRUE == eglMakeCurrent(gl_display(&native->display), native->draw.eglwin, native->draw.eglwin, native->draw.eglcontext));

   return 0;
ONERR:
   return EINVAL;
}

static int free_native(native_types_t * native)
{
	TEST(EGL_TRUE == eglMakeCurrent(gl_display(&native->display), EGL_NO_SURFACE, EGL_NO_SURFACE, native->draw.eglcontext));
   TEST(0 == free_eglwindow(&native->draw.eglwin, gl_display(&native->display)));
   TEST(0 == free_eglconfig(&native->draw.eglconfig));
   TEST(0 == free_x11window(&native->draw.oswindow));
   for (unsigned i = 0; i < lengthof(native->oswindow); ++i) {
      TEST(0 == free_eglconfig(&native->eglconfig[i]));
      TEST(0 == free_x11window(&native->oswindow[i]));
   }
	TEST(EGL_TRUE == eglDestroyContext(gl_display(&native->display), native->draw.eglcontext));
	TEST(EGL_TRUE == eglMakeCurrent(gl_display(&native->display), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
   TEST(0 == free_display(&native->display));
   return 0;
ONERR:
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
   x11window_t *  oswin   = &native->draw.oswindow;
   eglwindow_t    eglwin  = native->draw.eglwin;

   // prepare
   TEST(0 == show_x11window(oswin));
   WAITFOR(&native->display, oswin->state == x11window_state_SHOWN);
   glClearColor(1, 0, 1, 1);
   glClear(GL_COLOR_BUFFER_BIT);

   // TEST swapbuffer_eglwindow: makes content visible
   TEST(0 == swapbuffer_eglwindow(eglwin, gl_display(&native->display)));
   eglWaitGL();
   sleepms_thread(300); // wait for compositor
   TEST(0 == compare_color(oswin, 100, 100, 1, 0, 1));

   return 0;
ONERR:
   return EINVAL;
}

#else

#error "Not implemented for this os specific windowing system"

#endif

static int test_initfree(native_types_t * native)
{
   eglwindow_t eglwin  = eglwindow_FREE;
   eglconfig_t eglconf = eglconfig_FREE;

   // TEST eglwindow_FREE
   TEST(0 == eglwin);

   // TEST init_eglwindow: uninitialized display
   TEST(EINVAL == init_eglwindow(&eglwin, egldisplay_FREE, native->eglconfig[0], syswindow_x11window(&native->oswindow[0])));
   TEST(0 == eglwin);

   // TEST init_eglwindow: uninitialized config
   TEST(EINVAL == init_eglwindow(&eglwin, gl_display(&native->display), eglconfig_FREE, syswindow_x11window(&native->oswindow[0])));
   TEST(0 == eglwin);

   // TEST init_eglwindow: uninitialized os window
   TEST(EINVAL == init_eglwindow(&eglwin, gl_display(&native->display), native->eglconfig[0], syswindow_x11window(&native->freeoswindow)));
   TEST(0 == eglwin);

   // TEST init_eglwindow: config does not match
   const int32_t pixmap_attr[] = { gconfig_TYPE, gconfig_value_TYPE_PIXMAP_BIT, gconfig_NONE };
   TEST(0 == init_eglconfig(&eglconf, gl_display(&native->display), pixmap_attr));
   TEST(EINVAL == init_eglwindow(&eglwin, gl_display(&native->display), eglconf, syswindow_x11window(&native->oswindow[0])));
   TEST(0 == eglwin);
   TEST(0 == free_eglconfig(&eglconf));

   for (unsigned i = 0; i < lengthof(native->oswindow); ++i) {
      // TEST init_eglwindow: different configurations
      TEST(0 == init_eglwindow(&eglwin, gl_display(&native->display), native->eglconfig[i], syswindow_x11window(&native->oswindow[i])));
      TEST(0 != eglwin);

      // TEST free_eglwindow
      TEST(0 == free_eglwindow(&eglwin, gl_display(&native->display)));
      TEST(0 == eglwin);
      TEST(0 == free_eglwindow(&eglwin, gl_display(&native->display)));
      TEST(0 == eglwin);
   }

   // TEST free_eglwindow: ERROR
   TEST(0 == init_eglwindow(&eglwin, gl_display(&native->display), native->eglconfig[0], syswindow_x11window(&native->oswindow[0])));
   TEST(0 != eglwin);
   init_testerrortimer(&s_eglwindow_errtimer, 1, ENOMEM);
   TEST(ENOMEM == free_eglwindow(&eglwin, gl_display(&native->display)));
   TEST(0 == eglwin);

   return 0;
ONERR:
   free_eglconfig(&eglconf);
   free_eglwindow(&eglwin, gl_display(&native->display));
   return EINVAL;
}

static int childprocess_unittest(void)
{
   resourceusage_t   usage   = resourceusage_FREE;
   native_types_t    native  = native_types_FREE;

   TEST(0 == init_native(&native));

   for (unsigned i = 0; i <= 2; ++i) {
      CLEARBUFFER_ERRLOG();

      TEST(0 == init_resourceusage(&usage));

      if (test_initfree(&native))   goto ONERR;

      if (0 == same_resourceusage(&usage)) break;
      TEST(0 == free_resourceusage(&usage));
   }

   TEST(0 == same_resourceusage(&usage));
   TEST(0 == free_resourceusage(&usage));

   size_t    logsize;
   uint8_t * logbuffer;
   GETBUFFER_ERRLOG(&logbuffer, &logsize);
   for (unsigned i = 0; i <= 2; ++i) {
      TEST(0 == init_resourceusage(&usage));

      if (test_draw(&native))       goto ONERR;

      if (0 == same_resourceusage(&usage)) break;
      TEST(0 == free_resourceusage(&usage));
      TRUNCATEBUFFER_ERRLOG(logsize);
   }

   TEST(0 == same_resourceusage(&usage));
   TEST(0 == free_resourceusage(&usage));

   TEST(0 == free_native(&native));

   return 0;
ONERR:
   (void) free_resourceusage(&usage);
   (void) free_native(&native);
   return EINVAL;
}

int unittest_platform_opengl_egl_eglwindow()
{
   int err;

   TEST(0 == execasprocess_unittest(&childprocess_unittest, &err));

   return err;
ONERR:
   return EINVAL;
}

#endif
