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

   file: C-kern/platform/X11/x11.c
    Implementation file <X11-Subsystem impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/platform/X11/x11.h"
#include "C-kern/api/platform/X11/x11display.h"
#include "C-kern/api/platform/X11/x11window.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/platform/task/thread.h"
#endif
#include STR(C-kern/api/platform/KONFIG_OS/graphic/sysx11.h)


// section: X11_t

// group: static variables

/* variable: s_X11_init
 * Remembers if initialization was done. It is used in
 * <initonce_X11> which is called before any other X11
 * function is called and which inits the X11-library to make it thread safe. */
static bool s_X11_init = false;

// group: init

int initonce_X11()
{
   int err;

   if (!s_X11_init) {
      if (! XInitThreads()) {
         err = ENOSYS;
         TRACESYSCALL_ERRLOG("XInitThreads", err);
         goto ONABORT;
      }
      s_X11_init = true;
   }

   return 0;
ONABORT:
   return err;
}

int freeonce_X11()
{
   s_X11_init = false;
   return 0;
}

// group: update

int dispatchevent_X11(x11display_t * x11disp)
{
   int err;
   x11window_t *  x11win;
   XEvent         xevent;

   while (XPending(x11disp->sys_display)) {

      if (XNextEvent(x11disp->sys_display, &xevent)) {
         err = EINVAL;
         goto ONABORT;
      }

      // process event
      switch (xevent.type) {
      case ClientMessage:
         #define event xevent.xclient
         // filter event
         if (  event.message_type       == x11disp->atoms.WM_PROTOCOLS
               && (Atom)event.data.l[0] == x11disp->atoms.WM_DELETE_WINDOW
               && 0 == tryfindobject_x11display(x11disp, &x11win, event.window)) {
            if (x11win->evhimpl && x11win->evhimpl->onclose) {
               x11win->evhimpl->onclose(x11win);
            }
         }
         #undef event
         break;

      case DestroyNotify:
         #define event xevent.xdestroywindow

         // filter event
         if (0 == tryfindobject_x11display(x11disp, &x11win, event.window)) {
            // <free_x11window> was not called before this message
            x11win->sys_drawable = 0;
            x11win->state        = x11window_state_DESTROYED;
            x11win->flags        = (uint8_t) (x11win->flags & ~x11window_flags_OWNWINDOW);
            (void) removeobject_x11display(x11disp, event.window);

            if (x11win->evhimpl && x11win->evhimpl->ondestroy) {
               x11win->evhimpl->ondestroy(x11win);
            }
         }
         #undef event
         break;

      case ConfigureNotify:
         #define event xevent.xconfigure

         // filter event
         if (0 == tryfindobject_x11display(x11disp, &x11win, event.window)) {
            if (x11win->evhimpl && x11win->evhimpl->onreshape) {
               x11win->evhimpl->onreshape(x11win, (uint32_t)event.width, (uint32_t)event.height);
            }
         }
         #undef event
         break;

      case Expose:
         #define event xevent.xexpose

         // filter event
         if (  0 == event.count/*last expose*/
               && 0 == tryfindobject_x11display(x11disp, &x11win, event.window)) {
            if (x11win->evhimpl && x11win->evhimpl->onredraw) {
               x11win->evhimpl->onredraw(x11win);
            }
         }
         #undef event
         break;

      case MapNotify:
         #define event xevent.xmap

         // filter event
         if (0 == tryfindobject_x11display(x11disp, &x11win, event.window)) {
            x11win->state = x11window_state_SHOWN;

            if (x11win->evhimpl && x11win->evhimpl->onvisible) {
               x11win->evhimpl->onvisible(x11win, true);
            }
         }
         #undef event
         break;

      case UnmapNotify:
         #define event xevent.xunmap

         // filter event
         if (0 == tryfindobject_x11display(x11disp, &x11win, event.window)) {
            x11win->state = x11window_state_HIDDEN;

            if (x11win->evhimpl && x11win->evhimpl->onvisible) {
               x11win->evhimpl->onvisible(x11win, false);
            }
         }
         #undef event
         break;

      default:
         // handle RRScreenChangeNotifyMask
         if (XRRUpdateConfiguration(&xevent)) {
            // handled by randr extension
            break;
         }

         //////////////////////////////////////////////////
         // add event handlers for other extensions here //
         //////////////////////////////////////////////////

         break;
      }
   }

   return 0;
ONABORT:
   TRACEABORT_ERRLOG(err);
   return err;
}

int nextevent_X11(struct x11display_t * x11disp)
{
   XEvent xevent;
   XPeekEvent(x11disp->sys_display, &xevent);
   return dispatchevent_X11(x11disp);
}


// section: Functions
// group: test

#ifdef KONFIG_UNITTEST

static int test_initonce(void)
{
   // prepare
   TEST(0 == freeonce_X11());
   TEST(!s_X11_init);

   // TEST initonce_X11
   TEST(0 == initonce_X11());
   TEST(s_X11_init);

   // TEST freeonce_X11
   TEST(0 == freeonce_X11());
   TEST(!s_X11_init);
   TEST(0 == freeonce_X11());
   TEST(!s_X11_init);

   // unprepare
   TEST(0 == initonce_X11());
   TEST(s_X11_init);

   return 0;
ONABORT:
   initonce_X11();
   return EINVAL;
}

int unittest_platform_X11()
{
   Display * disp = 0;

   // prepare
   disp = XOpenDisplay(0);
   TEST(disp);


   if (test_initonce())    goto ONABORT;

   // restore
   XCloseDisplay(disp);

   return 0;
ONABORT:
   if (disp) {
      XCloseDisplay(disp);
   }
   return EINVAL;
}

#endif
