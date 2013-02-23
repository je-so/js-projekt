/* title: X11-Subsystem impl
   Implements window management for X11 graphics environment.

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
   (C) 2011 Jörg Seebohn

   file: C-kern/api/presentation/X11/x11.h
    Header file of <X11-Subsystem>.

   file: C-kern/presentation/X11/x11.c
    Implementation file <X11-Subsystem impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/presentation/X11/x11.h"
#include "C-kern/api/presentation/X11/x11display.h"
#include "C-kern/api/presentation/X11/glxwindow.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/platform/thread.h"
#endif
#include "C-kern/api/presentation/X11/x11syskonfig.h"


// section: X11_t

// group: static variables

/* variable: s_X11_init
 * Remembers if initialization was done. It is used in
 * <initonce_X11> which is called before any other X11
 * function is called and which inits the X11-library to make it thread safe. */
static bool             s_X11_init = false ;

/* variable: s_X11_callback
 * Remembers registered event handlers. */
static X11_callback_f   s_X11_callback[256] = { 0 } ;

// group: init

int initonce_X11()
{
   int err ;

   if (!s_X11_init) {
      if (! XInitThreads()) {
         err = ENOSYS ;
         TRACESYSERR_LOG("XInitThreads", err) ;
         goto ONABORT ;
      }
      s_X11_init = true ;
   }

   return 0 ;
ONABORT:
   return err ;
}

int freeonce_X11()
{
   s_X11_init = false ;
   memset(s_X11_callback, 0, sizeof(s_X11_callback)) ;
   return 0 ;
}

// group: callback

int setcallback_X11(uint8_t type, X11_callback_f eventcb)
{
   int err ;

   static_assert(256 == lengthof(s_X11_callback), "type is always in range") ;

   if (s_X11_callback[type]) {
      err = EBUSY ;
      goto ONABORT ;
   }

   s_X11_callback[type] = eventcb ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int clearcallback_X11(uint8_t type, X11_callback_f eventcb)
{
   int err ;

   static_assert(256 == lengthof(s_X11_callback), "type is always in range") ;

   if (s_X11_callback[type]) {

      if (s_X11_callback[type] != eventcb) {
         err = EPERM ;
         goto ONABORT ;
      }

      s_X11_callback[type] = 0 ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int dispatchevent_X11(x11display_t * x11disp)
{
   int err ;

   while (XPending(x11disp->sys_display)) {
      XEvent xevent ;

      if (XNextEvent(x11disp->sys_display, &xevent)) {
         err = EINVAL ;
         goto ONABORT ;
      }

      // process event
      const int type = xevent.type ;
      if (  (unsigned)type < lengthof(s_X11_callback)
            && s_X11_callback[type]) {
         s_X11_callback[type](x11disp, &xevent) ;
      }

      break ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

bool iscallback_X11(uint8_t type)
{
   static_assert(256 == lengthof(s_X11_callback), "type is always in range") ;

   return (0 != s_X11_callback[type]) ;
}

// group: test

#ifdef KONFIG_UNITTEST

static int test_initonce(void)
{
   // prepare
   TEST(0 == freeonce_X11()) ;
   TEST(!s_X11_init) ;

   // TEST initonce_X11
   TEST(0 == initonce_X11()) ;
   TEST(s_X11_init) ;

   // TEST freeonce_X11
   memset(s_X11_callback, 255, sizeof(s_X11_callback)) ;
   TEST(0 == freeonce_X11()) ;
   TEST(!s_X11_init) ;
   for (uint16_t i = 0; i < lengthof(s_X11_callback); ++i) {
      TEST(s_X11_callback[i] == 0) ;
   }
   TEST(0 == freeonce_X11()) ;
   TEST(!s_X11_init) ;
   for (uint16_t i = 0; i < lengthof(s_X11_callback); ++i) {
      TEST(s_X11_callback[i] == 0) ;
   }

   // unprepare
   TEST(0 == initonce_X11()) ;
   TEST(s_X11_init) ;

   return 0 ;
ONABORT:
   initonce_X11() ;
   return EINVAL ;
}

static int test_callback_set(void)
{
   // TEST setcallback_X11, iscallback_X11
   for (uint16_t i = 0; i < 256; ++i) {
      s_X11_callback[i] = 0 ;
      TEST(! iscallback_X11((uint8_t)i)) ;
      TEST(0 == setcallback_X11((uint8_t)i, (X11_callback_f) (i+1))) ;
      TEST(iscallback_X11((uint8_t)i)) ;
   }
   for (uint16_t i = 0; i < 256; ++i) {
      TEST(s_X11_callback[i] == (X11_callback_f) (i+1)) ;
   }

   // TEST clearcallback_X11, iscallback_X11
   for (uint16_t i = 0; i < 256; ++i) {
      TEST(s_X11_callback[i] == (X11_callback_f) (i+1)) ;
      TEST(iscallback_X11((uint8_t)i)) ;
      TEST(0 == clearcallback_X11((uint8_t)i, (X11_callback_f) (i+1))) ;
      TEST(s_X11_callback[i] == 0) ;
      TEST(!iscallback_X11((uint8_t)i)) ;
   }
   for (uint16_t i = 0; i < 256; ++i) {
      TEST(s_X11_callback[i] == 0) ;
   }

   // TEST setcallback_X11: EBUSY (set and already set handler)
   TEST(0 == setcallback_X11(10, (X11_callback_f) 10)) ;
   TEST(EBUSY == setcallback_X11(10, (X11_callback_f) 10)) ;
   TEST(0 == clearcallback_X11(10, (X11_callback_f) 10)) ;

   // TEST clearcallback_X11: EPERM (clear an handler with another address)
   TEST(0 == setcallback_X11(10, (X11_callback_f) 10)) ;
   TEST(EPERM == clearcallback_X11(10, (X11_callback_f) 11)) ;
   TEST(0 == clearcallback_X11(10, (X11_callback_f) 10)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}


static x11display_t * s_dummy_x11disp = 0 ;
static XAnyEvent    s_dummy_event     = { 0, 0, 0, 0, 0 } ;

static void dummy_handler(x11display_t * x11disp, void * xevent)
{
   s_dummy_x11disp = x11disp ;
   s_dummy_event   = *(XAnyEvent*)xevent ;
}

static int test_callback_dispatch(Display * disp)
{
   Window   win = 0 ;
   bool     isEvent[LASTEvent] = { false } ;

   // TEST dispatchevent_X11
   TEST(0 == setcallback_X11(CirculateNotify, &dummy_handler)) ;
   TEST(0 == setcallback_X11(ConfigureNotify, &dummy_handler)) ;
   TEST(0 == setcallback_X11(DestroyNotify, &dummy_handler)) ;
   TEST(0 == setcallback_X11(GravityNotify, &dummy_handler)) ;
   TEST(0 == setcallback_X11(MapNotify, &dummy_handler)) ;
   TEST(0 == setcallback_X11(ReparentNotify, &dummy_handler)) ;
   TEST(0 == setcallback_X11(UnmapNotify, &dummy_handler)) ;

   win = XCreateSimpleWindow(disp, DefaultRootWindow(disp), 0, 0, 100, 100, BlackPixel(disp, DefaultScreen(disp)), 0, WhitePixel(disp, DefaultScreen(disp))) ;
   XSelectInput(disp, win, StructureNotifyMask) ;
   TEST(0 != XMapWindow(disp, win)) ;
   TEST(0 != XFlush(disp)) ;

   for (float seconds = 0; seconds < 3 && !isEvent[MapNotify];) {
      if (XPending(disp)) {
         MEMSET0(&s_dummy_event) ;
         x11display_t x11disp = { .sys_display = disp } ;
         s_dummy_event.type = 0 ;
         XEvent ev ;
         MEMSET0(&ev) ;
         XPeekEvent(x11disp.sys_display, &ev) ;
         TEST(0 == insertobject_x11display(&x11disp, (void*)win, win)) ;
         TEST(0 == dispatchevent_X11(&x11disp)) ;
         TEST(0 == removeobject_x11display(&x11disp, win)) ;
         TEST(s_dummy_x11disp    == &x11disp) ;
         TEST(s_dummy_event.type == ev.xany.type) ;
         TEST(s_dummy_event.type == ConfigureNotify
              || s_dummy_event.type == ReparentNotify
              || s_dummy_event.type == MapNotify) ;
         TEST(s_dummy_event.display == disp) ;
         TEST(s_dummy_event.window  == win) ;
         isEvent[s_dummy_event.type] = true ;
      } else {
         sleepms_thread(50) ;
         seconds += 0.05f ;
      }
   }
   TEST(0 != XDestroyWindow(disp, win)) ;
   TEST(0 != XFlush(disp)) ;
   for (float seconds = 0; seconds < 3 && !isEvent[DestroyNotify];) {
      if (XPending(disp)) {
         MEMSET0(&s_dummy_event) ;
         x11display_t x11disp = { .sys_display = disp } ;
         TEST(0 == insertobject_x11display(&x11disp, (void*)win, win)) ;
         s_dummy_event.type = 0 ;
         XEvent ev ;
         MEMSET0(&ev) ;
         XPeekEvent(x11disp.sys_display, &ev) ;
         TEST(0 == dispatchevent_X11(&x11disp)) ;
         TEST(0 == removeobject_x11display(&x11disp, win)) ;
         TEST(s_dummy_x11disp    == &x11disp) ;
         TEST(s_dummy_event.type == ev.xany.type) ;
         TEST(s_dummy_event.type == ConfigureNotify
              || s_dummy_event.type == DestroyNotify
              || s_dummy_event.type == UnmapNotify) ;
         TEST(s_dummy_event.display == disp) ;
         TEST(s_dummy_event.window  == win) ;
         isEvent[s_dummy_event.type] = true ;
      } else {
         sleepms_thread(50) ;
         seconds += 0.05f ;
      }
   }

   // TEST at least 4 events dispatched
   TEST(isEvent[ConfigureNotify]) ;
   TEST(isEvent[MapNotify]) ;
   TEST(isEvent[UnmapNotify]) ;
   TEST(isEvent[DestroyNotify]) ;
   isEvent[CirculateNotify] = false ;
   isEvent[ConfigureNotify] = false ;
   isEvent[GravityNotify]  = false ;
   isEvent[MapNotify]      = false ;
   isEvent[ReparentNotify] = false ;
   isEvent[UnmapNotify]    = false ;
   isEvent[DestroyNotify]  = false ;
   for (unsigned i = 0; i < lengthof(isEvent); ++i) {
      TEST(!isEvent[i]) ;
   }

   // unprepare
   TEST(0 == clearcallback_X11(CirculateNotify, &dummy_handler)) ;
   TEST(0 == clearcallback_X11(ConfigureNotify, &dummy_handler)) ;
   TEST(0 == clearcallback_X11(DestroyNotify, &dummy_handler)) ;
   TEST(0 == clearcallback_X11(GravityNotify, &dummy_handler)) ;
   TEST(0 == clearcallback_X11(MapNotify, &dummy_handler)) ;
   TEST(0 == clearcallback_X11(ReparentNotify, &dummy_handler)) ;
   TEST(0 == clearcallback_X11(UnmapNotify, &dummy_handler)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

int unittest_presentation_X11()
{
   Display           * disp = 0 ;
   resourceusage_t   usage  = resourceusage_INIT_FREEABLE ;
   X11_callback_f    old_callbacks[lengthof(s_X11_callback)] ;

   // prepare
   memcpy(old_callbacks, s_X11_callback, sizeof(s_X11_callback)) ;
   memset(s_X11_callback, 0, sizeof(s_X11_callback)) ;
   disp = XOpenDisplay(0) ;
   TEST(disp) ;

   if (test_callback_dispatch(disp))   goto ONABORT ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initonce())                goto ONABORT ;
   if (test_callback_set())            goto ONABORT ;
   if (test_callback_dispatch(disp))   goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   // restore
   XCloseDisplay(disp) ;
   memcpy(s_X11_callback, old_callbacks, sizeof(s_X11_callback)) ;

   return 0 ;
ONABORT:
   if (disp) {
      XCloseDisplay(disp) ;
   }
   memcpy(s_X11_callback, old_callbacks, sizeof(s_X11_callback)) ;
   return EINVAL ;
}

#endif
