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

   file: C-kern/api/platform/X11/x11.h
    Header file of <X11-Subsystem>.

   file: C-kern/platform/shared/X11/x11.c
    Implementation file <X11-Subsystem impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/platform/X11/x11.h"
#include "C-kern/api/platform/X11/x11display.h"
#include "C-kern/api/platform/X11/x11window.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/platform/task/thread.h"
#endif
#include "C-kern/api/platform/X11/x11syskonfig.h"


// section: X11_t

// group: static variables

/* variable: s_X11_init
 * Remembers if initialization was done. It is used in
 * <initonce_X11> which is called before any other X11
 * function is called and which inits the X11-library to make it thread safe. */
static bool    s_X11_init = false ;

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
   return 0 ;
}

// group: update

int dispatchevent_X11(x11display_t * x11disp)
{
   int err ;
   x11window_t *  x11win ;
   XEvent         xevent ;

   while (XPending(x11disp->sys_display)) {

      if (XNextEvent(x11disp->sys_display, &xevent)) {
         err = EINVAL ;
         goto ONABORT ;
      }

      // process event
      switch (xevent.type) {
      case ClientMessage:
         #define event  xevent.xclient
         // filter event
         if (  event.message_type       == x11disp->atoms.WM_PROTOCOLS
               && (Atom)event.data.l[0] == x11disp->atoms.WM_DELETE_WINDOW
               && 0 == tryfindobject_x11display(x11disp, (void**)&x11win, event.window)) {
            if (x11win->iimpl && x11win->iimpl->closerequest) {
               x11win->iimpl->closerequest(x11win) ;
            }
         }
         #undef event
         break ;

      case DestroyNotify:
         #define event  xevent.xdestroywindow

         // filter event
         if (0 == tryfindobject_x11display(x11disp, (void**)&x11win, event.window)) {
            // <free_x11window> was not called before this message
            x11win->sys_window = 0 ;
            x11win->state      = x11window_Destroyed ;
            x11win->flags      = (uint8_t) (x11win->flags & ~x11window_OwnWindow) ;
            (void) removeobject_x11display(x11disp, event.window) ;

            if (x11win->iimpl && x11win->iimpl->destroy) {
               x11win->iimpl->destroy(x11win) ;
            }
         }
         #undef event
         break ;

      case ConfigureNotify:
         #define event  xevent.xconfigure

         // filter event
         if (0 == tryfindobject_x11display(x11disp, (void**)&x11win, event.window)) {
            if (x11win->iimpl) {
               if (event.above != 0 && x11win->iimpl->repos) {
                  if (event.override_redirect && !event.send_event) {
                     x11win->iimpl->resize(x11win, (uint32_t)event.width, (uint32_t)event.height) ;
                  }
                  x11win->iimpl->repos(x11win, event.x, event.y, (uint32_t)event.width, (uint32_t)event.height) ;
               } else if (event.above == 0 && x11win->iimpl->resize) {
                  x11win->iimpl->resize(x11win, (uint32_t)event.width, (uint32_t)event.height) ;
               }
            }
         }
         #undef event
         break ;

      case Expose:
         #define event  xevent.xexpose

         // filter event
         if (  0 == event.count/*last expose*/
               && 0 == tryfindobject_x11display(x11disp, (void**)&x11win, event.window)) {
            if (x11win->iimpl && x11win->iimpl->redraw) {
               x11win->iimpl->redraw(x11win) ;
            }
         }
         #undef event
         break ;

      case MapNotify:
         #define event  xevent.xmap

         // filter event
         if (0 == tryfindobject_x11display(x11disp, (void**)&x11win, event.window)) {
            x11win->state = x11window_Shown ;

            if (x11win->iimpl && x11win->iimpl->showhide) {
               x11win->iimpl->showhide(x11win) ;
            }
         }
         #undef event
         break ;

      case UnmapNotify:
         #define event  xevent.xunmap

         // filter event
         if (0 == tryfindobject_x11display(x11disp, (void**)&x11win, event.window)) {
            x11win->state = x11window_Hidden ;

            if (x11win->iimpl && x11win->iimpl->showhide) {
               x11win->iimpl->showhide(x11win) ;
            }
         }
         #undef event
         break ;

      default:
         // handle RRScreenChangeNotifyMask
         if (XRRUpdateConfiguration(&xevent)) {
            // handled by randr extension
            break ;
         }

         //////////////////////////////////////////////////
         // add event handlers for other extensions here //
         //////////////////////////////////////////////////

         break ;
      }
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
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
   TEST(0 == freeonce_X11()) ;
   TEST(!s_X11_init) ;
   TEST(0 == freeonce_X11()) ;
   TEST(!s_X11_init) ;

   // unprepare
   TEST(0 == initonce_X11()) ;
   TEST(s_X11_init) ;

   return 0 ;
ONABORT:
   initonce_X11() ;
   return EINVAL ;
}

int unittest_platform_X11()
{
   Display           * disp = 0 ;
   resourceusage_t   usage  = resourceusage_INIT_FREEABLE ;

   // prepare
   disp = XOpenDisplay(0) ;
   TEST(disp) ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initonce())    goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   // restore
   XCloseDisplay(disp) ;

   return 0 ;
ONABORT:
   if (disp) {
      XCloseDisplay(disp) ;
   }
   return EINVAL ;
}

#endif
