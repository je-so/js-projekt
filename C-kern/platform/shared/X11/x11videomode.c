/* title: X11-Videomode impl

   Implements <X11-Videomode>.

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

   file: C-kern/api/platform/X11/x11videomode.h
    Header file <X11-Videomode>.

   file: C-kern/platform/shared/X11/x11videomode.c
    Implementation file <X11-Videomode impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/platform/X11/x11videomode.h"
#include "C-kern/api/err.h"
#include "C-kern/api/platform/thread.h"
#include "C-kern/api/platform/X11/x11.h"
#include "C-kern/api/platform/X11/x11display.h"
#include "C-kern/api/platform/X11/x11screen.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif
#include "C-kern/api/platform/X11/x11syskonfig.h"


// section: x11videomode_iterator_t

// group: lifetime

int init_x11videomodeiterator(/*out*/x11videomode_iterator_t * xvidit, x11screen_t * x11screen)
{
   int err ;
   XRRScreenConfiguration * screen_config = 0 ;

   if (!isextxrandr_x11display(display_x11screen(x11screen))) {
      err = ENOSYS ;
      goto ONABORT ;
   }

   Display * sys_display = display_x11screen(x11screen)->sys_display ;
   screen_config = XRRGetScreenInfo(sys_display, RootWindow(sys_display, number_x11screen(x11screen))) ;
   if (!screen_config) {
      err = ENOSYS ;
      goto ONABORT ;
   }

   int           sizes_count ;
   XRRScreenSize * sizes = XRRConfigSizes(screen_config, &sizes_count) ;
   if (  !sizes
         || (UINT16_MAX < (unsigned)sizes_count)) {
      err = EOVERFLOW ;
      goto ONABORT ;
   }

   xvidit->nextindex = 0 ;
   xvidit->nrmodes   = (uint16_t) sizes_count ;
   xvidit->config    = screen_config ;

   return 0 ;
ONABORT:
   if (screen_config) {
      XRRFreeScreenConfigInfo(screen_config) ;
   }
   TRACEABORT_LOG(err) ;
   return err ;
}

int free_x11videomodeiterator(x11videomode_iterator_t * xvidit)
{
   XRRScreenConfiguration * screen_config = xvidit->config ;

   xvidit->nextindex = 0 ;
   xvidit->nrmodes   = 0 ;
   xvidit->config    = 0 ;

   if (screen_config) {
      XRRFreeScreenConfigInfo(screen_config) ;
   }

   return 0 ;
}

void gofirst_x11videomodeiterator(x11videomode_iterator_t * xvidit)
{
   xvidit->nextindex = 0 ;
}

bool next_x11videomodeiterator(x11videomode_iterator_t * xvidit, /*out*/x11videomode_t * xvidmode)
{
   int count ;
   XRRScreenSize * sizes ;

   if (xvidit->nextindex >= xvidit->nrmodes) {
      return false ;
   }

   sizes = XRRConfigSizes((XRRScreenConfiguration *)xvidit->config, &count) ;

   initfromvalues_x11videomode(xvidmode,
                               (unsigned)sizes[xvidit->nextindex].width,
                               (unsigned)sizes[xvidit->nextindex].height, xvidit->nextindex) ;

   ++ xvidit->nextindex ;

   return true ;
}



// section: x11videomode_t

// group: lifetime

int initcurrent_x11videomode(/*out*/x11videomode_t * current_xvidmode, x11screen_t * x11screen)
{
   int err ;
   XRRScreenConfiguration * screen_config = 0 ;

   if (!isextxrandr_x11display(display_x11screen(x11screen))) {
      err = ENOSYS ;
      goto ONABORT ;
   }

   Display * sys_display = display_x11screen(x11screen)->sys_display ;
   screen_config = XRRGetScreenInfo(sys_display, RootWindow(sys_display, number_x11screen(x11screen))) ;
   if (!screen_config) {
      err = ENOSYS ;
      goto ONABORT ;
   }

   Rotation current_rotation ;
   uint16_t current_size = XRRConfigCurrentConfiguration(screen_config, &current_rotation) ;

   int           sizes_count ;
   XRRScreenSize * sizes = XRRConfigSizes(screen_config, &sizes_count) ;
   if (  !sizes
         || (current_size >= sizes_count)) {
      err = EOVERFLOW ;
      goto ONABORT ;
   }

   current_xvidmode->modeid          = current_size ;
   current_xvidmode->width_in_pixel  = (uint32_t) sizes[current_size].width ;
   current_xvidmode->height_in_pixel = (uint32_t) sizes[current_size].height ;

   XRRFreeScreenConfigInfo(screen_config) ;

   return 0 ;
ONABORT:
   if (screen_config) {
      XRRFreeScreenConfigInfo(screen_config) ;
   }
   TRACEABORT_LOG(err) ;
   return err ;
}

// change

int set_x11videomode(const x11videomode_t * xvidmode, x11screen_t * x11screen)
{
   int err ;
   XRRScreenConfiguration * screen_config = 0 ;

   if (!isextxrandr_x11display(display_x11screen(x11screen))) {
      err = ENOSYS ;
      goto ONABORT ;
   }

   Display * sys_display = display_x11screen(x11screen)->sys_display ;
   screen_config = XRRGetScreenInfo(sys_display, RootWindow(sys_display, number_x11screen(x11screen))) ;
   if (!screen_config) {
      err = ENOSYS ;
      goto ONABORT ;
   }

   Rotation current_rotation ;
   uint16_t current_size = XRRConfigCurrentConfiguration(screen_config, &current_rotation) ;

   int            sizes_count ;
   XRRScreenSize  * sizes = XRRConfigSizes(screen_config, &sizes_count) ;
   if (  !sizes
         || (current_size >= sizes_count)) {
      err = EOVERFLOW ;
      goto ONABORT ;
   }

   if (  xvidmode->modeid > sizes_count
         || xvidmode->width_in_pixel  != (uint32_t) sizes[xvidmode->modeid].width
         || xvidmode->height_in_pixel != (uint32_t) sizes[xvidmode->modeid].height) {
      err = EINVAL ;
      goto ONABORT ;
   }

   if (XRRSetScreenConfig( sys_display, screen_config,
                           RootWindow(sys_display, number_x11screen(x11screen)),
                           xvidmode->modeid, current_rotation, CurrentTime)) {
      err = EOPNOTSUPP ;
      goto ONABORT ;
   }

   XRRFreeScreenConfigInfo(screen_config) ;

   return 0 ;
ONABORT:
   if (screen_config) {
      XRRFreeScreenConfigInfo(screen_config) ;
   }
   TRACEABORT_LOG(err) ;
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_iterator(x11screen_t * x11screen)
{
   x11videomode_iterator_t xvidit = x11videomode_iterator_INIT_FREEABLE ;

   // TEST x11videomode_iterator_INIT_FREEABLE
   TEST(0 == xvidit.nextindex) ;
   TEST(0 == xvidit.nrmodes) ;
   TEST(0 == xvidit.config) ;

   // TEST init_x11videomodeiterator, free_x11videomodeiterator
   xvidit.nextindex = 1 ;
   TEST(0 == init_x11videomodeiterator(&xvidit, x11screen)) ;
   TEST(0 == xvidit.nextindex) ;
   TEST(0 != xvidit.nrmodes) ;
   TEST(0 != xvidit.config) ;
   TEST(0 == free_x11videomodeiterator(&xvidit)) ;
   TEST(0 == xvidit.nextindex) ;
   TEST(0 == xvidit.nrmodes) ;
   TEST(0 == xvidit.config) ;
   TEST(0 == free_x11videomodeiterator(&xvidit)) ;
   TEST(0 == xvidit.nextindex) ;
   TEST(0 == xvidit.nrmodes) ;
   TEST(0 == xvidit.config) ;

   // TEST next_x11videomodeiterator
   TEST(0 == init_x11videomodeiterator(&xvidit, x11screen)) ;
   x11videomode_t firstxvidmode ;
   for (size_t count = 0; !count; ) {
      x11videomode_t xvidmode = { 0, 0, 0 } ;
      while (next_x11videomodeiterator(&xvidit, &xvidmode)) {
         if (!count) firstxvidmode = xvidmode ;
         TEST(xvidmode.width_in_pixel > 0) ;
         TEST(xvidmode.height_in_pixel > 0) ;
         TEST(xvidmode.width_in_pixel < 5000) ;
         TEST(xvidmode.height_in_pixel < 5000) ;
         TEST(xvidmode.modeid == count) ;
         ++ count ;
      }
      TEST(count == xvidit.nextindex) ;
      TEST(count == xvidit.nrmodes) ;
   }

   // TEST gofirst_x11videomodeiterator, next_x11videomodeiterator
   gofirst_x11videomodeiterator(&xvidit) ;
   TEST(0 == xvidit.nextindex) ;
   for (size_t count = 0; !count; ) {
      x11videomode_t xvidmode = { 0, 0, 0 } ;
      while (next_x11videomodeiterator(&xvidit, &xvidmode)) {
         if (!count) {
            TEST(0 == memcmp(&xvidmode, &firstxvidmode, sizeof(xvidmode))) ;
         }
         ++ count ;
      }
      TEST(count == xvidit.nrmodes) ;
   }
   TEST(0 == free_x11videomodeiterator(&xvidit)) ;

   return 0 ;
ONABORT:
   (void) free_x11videomodeiterator(&xvidit) ;
   return EINVAL ;
}

static int test_initfree(x11screen_t * x11screen)
{
   x11videomode_t          xvidmode = x11videomode_INIT_FREEABLE ;
   x11videomode_iterator_t xvidit   = x11videomode_iterator_INIT_FREEABLE ;

   // TEST x11videomode_INIT_FREEABLE
   TEST(0 == xvidmode.width_in_pixel) ;
   TEST(0 == xvidmode.height_in_pixel) ;
   TEST(0 == xvidmode.modeid) ;

   // TEST initfromvalues_x11videomode
   initfromvalues_x11videomode(&xvidmode, 11, 12, 13) ;
   TEST(11 == xvidmode.width_in_pixel) ;
   TEST(12 == xvidmode.height_in_pixel) ;
   TEST(13 == xvidmode.modeid) ;

   // TEST initcurrent_x11videomode
   xvidmode.width_in_pixel = 0 ;
   xvidmode.height_in_pixel = 0 ;
   xvidmode.modeid = 1 ;
   TEST(0 == initcurrent_x11videomode(&xvidmode, x11screen)) ;
   TEST(0 != xvidmode.width_in_pixel) ;
   TEST(0 != xvidmode.height_in_pixel) ;
   TEST(0 == xvidmode.modeid/*default mode of x11display is active*/) ;
   TEST(0 == init_x11videomodeiterator(&xvidit, x11screen)) ;
   for (size_t count = 0; !count; count = 1) {
      x11videomode_t xvidmode2 = x11videomode_INIT_FREEABLE ;
      while (next_x11videomodeiterator(&xvidit, &xvidmode2)) {
         if (count == xvidmode.modeid) {
            break ;
         }
         ++ count ;
      }
      TEST(xvidmode.width_in_pixel  == xvidmode2.width_in_pixel) ;
      TEST(xvidmode.height_in_pixel == xvidmode2.height_in_pixel) ;
      TEST(xvidmode.modeid          == xvidmode2.modeid) ;
   }
   TEST(0 == free_x11videomodeiterator(&xvidit)) ;

   // TEST initcurrent_x11videomode: ENOSYS
   display_x11screen(x11screen)->xrandr.isSupported = false ;
   TEST(ENOSYS == initcurrent_x11videomode(&xvidmode, x11screen)) ;
   display_x11screen(x11screen)->xrandr.isSupported = true ;

   return 0 ;
ONABORT:
   (void) free_x11videomodeiterator(&xvidit) ;
   return EINVAL ;
}


/* function: waitXRRScreenChangeNotify
 * Wait for XRRScreenChangeNotify and checks that x11disp gets updated. */
static int waitXRRScreenChangeNotify(x11screen_t * x11screen, x11videomode_t * xvidmode)
{
   Display * sys_display = display_x11screen(x11screen)->sys_display ;

   XFlush(sys_display) ;

   for (;;) {
      if (!XPending(sys_display)) {
         sleep(1) ;
         TEST(XPending(sys_display)) ;
      }
      XEvent e ;
      XRRScreenChangeNotifyEvent e2 ;
      XPeekEvent(sys_display, &e) ;
      memcpy(&e2, &e, sizeof(e2)) ;

      if (  e.type == (RRScreenChangeNotify + display_x11screen(x11screen)->xrandr.eventbase)
            && e2.height == DisplayHeight(sys_display, number_x11screen(x11screen))
            && e2.width  == DisplayWidth(sys_display, number_x11screen(x11screen))) {
         // got previous configuration
         TEST(0 == dispatchevent_X11(display_x11screen(x11screen))) ;
         continue ;

      } else if (e.type == (RRScreenChangeNotify + display_x11screen(x11screen)->xrandr.eventbase)) {
         // display_x11screen(x11screen) contains old video configuration
         TEST( e2.height != DisplayHeight(sys_display, number_x11screen(x11screen))
               || e2.width != DisplayWidth(sys_display, number_x11screen(x11screen))) ;
         TEST(0 == dispatchevent_X11(display_x11screen(x11screen))) ;
         // event dispatcher has updated video configuration
         TEST(e2.height == DisplayHeight(sys_display, number_x11screen(x11screen))) ;
         TEST(e2.width  == DisplayWidth(sys_display, number_x11screen(x11screen))) ;
         TEST(xvidmode->height_in_pixel == (uint32_t)e2.height) ;
         TEST(xvidmode->width_in_pixel  == (uint32_t)e2.width) ;
         break ;

      } else {
         // consume event
         XNextEvent(sys_display, &e) ;
      }
   }

   TEST(0 == dispatchevent_X11(display_x11screen(x11screen))) ;
   sleepms_thread(100) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_setvideomode(x11screen_t * x11screen)
{
   x11videomode_iterator_t xvidit  = x11videomode_iterator_INIT_FREEABLE ;
   x11videomode_t          setmode = x11videomode_INIT_FREEABLE ;
   bool                    isWrongVideoMode = false ;
   x11videomode_t          current_xvidmode ;

   // prepare
   TEST(0 == initcurrent_x11videomode(&current_xvidmode, x11screen)) ;

   // TEST set_x11videomode
   TEST(0 == init_x11videomodeiterator(&xvidit, x11screen)) ;
   while (next_x11videomodeiterator(&xvidit, &setmode)) {
      if (  (  setmode.height_in_pixel   != current_xvidmode.height_in_pixel
               || setmode.width_in_pixel != current_xvidmode.width_in_pixel)
            && setmode.width_in_pixel  >= setmode.width_in_pixel
            && setmode.height_in_pixel >= setmode.height_in_pixel) {
         break ;
      }
   }
   TEST(0 == free_x11videomodeiterator(&xvidit)) ;

   TEST(0 == set_x11videomode(&setmode, x11screen)) ;
   isWrongVideoMode = true ;
   TEST(0 == waitXRRScreenChangeNotify(x11screen, &setmode)) ;

   // TEST set_x11videomode: reset video mode
   isWrongVideoMode = false ;
   TEST(0 == set_x11videomode(&current_xvidmode, x11screen)) ;
   TEST(0 == waitXRRScreenChangeNotify(x11screen, &current_xvidmode)) ;

   // TEST set_x11videomode: ENOSYS
   display_x11screen(x11screen)->xrandr.isSupported = false ;
   TEST(ENOSYS == set_x11videomode(&setmode, x11screen)) ;
   display_x11screen(x11screen)->xrandr.isSupported = true ;

   return 0 ;
ONABORT:
   if (isWrongVideoMode) {
      set_x11videomode(&current_xvidmode, x11screen) ;
   }
   (void) free_x11videomodeiterator(&xvidit) ;
   return EINVAL ;
}

int unittest_platform_X11_x11videomode()
{
   x11display_t      x11disp   = x11display_INIT_FREEABLE ;
   x11screen_t       x11screen = x11screen_INIT_FREEABLE ;
   resourceusage_t   usage     = resourceusage_INIT_FREEABLE ;

   // prepare
   TEST(0 == init_x11display(&x11disp, ":0")) ;
   x11screen = defaultscreen_x11display(&x11disp) ;

   if (test_iterator(&x11screen))      goto ONABORT ;
   if (test_initfree(&x11screen))      goto ONABORT ;
   if (test_setvideomode(&x11screen))  goto ONABORT ; // has memory leak

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_iterator(&x11screen))      goto ONABORT ;
   if (test_initfree(&x11screen))      goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   // unprepare
   TEST(0 == free_x11display(&x11disp)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   (void) free_x11display(&x11disp) ;
   return EINVAL ;
}

#endif