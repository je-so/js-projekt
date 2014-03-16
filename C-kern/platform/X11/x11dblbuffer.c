/* title: X11-DoubleBuffer impl

   Implements <X11-DoubleBuffer>.

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
   (C) 2013 Jörg Seebohn

   file: C-kern/api/platform/X11/x11dblbuffer.h
    Header file <X11-DoubleBuffer>.

   file: C-kern/platform/X11/x11dblbuffer.c
    Implementation file <X11-DoubleBuffer impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/platform/X11/x11dblbuffer.h"
#include "C-kern/api/err.h"
#include "C-kern/api/platform/X11/x11display.h"
#include "C-kern/api/platform/X11/x11drawable.h"
#include "C-kern/api/platform/X11/x11window.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/test/resourceusage.h"
#include "C-kern/api/platform/task/thread.h"
#include "C-kern/api/platform/X11/x11.h"
#include "C-kern/api/platform/X11/x11attribute.h"
#include "C-kern/api/platform/X11/x11screen.h"
#endif
#include STR(C-kern/api/platform/KONFIG_OS/graphic/sysx11.h)


// section: x11dblbuffer_t

// group: helper

static inline void compiletime_assert(void)
{
   static_assert( ((x11drawable_t*)0) == genericcast_x11drawable((x11dblbuffer_t*)0),
                  "x11dblbuffer_t can be cast to x11drawable_t");
   static_assert(sizeof(((x11drawable_t*)0)->sys_drawable) == sizeof(XdbeBackBuffer),
                 "external visible handle has same size as internal X11 Window handle");
}

// group: lifetime

int init_x11dblbuffer(/*out*/x11dblbuffer_t * dblbuf, struct x11window_t * x11win)
{
   int err;
   XdbeBackBuffer backbuffer;

   if (  !x11win->display ||
         XdbeBadBuffer == (backbuffer = XdbeAllocateBackBufferName(x11win->display->sys_display, x11win->sys_drawable, XdbeUndefined))) {
      err = EINVAL;
      goto ONABORT;
   }

   *dblbuf = (x11dblbuffer_t) x11drawable_INIT(x11win->display, backbuffer, x11win->sys_colormap);

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

int free_x11dblbuffer(x11dblbuffer_t * dblbuf)
{
   int err;
   x11display_t * x11disp = dblbuf->display;

   if (x11disp) {
      int isErr = !XdbeDeallocateBackBufferName(x11disp->sys_display, dblbuf->sys_drawable);

      *dblbuf = (x11dblbuffer_t) x11dblbuffer_INIT_FREEABLE;

      if (isErr) {
         err = EINVAL ;
         goto ONABORT;
      }
   }

   return 0 ;
ONABORT:
   TRACEABORTFREE_ERRLOG(err) ;
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

#define WAITFOR(x11disp, loop_count, CONDITION)                   \
   XFlush(x11disp->sys_display) ;                                 \
   for (int _count = 0; _count < loop_count; ++_count) {          \
      while (XPending(x11disp->sys_display)) {                    \
         dispatchevent_X11(x11disp) ;                             \
      }                                                           \
      if (CONDITION) break ;                                      \
      sleepms_thread(20) ;                                        \
   }

static int test_initfree(x11window_t * x11win)
{
   x11dblbuffer_t dblbuf   = x11dblbuffer_INIT_FREEABLE;

   // TEST x11dblbuffer_INIT_FREEABLE
   TEST(0 == dblbuf.display);
   TEST(0 == dblbuf.sys_drawable);
   TEST(0 == dblbuf.sys_colormap);

   // TEST init_x11dblbuffer
   TEST(0 == init_x11dblbuffer(&dblbuf, x11win));
   TEST(dblbuf.display      == x11win->display);
   TEST(dblbuf.sys_drawable != 0);
   TEST(dblbuf.sys_drawable != x11win->sys_drawable);
   TEST(dblbuf.sys_colormap == x11win->sys_colormap);

   // TEST free_x11dblbuffer
   TEST(0 == free_x11dblbuffer(&dblbuf));
   TEST(0 == dblbuf.display);
   TEST(0 == dblbuf.sys_drawable);
   TEST(0 == dblbuf.sys_colormap);
   TEST(0 == free_x11dblbuffer(&dblbuf));
   TEST(0 == dblbuf.display);
   TEST(0 == dblbuf.sys_drawable);
   TEST(0 == dblbuf.sys_colormap);

   return 0 ;
ONABORT:
   free_x11dblbuffer(&dblbuf);
   return EINVAL ;
}

static int compare_color(x11window_t * x11win, uint32_t w, uint32_t h, bool isRed, bool isGreen, bool isBlue)
{
   XImage * ximg;
   size_t   pixels = 0;

   ximg = XGetImage(x11win->display->sys_display, x11win->sys_drawable, 0, 0, w, h, (unsigned long)-1, ZPixmap) ;

   for (uint32_t y = 0; y < h; ++y) {
      for (uint32_t x = 0; x < w; ++x) {
         unsigned long rgbcolor = XGetPixel(ximg, (int)x, (int)y) ;
         if (isRed == (0 != (rgbcolor & ximg->red_mask))
             && isGreen == (0 != (rgbcolor & ximg->green_mask))
             && isBlue  == (0 != (rgbcolor & ximg->blue_mask))) {
            ++ pixels ;
         }
      }
   }

   XDestroyImage(ximg) ;

   return (pixels == (size_t)w * h) ? 0 : EINVAL ;
}

static int test_draw(x11window_t * x11win)
{
   XColor         colblue  = { .red = 0, .green = 0, .blue = USHRT_MAX, .flags = DoRed|DoGreen|DoBlue } ;
   XColor         colgreen = { .red = 0, .green = USHRT_MAX, .blue = 0, .flags = DoRed|DoGreen|DoBlue } ;
   x11dblbuffer_t dblbuf   = x11dblbuffer_INIT_FREEABLE;
   GC             gc = 0;

   // prepare
   TEST(0 == init_x11dblbuffer(&dblbuf, x11win));
   XAllocColor(x11win->display->sys_display, x11win->sys_colormap, &colblue) ;
   XAllocColor(x11win->display->sys_display, x11win->sys_colormap, &colgreen) ;
   XGCValues gcvalues = { .foreground = colgreen.pixel  } ;
   gc = XCreateGC(x11win->display->sys_display, x11win->sys_drawable, GCForeground, &gcvalues) ;
   TEST(gc) ;
   TEST(0 == setpos_x11window(x11win, 100, 100)) ;
   TEST(0 == show_x11window(x11win)) ;
   WAITFOR(x11win->display, 10, x11win->state == x11window_state_SHOWN) ;
   TEST(x11win->state == x11window_state_SHOWN) ;

   // TEST window foreground green
   TEST(1 == XFillRectangle(x11win->display->sys_display, x11win->sys_drawable, gc, 0, 0, 200, 100)) ;
   WAITFOR(x11win->display, 1, false) ;
   TEST(0 == compare_color(x11win, 200, 100, 0, 1, 0)) ;

   // TEST window background blue / foreground green
   gcvalues.foreground = colblue.pixel ;
   TEST(1 == XChangeGC(x11win->display->sys_display, gc, GCForeground, &gcvalues)) ;
   TEST(1 == XFillRectangle(x11win->display->sys_display, dblbuf.sys_drawable, gc, 0, 0, 200, 100)) ;
   WAITFOR(x11win->display, 1, false) ;
   TEST(0 == compare_color(x11win, 200, 100, 0, 1, 0)) ;

   // TEST window background blue / foreground blue
   TEST(0 == swapbuffer_x11window(x11win)) ;
   WAITFOR(x11win->display, 1, false) ;
   TEST(0 == compare_color(x11win, 200, 100, 0, 0, 1)) ;

   // unprepare
   TEST(0 == free_x11dblbuffer(&dblbuf));
   XFreeGC(x11win->display->sys_display, gc) ;
   WAITFOR(x11win->display, 1, false) ;

   return 0 ;
ONABORT:
   if (gc) XFreeGC(x11win->display->sys_display, gc) ;
   free_x11dblbuffer(&dblbuf);
   return EINVAL ;
}


static int childprocess_unittest(void)
{
   x11display_t      x11disp   = x11display_INIT_FREEABLE ;
   x11window_t       x11win    = x11window_INIT_FREEABLE ;
   x11window_t       x11win2   = x11window_INIT_FREEABLE ;
   x11screen_t       x11screen = x11screen_INIT_FREEABLE ;
   x11attribute_t    config[]  = {  x11attribute_INIT_WINFRAME,
                                    x11attribute_INIT_WINTITLE("Double Buffer"),
                                    x11attribute_INIT_DOUBLEBUFFER,
                                    x11attribute_INIT_WINSIZE(200, 100)
                                 } ;
   x11attribute_t    config2[] = {  x11attribute_INIT_DOUBLEBUFFER,
                                    x11attribute_INIT_WINSIZE(200, 100)
                                 } ;
   resourceusage_t   usage     = resourceusage_INIT_FREEABLE ;

   // prepare
   TEST(0 == init_x11display(&x11disp, 0)) ;
   x11screen = defaultscreen_x11display(&x11disp) ;
   TEST(0 == init_x11window(&x11win, &x11screen, 0, lengthof(config), config)) ;
   TEST(0 == init_x11window(&x11win2, &x11screen, 0, lengthof(config2), config2)) ;

   if (test_initfree(&x11win))   goto ONABORT ;
   if (test_draw(&x11win))       goto ONABORT ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree(&x11win))   goto ONABORT ;
   if (test_draw(&x11win))       goto ONABORT ;
   if (test_draw(&x11win2))      goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   // unprepare
   TEST(0 == free_x11window(&x11win)) ;
   TEST(0 == free_x11window(&x11win2)) ;
   TEST(0 == free_x11display(&x11disp)) ;

   return 0 ;
ONABORT:
   (void) free_x11window(&x11win) ;
   (void) free_x11window(&x11win2) ;
   (void) free_x11display(&x11disp) ;
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

int unittest_platform_X11_x11dblbuffer()
{
   int err;

   TEST(0 == execasprocess_unittest(&childprocess_unittest, &err));

   return err;
ONABORT:
   return EINVAL ;
}

#endif
