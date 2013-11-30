/* title: X11-Window impl

   Implements <X11-Window>.

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

   file: C-kern/api/platform/X11/x11window.h
    Header file <X11-Window>.

   file: C-kern/platform/shared/X11/x11window.c
    Implementation file <X11-Window impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/platform/X11/x11window.h"
#include "C-kern/api/err.h"
#include "C-kern/api/platform/X11/x11.h"
#include "C-kern/api/platform/X11/x11attribute.h"
#include "C-kern/api/platform/X11/x11display.h"
#include "C-kern/api/platform/X11/x11drawable.h"
#include "C-kern/api/platform/X11/x11screen.h"
#include "C-kern/api/string/cstring.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/platform/task/thread.h"
#endif
#include "C-kern/api/platform/X11/x11syskonfig.h"


// section: x11window_t

// group: helper

static inline void compiletime_assert(void)
{
   static_assert(sizeof(((x11window_t*)0)->sys_window) == sizeof(Window),
                 "external visible handle has same size as internal X11 Window handle") ;
   static_assert(sizeof(((x11window_t*)0)->sys_colormap) == sizeof(Colormap),
                 "external visible handle has same size as internal X11 Window handle") ;
   static_assert(sizeof(((x11window_t*)0)->sys_backbuffer) == sizeof(XdbeBackBuffer),
                 "external visible handle has same size as internal X11 Window handle") ;
   static_assert(sizeof(x11drawable_t) == sizeof(((x11window_t*)0)->display) + sizeof(((x11window_t*)0)->sys_window) + sizeof(((x11window_t*)0)->sys_colormap)
                 && sizeof(((x11drawable_t*)0)->display) == sizeof(((x11window_t*)0)->display)
                 && offsetof(x11drawable_t, display) == offsetof(x11window_t, display)
                 && sizeof(((x11drawable_t*)0)->sys_drawable) == sizeof(((x11window_t*)0)->sys_window)
                 && offsetof(x11drawable_t, sys_drawable) == offsetof(x11window_t, sys_window)
                 && sizeof(((x11drawable_t*)0)->sys_colormap) == sizeof(((x11window_t*)0)->sys_colormap)
                 && offsetof(x11drawable_t, sys_colormap) == offsetof(x11window_t, sys_colormap),
                 "x11window_t can be cast to x11drawable_t") ;
}

static void setwinopacity_x11window(x11display_t * x11disp, Window win, uint32_t opacity)
{
   if (opacity == UINT32_MAX) {
      XDeleteProperty(x11disp->sys_display, win, x11disp->atoms._NET_WM_WINDOW_OPACITY) ;
   } else {
      XChangeProperty(x11disp->sys_display, win, x11disp->atoms._NET_WM_WINDOW_OPACITY,
                      XA_CARDINAL, 32, PropModeReplace, (uint8_t*) &opacity, 1) ;
   }
}

static int allocatebackbuffer_x11window(x11window_t * x11win)
{
   if (! (x11win->flags & x11window_OwnBackBuffer)) {
      XdbeBackBuffer backbuffer = XdbeAllocateBackBufferName(x11win->display->sys_display, x11win->sys_window, XdbeUndefined) ;
      if (backbuffer == XdbeBadBuffer) return EINVAL ;

      x11win->flags          |= x11window_OwnBackBuffer ;
      x11win->sys_backbuffer  = backbuffer ;
   }

   return 0 ;
}

static int matchvisual_x11window(
      x11screen_t *                 x11screen,
      /*out*/Visual **              x11_visual,
      /*out*/int *                  x11_depth,
      /*out*/bool *                 isBackBuffer,
      uint8_t                       nrofattributes,
      const struct x11attribute_t * configuration/*[nrofattributes]*/)
{
   uint32_t rgbbits   = 0 ;
   uint32_t alphabits = 0 ;
   bool     isDouble  = false ;
   bool     isOpacity = false ;

   for (uint16_t i = 0; i < nrofattributes; ++i) {
      switch (configuration[i].name) {
      case x11attribute_DOUBLEBUFFER:
         isDouble = configuration[i].value.isOn ;
         break ;
      case x11attribute_REDBITS:
      case x11attribute_GREENBITS:
      case x11attribute_BLUEBITS:
         if (configuration[i].value.u32 > rgbbits) {
            rgbbits = configuration[i].value.u32  ;
            if (rgbbits >= 32) goto ONABORT ;
         }
         break ;
      case x11attribute_ALPHABITS:
         alphabits = configuration[i].value.u32 ;
         if (alphabits >= 32) goto ONABORT ;
         break ;
      case x11attribute_ALPHAOPACITY:
         isOpacity = configuration[i].value.isOn ;
         break ;
      }
   }

   if (isOpacity && alphabits == 0) {
      alphabits = 1 ;
   }

   XVisualInfo    vinfo_pattern = { .class = TrueColor, .screen = x11screen->nrscreen } ;
   int            vinfo_length  = 0 ;
   XVisualInfo *  vinfo         = XGetVisualInfo(x11screen->display->sys_display, VisualClassMask|VisualScreenMask, &vinfo_pattern, &vinfo_length) ;
   XdbeScreenVisualInfo * vinfodb = 0 ;

   if (isDouble) {
      int      nrscreen = 1 ;
      Drawable screens  = RootWindow(x11screen->display->sys_display, x11screen->nrscreen) ;
      vinfodb = XdbeGetVisualInfo(x11screen->display->sys_display, &screens, &nrscreen) ;
   }

   bool isMatch = false ;

   if (  vinfo
         && (!isDouble  || vinfodb)
         && (!alphabits || x11screen->display->xrender.isSupported)) {
      for (int i = 0; i < vinfo_length; ++i) {
         if (  (int32_t)rgbbits <= vinfo[i].bits_per_rgb
               && 3 * vinfo[i].bits_per_rgb + (int32_t)alphabits <= vinfo[i].depth) {
            if (alphabits) {  // check alpha
               XRenderPictFormat * format = XRenderFindVisualFormat(x11screen->display->sys_display, vinfo[i].visual) ;
               if (!format || format->direct.alphaMask < (((int32_t)1 << alphabits)-1)) continue ;
            }
            if (isDouble) {   // check double buffer
               int dbi ;
               for (dbi = 0; dbi < vinfodb[0].count; ++dbi) {
                  if (vinfodb[0].visinfo[dbi].visual == vinfo[i].visualid) {
                     break ;
                  }
               }
               if (dbi >= vinfodb[0].count) continue ;
            }
            isMatch = true ;
            *x11_visual   = vinfo[i].visual ;
            *x11_depth    = vinfo[i].depth ;
            *isBackBuffer = isDouble ;
            break;
         }
      }
   }

   if (vinfodb)   XdbeFreeVisualInfo(vinfodb) ;
   if (vinfo)     XFree(vinfo) ;
   if (! isMatch) goto ONABORT ;

   return 0 ;
ONABORT:
   return ESRCH ;
}

// group: lifetime

/* function: initbasetype_x11window
 * In case visual == CopyFromParent the function expects depth == CopyFromParent also. */
int initbasetype_x11window(
      /*out*/x11window_t *          x11win,
      const struct x11window_it *   eventhandler,
      x11display_t *                x11disp,
      uint32_t                      parent_sys_window,
      /*Visual*/void *              visual,
      int                           depth,
      uint8_t                       nrofattributes,
      const struct x11attribute_t * configuration/*[nrofattributes]*/)
{
   int err ;
   Window      win ;
   Colormap    colormap ;
   uint8_t     flags      = 0 ;
   uint8_t     isFrame    = false ;
   uint32_t    opacity    = UINT32_MAX ;
   const char* title      = 0 ;
   XWMHints    wm_hints   = { .flags = StateHint, .initial_state = NormalState /*or WithdrawnState or IconicState*/ } ;
   XSizeHints  size_hints = { .flags = PBaseSize|PWinGravity,
                              .x = 0, .y = 0,
                              .base_width  = 100, .base_height = 100,
                              .win_gravity = NorthWestGravity
                           } ;
   XColor      colwhite   = { .red = (unsigned short)-1, .green = (unsigned short)-1, .blue = (unsigned short)-1, .flags = DoRed|DoGreen|DoBlue } ;


   static_assert(sizeof(parent_sys_window) == sizeof(Window), "same type") ;

   for (uint16_t i = 0; i < nrofattributes; ++i) {
      switch (configuration[i].name) {
      case x11attribute_WINTITLE:
         title = configuration[i].value.str ;
         break ;
      case x11attribute_WINFRAME:
         isFrame = configuration[i].value.isOn ;
         break ;
      case x11attribute_WINPOS:
         size_hints.flags |= PPosition ;
         size_hints.x = configuration[i].value.x ;
         size_hints.y = configuration[i].value.y ;
         break ;
      case x11attribute_WINSIZE:
         size_hints.base_width  = configuration[i].value.width ;
         size_hints.base_height = configuration[i].value.height ;
         break ;
      case x11attribute_WINMINSIZE:
         size_hints.flags |= PMinSize ;
         size_hints.min_width  = configuration[i].value.width ;
         size_hints.min_height = configuration[i].value.height ;
         break ;
      case x11attribute_WINMAXSIZE:
         size_hints.flags |= PMaxSize ;
         size_hints.max_width  = configuration[i].value.width ;
         size_hints.max_height = configuration[i].value.height ;
         break ;
      case x11attribute_WINOPACITY:
         opacity = configuration[i].value.u32 ;
         break ;
      }
   }

   if (visual == CopyFromParent) {
      XWindowAttributes winattr ;
      const bool isOK = XGetWindowAttributes(x11disp->sys_display, parent_sys_window, &winattr) ;
      if (!isOK) {
         err = EINVAL ;
         goto ONABORT ;
      }
      colormap = winattr.colormap ;
   } else {
      flags    = x11window_OwnColormap ;
      colormap = XCreateColormap(x11disp->sys_display, parent_sys_window, visual, AllocNone) ;
      XAllocColor(x11disp->sys_display, colormap, &colwhite) ;
   }

   {
      XSetWindowAttributes attr ;
      attr.background_pixmap = None ;
      attr.event_mask        = ExposureMask | KeyPressMask | StructureNotifyMask ;
      attr.override_redirect = ! isFrame ;
      attr.colormap          = colormap ;
      attr.border_pixel      = colwhite.pixel ; // this ensures that non standard visuals do not generate protocol error

      win = XCreateWindow( x11disp->sys_display, parent_sys_window, size_hints.x, size_hints.y,
                           (uint32_t)size_hints.base_width, (uint32_t)size_hints.base_height, 0, depth, InputOutput, visual,
                           CWBackPixmap | CWEventMask | CWOverrideRedirect | (flags ? (CWColormap|CWBorderPixel): 0),
                           &attr ) ;
      flags |= x11window_OwnWindow ;
   }

   // set opacity (if less than 100%)
   setwinopacity_x11window(x11disp, win, opacity) ;
   // set size and title
   Xutf8SetWMProperties(x11disp->sys_display, win, title ? title : "", 0, 0, 0, &size_hints, &wm_hints, 0) ;
   // closing a window only sends a request no actual destroying takes place
   Atom wm_delete_window = x11disp->atoms.WM_DELETE_WINDOW ;
   XSetWMProtocols(x11disp->sys_display, win, &wm_delete_window, 1) ;

   err = insertobject_x11display(x11disp, x11win, win) ;
   if (err) goto ONABORT ;

   x11win->display      = x11disp ;
   x11win->sys_window   = win ;
   x11win->sys_colormap = colormap ;
   x11win->iimpl        = eventhandler ;
   x11win->state        = x11window_Hidden ;
   x11win->flags        = flags ;

   return 0 ;
ONABORT:
   if (0 != (flags & x11window_OwnWindow)) {
      XDestroyWindow(x11disp->sys_display, win) ;
   }
   if (0 != (flags & x11window_OwnColormap)) {
      XFreeColormap(x11disp->sys_display, colormap) ;
   }
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

int init_x11window(/*out*/x11window_t * x11win, struct x11screen_t * x11screen, const struct x11window_it * eventhandler, uint8_t nrofattributes, const struct x11attribute_t * configuration/*[nrofattributes]*/)
{
   int err ;
   bool     isBackBuffer = false ;
   Visual * visual = XDefaultVisual(x11screen->display->sys_display, x11screen->nrscreen) ;
   int      depth  = XDefaultDepth(x11screen->display->sys_display, x11screen->nrscreen) ;

   *x11win = (x11window_t) x11window_INIT_FREEABLE ;

   err = matchvisual_x11window(x11screen, &visual, &depth, &isBackBuffer, nrofattributes, configuration) ;
   if (err) goto ONABORT ;

   err = initbasetype_x11window(x11win, eventhandler, x11screen->display, RootWindow(x11screen->display->sys_display, x11screen->nrscreen), visual, depth, nrofattributes, configuration) ;
   if (err) goto ONABORT ;

   if (isBackBuffer) {
      err = allocatebackbuffer_x11window(x11win) ;
      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   free_x11window(x11win) ;
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

int free_x11window(x11window_t * x11win)
{
   int err, err2 ;
   x11display_t * x11disp = x11win->display ;

   if (x11disp) {
      err = 0 ;

      if (0 != (x11win->flags & x11window_OwnBackBuffer)) {
         if (!XdbeDeallocateBackBufferName(x11disp->sys_display, x11win->sys_backbuffer)) {
            err = EINVAL ;
         }
      }

      err2 = freebasetype_x11window(x11win) ;
      if (err2) err = err2 ;

      x11win->sys_backbuffer = 0 ;

      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORTFREE_ERRLOG(err) ;
   return err ;
}

int freebasetype_x11window(x11window_t * x11win)
{
   int err ;
   x11display_t * x11disp = x11win->display ;

   if (x11disp) {
      err = 0 ;

      if (0 != (x11win->flags & x11window_OwnColormap)) {
         XFreeColormap(x11disp->sys_display, x11win->sys_colormap) ;
      }

      if (0 != (x11win->flags & x11window_OwnWindow)) {

         err = removeobject_x11display(x11disp, x11win->sys_window) ;

         XDestroyWindow(x11disp->sys_display, x11win->sys_window) ;
      }

      x11win->display      = 0 ;
      x11win->sys_window   = 0 ;
      x11win->sys_colormap = 0 ;
      x11win->iimpl        = 0 ;
      x11win->state        = x11window_Destroyed ;
      x11win->flags        = 0 ;

      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORTFREE_ERRLOG(err) ;
   return err ;
}

// group: query

x11screen_t screen_x11window(const x11window_t * x11win)
{
   XWindowAttributes winattr ;
   uint16_t          nrscreen ;

   if (!XGetWindowAttributes(x11win->display->sys_display, x11win->sys_window, &winattr)) {
      TRACESYSCALL_ERRLOG("XGetWindowAttributes", EINVAL) ;
      nrscreen = 0 ;
   } else {
      nrscreen = (uint16_t)XScreenNumberOfScreen(winattr.screen) ;
   }

   return (x11screen_t) x11screen_INIT(x11win->display, nrscreen) ;
}

int title_x11window(const x11window_t * x11win, /*ret*/struct cstring_t * title)
{
   int err ;
   XTextProperty  textprop  = { .value = 0, .nitems = 0 } ;
   char **        textlist  = 0 ;
   int            textcount = 0 ;

   VALIDATE_INPARAM_TEST(x11win->state != x11window_Destroyed, ONABORT, ) ;

   if (  0 == XGetWMName(x11win->display->sys_display, x11win->sys_window, &textprop)
         || Success != Xutf8TextPropertyToTextList(x11win->display->sys_display, &textprop, &textlist, &textcount)) {
      err = EINVAL ;
      goto ONABORT ;
   }

   err = textcount ? append_cstring(title, strlen(textlist[0]), textlist[0]) : 0 ;

   XFree(textprop.value) ;
   XFreeStringList(textlist) ;

   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   if (textprop.value)  XFree(textprop.value) ;
   if (textlist)        XFreeStringList(textlist) ;
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

int geometry_x11window(const x11window_t * x11win, /*out*/int32_t * screen_x, /*out*/int32_t * screen_y, /*out*/uint32_t * width, /*out*/uint32_t * height)
{
   int err ;
   Window   root ;
   Window   windummy ;
   uint32_t udummy ;
   int32_t  idummy ;

   VALIDATE_INPARAM_TEST(x11win->state != x11window_Destroyed, ONABORT, ) ;


   if (! XGetGeometry(x11win->display->sys_display, x11win->sys_window, &root, &idummy, &idummy,
                      width ? width : &udummy, height ? height : &udummy, &udummy, &udummy)) {
      err = EINVAL ;
      goto ONABORT ;
   }

   if (screen_x || screen_y) {
      XTranslateCoordinates(x11win->display->sys_display, x11win->sys_window, root, 0, 0, screen_x ? screen_x : &idummy, screen_y ? screen_y : &idummy, &windummy) ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

int frame_x11window(const x11window_t * x11win, /*out*/int32_t * screen_x, /*out*/int32_t * screen_y, /*out*/uint32_t * width, /*out*/uint32_t * height)
{
   int err ;
   int32_t     x, y ;
   uint32_t    w, h ;
   uint32_t *  data = 0 ;

   VALIDATE_INPARAM_TEST(x11win->state != x11window_Destroyed, ONABORT, ) ;

   err = geometry_x11window(x11win, &x, &y, &w, &h) ;
   if (err) goto ONABORT ;

   Atom           type ;
   int            format ;
   unsigned long  items ;
   unsigned long  unread_bytes ;

   if (  0 == XGetWindowProperty(x11win->display->sys_display, x11win->sys_window, x11win->display->atoms._NET_FRAME_EXTENTS,
                                 0, 4, False, XA_CARDINAL, &type, &format,
                                 &items, &unread_bytes, (unsigned char**)&data)
         && type == XA_CARDINAL && format == 32 && items == 4 && ! unread_bytes && data) {
      /* data[] = { left, right, top, bottom } */
      x -= (int32_t) data[0] ;
      y -= (int32_t) data[2] ;
      w += data[0] + data[1] ;
      h += data[2] + data[3] ;
   }

   if (screen_x) *screen_x = x ;
   if (screen_y) *screen_y = y ;
   if (width)    *width  = w ;
   if (height)   *height = h ;

   if (data) XFree(data) ;

   return 0 ;
ONABORT:
   if (data) XFree(data) ;
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

// group: update

int show_x11window(x11window_t * x11win)
{
   int err ;

   VALIDATE_INPARAM_TEST(x11win->state != x11window_Destroyed, ONABORT, ) ;

   XMapRaised(x11win->display->sys_display, x11win->sys_window) ;

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

int hide_x11window(x11window_t * x11win)
{
   int err ;

   VALIDATE_INPARAM_TEST(x11win->state != x11window_Destroyed, ONABORT, ) ;

   XUnmapWindow(x11win->display->sys_display, x11win->sys_window) ;

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

int sendcloserequest_x11window(x11window_t * x11win)
{
   int err ;

   VALIDATE_INPARAM_TEST(x11win->state != x11window_Destroyed, ONABORT, ) ;

   XEvent xevent ;
   xevent.xclient.type    = ClientMessage ;
   xevent.xclient.window  = x11win->sys_window ;
   xevent.xclient.message_type = x11win->display->atoms.WM_PROTOCOLS ;
   xevent.xclient.data.l[0]    = (int32_t) x11win->display->atoms.WM_DELETE_WINDOW ;
   xevent.xclient.format       = 32 ;
   XSendEvent(x11win->display->sys_display, x11win->sys_window, 1, 0, &xevent) ;

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

int sendredraw_x11window(x11window_t * x11win)
{
   int err ;

   VALIDATE_INPARAM_TEST(x11win->state != x11window_Destroyed, ONABORT, ) ;

   // the background is set to none => only Expose events are generated, background is not cleared !
   XClearArea(x11win->display->sys_display, x11win->sys_window, 0, 0, 0, 0, True) ;

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

int settitle_x11window(const x11window_t * x11win, /*ret*/const char * title)
{
   int err ;
   XTextProperty textprop ;
   char  *       textlist = CONST_CAST(char, title) ;

   VALIDATE_INPARAM_TEST(x11win->state != x11window_Destroyed, ONABORT, ) ;

   if (Success != Xutf8TextListToTextProperty(x11win->display->sys_display, &textlist, 1, XUTF8StringStyle, &textprop)) {
      err = EINVAL ;
      goto ONABORT ;
   }

   XSetWMName(x11win->display->sys_display, x11win->sys_window, &textprop) ;
   XFree(textprop.value) ;

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

int setopacity_x11window(x11window_t * x11win, double opacity)
{
   int err ;

   VALIDATE_INPARAM_TEST(x11win->state != x11window_Destroyed, ONABORT, ) ;
   VALIDATE_INPARAM_TEST(0 <=opacity && opacity <= 1, ONABORT, ) ;

   uint32_t cardinal_opacity = (uint32_t) (opacity * UINT32_MAX) ;

   setwinopacity_x11window(x11win->display, x11win->sys_window, cardinal_opacity) ;

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

int setpos_x11window(x11window_t * x11win, int32_t screen_x, int32_t screen_y)
{
   int err ;

   VALIDATE_INPARAM_TEST(x11win->state != x11window_Destroyed, ONABORT, ) ;

   XMoveWindow(x11win->display->sys_display, x11win->sys_window, screen_x, screen_y) ;

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

int resize_x11window(x11window_t * x11win, uint32_t width, uint32_t height)
{
   int err ;

   VALIDATE_INPARAM_TEST(x11win->state != x11window_Destroyed, ONABORT, ) ;

   XResizeWindow(x11win->display->sys_display, x11win->sys_window, width, height) ;

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

int swapbuffer_x11window(x11window_t * x11win)
{
   int err ;

   VALIDATE_INPARAM_TEST(x11win->state != x11window_Destroyed, ONABORT, ) ;

   XdbeSwapInfo swapInfo ;
   swapInfo.swap_window = x11win->sys_window ;
   swapInfo.swap_action = XdbeUndefined ;

   if (!XdbeSwapBuffers(x11win->display->sys_display, &swapInfo, 1)) {
      err = EINVAL ;
      goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

#define WINDOW_TITLE    "test üöä title"

typedef struct testwindow_t         testwindow_t ;

struct testwindow_t {
   x11window_t x11win ;
   int closerequest ;
   int destroy ;
   int redraw ;
   int repos ;
   int resize ;
   int showhide ;
   int32_t  x ;
   int32_t  y ;
   uint32_t width ;
   uint32_t height ;
} ;

#define testwindow_INIT_FREEABLE    { x11window_INIT_FREEABLE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }

x11window_it_DECLARE(testwindow_it, testwindow_t) ;

static void handlecloserequest_testwindow(testwindow_t * testwin)
{
   ++ testwin->closerequest ;
}

static void handledestroy_testwindow(testwindow_t * testwin)
{
   ++ testwin->destroy ;
}

static void handleredraw_testwindow(testwindow_t * testwin)
{
   ++ testwin->redraw ;
}

static void handlerepos_testwindow(testwindow_t * testwin, int32_t x, int32_t y, uint32_t width, uint32_t height)
{
   ++ testwin->repos ;
   testwin->x      = x ;
   testwin->y      = y ;
   testwin->width  = width ;
   testwin->height = height ;
}

static void handleresize_testwindow(testwindow_t * testwin, uint32_t width, uint32_t height)
{
   ++ testwin->resize ;
   testwin->width  = width ;
   testwin->height = height ;
}

static void handleshowhide_testwindow(testwindow_t * testwin)
{
   ++ testwin->showhide ;
}

static int test_interface(void)
{
   testwindow_t   testwin = testwindow_INIT_FREEABLE ;
   testwindow_it  iimpl   = x11window_it_INIT(_testwindow) ;

   // TEST x11window_it_DECLARE
   static_assert(offsetof(testwindow_it, closerequest) == 0 * sizeof(void(*)(void)), "") ;
   static_assert(offsetof(testwindow_it, destroy)      == 1 * sizeof(void(*)(void)), "") ;
   static_assert(offsetof(testwindow_it, redraw)       == 2 * sizeof(void(*)(void)), "") ;
   static_assert(offsetof(testwindow_it, repos)        == 3 * sizeof(void(*)(void)), "") ;
   static_assert(offsetof(testwindow_it, resize)       == 4 * sizeof(void(*)(void)), "") ;
   static_assert(offsetof(testwindow_it, showhide)     == 5 * sizeof(void(*)(void)), "") ;
   static_assert(offsetof(x11window_it, closerequest)  == 0 * sizeof(void(*)(void)), "") ;
   static_assert(offsetof(x11window_it, destroy)       == 1 * sizeof(void(*)(void)), "") ;
   static_assert(offsetof(x11window_it, redraw)        == 2 * sizeof(void(*)(void)), "") ;
   static_assert(offsetof(x11window_it, repos)         == 3 * sizeof(void(*)(void)), "") ;
   static_assert(offsetof(x11window_it, resize)        == 4 * sizeof(void(*)(void)), "") ;
   static_assert(offsetof(x11window_it, showhide)      == 5 * sizeof(void(*)(void)), "") ;

   // TEST x11window_it_INIT
   TEST(iimpl.closerequest == &handlecloserequest_testwindow) ;
   TEST(iimpl.destroy  == &handledestroy_testwindow) ;
   TEST(iimpl.redraw   == &handleredraw_testwindow) ;
   TEST(iimpl.showhide == &handleshowhide_testwindow) ;

   // TEST genericcast_x11windowit (x11window_it_DECLARE)
   TEST(genericcast_x11windowit(&iimpl, testwindow_t) == (x11window_it*)&iimpl) ;

   // TEST handlecloserequest_testwindow
   for (int i = 0; i < 10; ++i) {
      TEST(testwin.closerequest == i) ;
      iimpl.closerequest(&testwin) ;
   }
   TEST(testwin.closerequest == 10) ;

   // TEST handledestroy_testwindow
   for (int i = 0; i < 10; ++i) {
      TEST(testwin.destroy == i) ;
      iimpl.destroy(&testwin) ;
   }
   TEST(testwin.destroy == 10) ;

   // TEST handleredraw_testwindow
   for (int i = 0; i < 10; ++i) {
      TEST(testwin.redraw == i) ;
      iimpl.redraw(&testwin) ;
   }
   TEST(testwin.redraw == 10) ;

   // TEST handleshowhide_testwindow
   for (int i = 0; i < 10; ++i) {
      TEST(testwin.showhide == i) ;
      iimpl.showhide(&testwin) ;
   }
   TEST(testwin.showhide == 10) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

#define WAITFOR(x11disp, loop_count, CONDITION)                   \
   XFlush(x11disp->sys_display) ;                                 \
   for (int _count = 0; _count < loop_count; ++_count) {          \
      while (XPending(x11disp->sys_display)) {                    \
         dispatchevent_X11(x11disp) ;                             \
      }                                                           \
      if (CONDITION) break ;                                      \
      sleepms_thread(20) ;                                        \
   }

static int test_initfree(x11screen_t * x11screen)
{
   testwindow_t   testwin = testwindow_INIT_FREEABLE ;
   testwindow_it  iimpl   = x11window_it_INIT(_testwindow) ;
   x11window_t *  x11win  = &testwin.x11win ;
   x11display_t * x11disp = x11screen->display ;
   uint32_t       syswin ;
   void         * object ;

   static_assert(x11window_Destroyed == 0, "x11window_INIT_FREEABLE and free_x11window sets x11window_t.state to x11window_Destroyed") ;

   // TEST x11window_INIT_FREEABLE
   TEST(x11win->display      == 0) ;
   TEST(x11win->sys_window   == 0) ;
   TEST(x11win->sys_colormap == 0) ;
   TEST(x11win->iimpl        == 0) ;
   TEST(x11win->state        == 0) ;
   TEST(x11win->flags        == 0) ;
   TEST(x11win->sys_backbuffer == 0) ;

   // TEST init_x11window, free_x11window
   TEST(0 == init_x11window(x11win, x11screen, 0, 0, 0)) ;
   TEST(x11win->display      == x11disp) ;
   TEST(x11win->sys_window   != 0) ;
   TEST(x11win->sys_colormap != 0) ;
   TEST(x11win->iimpl        == 0) ;
   TEST(x11win->state        == x11window_Hidden) ;
   TEST(x11win->flags        == (x11window_OwnWindow|x11window_OwnColormap)) ;
   TEST(x11win->sys_backbuffer == 0) ;
   syswin = x11win->sys_window ;
   object = 0 ;
   TEST(0 == findobject_x11display(x11disp, &object, syswin)) ;
   TEST(x11win == object) ;
   TEST(0 == free_x11window(x11win)) ;
   TEST(x11win->display      == 0) ;
   TEST(x11win->sys_window   == 0) ;
   TEST(x11win->sys_colormap == 0) ;
   TEST(x11win->iimpl        == 0) ;
   TEST(x11win->state        == 0) ;
   TEST(x11win->flags        == 0) ;
   TEST(x11win->sys_backbuffer == 0) ;
   // object does no longer exist and also window handle is removed
   TEST(ESRCH == findobject_x11display(x11disp, &object, syswin)) ;
   TEST(0 == free_x11window(x11win)) ;
   TEST(x11win->display      == 0) ;
   TEST(x11win->sys_window   == 0) ;
   TEST(x11win->sys_colormap == 0) ;
   TEST(x11win->iimpl        == 0) ;
   TEST(x11win->state        == 0) ;
   TEST(x11win->flags        == 0) ;
   TEST(x11win->sys_backbuffer == 0) ;
   WAITFOR(x11disp, 5, false) ;

   // TEST init_x11window, free_x11window: x11attribute_INIT_DOUBLEBUFFER
   x11attribute_t config[] = { x11attribute_INIT_DOUBLEBUFFER } ;
   TEST(0 == init_x11window(x11win, x11screen, genericcast_x11windowit(&iimpl, testwindow_t), 1, config)) ;
   TEST(x11win->display      == x11disp) ;
   TEST(x11win->sys_window   != 0) ;
   TEST(x11win->sys_colormap != 0) ;
   TEST(x11win->iimpl        == genericcast_x11windowit(&iimpl, testwindow_t)) ;
   TEST(x11win->state        == x11window_Hidden) ;
   TEST(x11win->flags        == (x11window_OwnWindow|x11window_OwnColormap|x11window_OwnBackBuffer)) ;
   TEST(x11win->sys_backbuffer != 0) ;
   TEST(x11win->sys_backbuffer != x11win->sys_window) ;
   syswin = x11win->sys_window ;
   object = 0 ;
   TEST(0 == findobject_x11display(x11disp, &object, syswin)) ;
   TEST(x11win == object) ;
   TEST(0 == free_x11window(x11win)) ;
   // object does no longer exist and also window handle is removed
   TEST(ESRCH == findobject_x11display(x11disp, &object, syswin)) ;
   TEST(x11win->display      == 0) ;
   TEST(x11win->sys_window   == 0) ;
   TEST(x11win->sys_colormap == 0) ;
   TEST(x11win->iimpl        == 0) ;
   TEST(x11win->state        == 0) ;
   TEST(x11win->flags        == 0) ;
   TEST(x11win->sys_backbuffer == 0) ;
   WAITFOR(x11disp, 5, false) ;

   // TEST free_x11window: XDestroyWindow called outside free_x11window (or another process for example)
   TEST(0 == init_x11window(x11win, x11screen, genericcast_x11windowit(&iimpl, testwindow_t), 0, 0)) ;
   TEST(x11win->display      == x11disp) ;
   TEST(x11win->sys_window   != 0) ;
   TEST(x11win->sys_colormap != 0) ;
   TEST(x11win->iimpl        == genericcast_x11windowit(&iimpl, testwindow_t)) ;
   TEST(x11win->state        == x11window_Hidden) ;
   TEST(x11win->flags        == (x11window_OwnWindow|x11window_OwnColormap)) ;
   TEST(x11win->sys_backbuffer == 0) ;
   syswin = x11win->sys_window ;
   object = 0 ;
   TEST(0 != XDestroyWindow(x11disp->sys_display, x11win->sys_window)) ;
   TEST(0 == findobject_x11display(x11disp, &object, syswin)) ;
   TEST(x11win == object) ;
   testwin.destroy = 0 ;
   WAITFOR(x11disp, 5, x11win->state == x11window_Destroyed) ;
   TEST(testwin.destroy      == 1) ;
   TEST(x11win->display      == x11disp) ;
   TEST(x11win->sys_window   == 0) ;
   TEST(x11win->sys_colormap != 0) ;
   TEST(x11win->iimpl        == genericcast_x11windowit(&iimpl, testwindow_t)) ;
   TEST(x11win->state        == x11window_Destroyed) ;
   TEST(x11win->flags        == (x11window_OwnColormap)) ;
   TEST(x11win->sys_backbuffer == 0) ;
   // removed from id -> object map
   TEST(ESRCH == findobject_x11display(x11disp, &object, syswin)) ;
   TEST(0 == free_x11window(x11win)) ;
   TEST(x11win->display      == 0) ;
   TEST(x11win->sys_window   == 0) ;
   TEST(x11win->sys_colormap == 0) ;
   TEST(x11win->state        == 0) ;
   TEST(x11win->flags        == 0) ;
   TEST(x11win->sys_backbuffer == 0) ;

   return 0 ;
ONABORT:
   free_x11window(x11win) ;
   return EINVAL ;
}

static int compare_visual(x11screen_t * x11screen, Visual * visual, int depth, int minrgbbits, int minalphabits, bool isDouble)
{
   XVisualInfo             vinfo_pattern = { .visualid = visual->visualid, .class = TrueColor, .screen = x11screen->nrscreen } ;
   int                     vinfo_length  = 0 ;
   XVisualInfo *           vinfo         = XGetVisualInfo(x11screen->display->sys_display, VisualIDMask|VisualClassMask|VisualScreenMask, &vinfo_pattern, &vinfo_length) ;
   XdbeScreenVisualInfo *  vinfodb       = 0 ;

   if (isDouble) {
      int nrscreen = 1 ;
      Drawable screens  = RootWindow(x11screen->display->sys_display, x11screen->nrscreen) ;
      vinfodb = XdbeGetVisualInfo(x11screen->display->sys_display, &screens, &nrscreen) ;
      TEST(vinfodb) ;
      int i ;
      for (i = 0; i < vinfodb[0].count; ++i) {
         if (vinfodb[0].visinfo[i].visual == visual->visualid) {
            break ;
         }
      }
      TEST(i < vinfodb[0].count) ;
   }

   TEST(vinfo != 0) ;
   TEST(vinfo_length == 1) ;
   TEST(vinfo->visual       == visual) ;
   TEST(vinfo->bits_per_rgb >= minrgbbits) ;
   TEST(vinfo->depth        == depth) ;
   if (minalphabits) {
      XRenderPictFormat * format = XRenderFindVisualFormat(x11screen->display->sys_display, visual) ;
      TEST(format) ;
      int alphabits = 0 ;
      int alphamask = format->direct.alphaMask ;
      while (alphamask) {
         alphamask >>= 1 ;
         alphabits +=  1 ;
      }
      TEST(alphabits >= minalphabits) ;
   }

   // unprepare
   if (vinfodb) XdbeFreeVisualInfo(vinfodb) ;
   XFree(vinfo) ;

   return 0 ;
ONABORT:
   if (vinfodb)   XdbeFreeVisualInfo(vinfodb) ;
   if (vinfo)     XFree(vinfo) ;
   return EINVAL ;
}

static int test_query(x11screen_t * x11screen, testwindow_t * testwin, testwindow_t * testwin_noframe)
{
   x11window_t *  x11win  = &testwin->x11win ;
   x11window_t *  x11win2 = &testwin_noframe->x11win ;
   cstring_t      title   = cstring_INIT ;
   x11window_t    dummy   = x11window_INIT_FREEABLE ;
   Visual *       visual ;
   int            depth ;
   bool           isBackBuffer ;

   // TEST matchvisual_x11window
   {
      x11attribute_t attr[] = { x11attribute_INIT_ALPHAOPACITY } ;
      TEST(0 == matchvisual_x11window(x11screen, &visual, &depth, &isBackBuffer, 1, attr)) ;
      TEST(0 == isBackBuffer) ;
      TEST(0 == compare_visual(x11screen, visual, depth, 0, 1, 0)) ;
   }  {
      x11attribute_t attr[] = { x11attribute_INIT_ALPHABITS(8) } ;
      TEST(0 == matchvisual_x11window(x11screen, &visual, &depth, &isBackBuffer, 1, attr)) ;
      TEST(0 == isBackBuffer) ;
      TEST(0 == compare_visual(x11screen, visual, depth, 0, 8, 0)) ;
   }  {
      x11attribute_t attr[] = { x11attribute_INIT_REDBITS(8) } ;
      TEST(0 == matchvisual_x11window(x11screen, &visual, &depth, &isBackBuffer, 1, attr)) ;
      TEST(0 == isBackBuffer) ;
      TEST(0 == compare_visual(x11screen, visual, depth, 8, 0, 0)) ;
   }  {
      x11attribute_t attr[] = { x11attribute_INIT_GREENBITS(8) } ;
      TEST(0 == matchvisual_x11window(x11screen, &visual, &depth, &isBackBuffer, 1, attr)) ;
      TEST(0 == isBackBuffer) ;
      TEST(0 == compare_visual(x11screen, visual, depth, 8, 0, 0)) ;
   }  {
      x11attribute_t attr[] = { x11attribute_INIT_BLUEBITS(8) } ;
      TEST(0 == matchvisual_x11window(x11screen, &visual, &depth, &isBackBuffer, 1, attr)) ;
      TEST(0 == isBackBuffer) ;
      TEST(0 == compare_visual(x11screen, visual, depth, 8, 0, 0)) ;
   }  {
      x11attribute_t attr[] = { x11attribute_INIT_DOUBLEBUFFER } ;
      TEST(0 == matchvisual_x11window(x11screen, &visual, &depth, &isBackBuffer, 1, attr)) ;
      TEST(1 == isBackBuffer) ;
      TEST(0 == compare_visual(x11screen, visual, depth, 0, 0, 1)) ;
   }

   // TEST isbackbuffer_x11window
   TEST(0 == isbackbuffer_x11window(x11win)) ;
   TEST(1 == isbackbuffer_x11window(x11win2)) ;
   for (int i = 0; i <= x11window_OwnBackBuffer; ++ i) {
      dummy.flags = (uint8_t)i ;
      TEST(isbackbuffer_x11window(&dummy) == (i == x11window_OwnBackBuffer? 1 : 0)) ;
   }

   // TEST backbuffer_x11window
   x11drawable_t backbuffer = backbuffer_x11window(x11win) ;
   TEST(backbuffer.display      == x11win->display) ;
   TEST(backbuffer.sys_drawable == 0) ;
   TEST(backbuffer.sys_colormap == x11win->sys_colormap) ;
   backbuffer = backbuffer_x11window(x11win2) ;
   TEST(backbuffer.display      == x11win2->display) ;
   TEST(backbuffer.sys_drawable == x11win2->sys_backbuffer) ;
   TEST(backbuffer.sys_colormap == x11win2->sys_colormap) ;

   // TEST flags_x11window
   for (uint8_t i = 15; i <= 15; --i) {
      dummy.flags = i ;
      TEST(flags_x11window(&dummy) == i) ;
   }

   // TEST state_x11window
   for (uint8_t i = 15; i <= 15; --i) {
      dummy.state = i ;
      TEST(state_x11window(&dummy) == i) ;
   }

   // TEST screen_x11window
   x11screen_t x11screen2 = screen_x11window(x11win) ;
   TEST(isequal_x11screen(x11screen, &x11screen2)) ;

   // TEST title_x11window
   TEST(0 == title_x11window(x11win, &title)) ;
   TEST(0 == strcmp(str_cstring(&title), WINDOW_TITLE)) ;
   clear_cstring(&title) ;
   TEST(0 == title_x11window(x11win2, &title)) ;
   TEST(0 == strcmp(str_cstring(&title), "")) ;

   // TEST geometry_x11window
   int32_t  x, y ;
   uint32_t w, h ;
   TEST(0 == geometry_x11window(x11win, &x, &y, &w, &h)) ;
   TEST(x > 100) ;
   TEST(y > 101) ;
   TEST(w == 200) ;
   TEST(h == 100) ;
   TEST(0 == geometry_x11window(x11win2, &x, &y, &w, &h)) ;
   TEST(x == 0) ;
   TEST(y == 1) ;
   TEST(w == 200) ;
   TEST(h == 100) ;

   // TEST frame_x11window
   int32_t  fx, fy ;
   uint32_t fw, fh ;
   TEST(0 == frame_x11window(x11win, &fx, &fy, &fw, &fh)) ;
   TEST(fx == 100) ;
   TEST(fy == 101) ;
   TEST(fw > 200) ;
   TEST(fh > 100) ;
   TEST(fw >= (w + (uint32_t)(x-fx))) ;
   TEST(fh >= (h + (uint32_t)(y-fy))) ;
   TEST(0 == frame_x11window(x11win2, &fx, &fy, &fw, &fh)) ;
   TEST(fx == 0) ;
   TEST(fy == 1) ;
   TEST(fw == 200) ;
   TEST(fh == 100) ;

   // TEST pos_x11window
   int32_t x2, y2 ;
   TEST(0 == geometry_x11window(x11win, &x, &y, &w, &h)) ;
   TEST(0 == pos_x11window(x11win, &x2, &y2)) ;
   TEST(x2 == x) ;
   TEST(y2 == y) ;
   TEST(0 == pos_x11window(x11win2, &x2, &y2)) ;
   TEST(x2 == 0) ;
   TEST(y2 == 1) ;

   // TEST size_x11window
   w = h = 0;
   TEST(0 == size_x11window(x11win, &w, &h)) ;
   TEST(w == 200) ;
   TEST(h == 100) ;
   w = h = 0;
   TEST(0 == size_x11window(x11win2, &w, &h)) ;
   TEST(w == 200) ;
   TEST(h == 100) ;

   // unprepare
   TEST(0 == free_cstring(&title)) ;
   WAITFOR(x11win->display, 1, false) ;

   return 0 ;
ONABORT:
   free_cstring(&title) ;
   return EINVAL ;
}

static int test_showhide(testwindow_t * testwin)
{
   x11window_t * x11win = &testwin->x11win ;

   // TEST show_x11window
   TEST(state_x11window(x11win) == x11window_Hidden) ;
   TEST(0 == show_x11window(x11win)) ;
   WAITFOR(x11win->display, 20, state_x11window(x11win) != x11window_Hidden) ;
   TEST(state_x11window(x11win) == x11window_Shown) ;

   // TEST hide_x11window
   TEST(0 == hide_x11window(x11win)) ;
   WAITFOR(x11win->display, 10, state_x11window(x11win) != x11window_Shown) ;
   TEST(state_x11window(x11win) == x11window_Hidden) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_update(testwindow_t * testwin)
{
   x11window_t *  x11win = &testwin->x11win ;
   cstring_t      title  = cstring_INIT ;

   // TEST settitle_x11window
   const char * tstr[] = { "new title \u1234", WINDOW_TITLE } ;
   for (unsigned ti = 0; ti < lengthof(tstr); ++ti) {
      TEST(0 == settitle_x11window(x11win, tstr[ti])) ;
      TEST(0 == title_x11window(x11win, &title)) ;
      TEST(0 == strcmp(str_cstring(&title), tstr[ti])) ;
      clear_cstring(&title) ;
   }

   // TEST sendcloserequest_x11window
   WAITFOR(x11win->display, 2, false) ;
   testwin->closerequest = 0 ;
   TEST(0 == sendcloserequest_x11window(x11win)) ;
   TEST(testwin->closerequest == 0) ;
   WAITFOR(x11win->display, 10, testwin->closerequest != 0) ;
   TEST(testwin->closerequest == 1) ;

   // TEST sendredraw_x11window
   TEST(0 == show_x11window(x11win)) ;
   WAITFOR(x11win->display, 3, state_x11window(x11win) == x11window_Shown) ;
   WAITFOR(x11win->display, 3, testwin->redraw != 0) ;
   TEST(0 == sendredraw_x11window(x11win)) ;
   testwin->redraw = 0 ;
   WAITFOR(x11win->display, 10, testwin->redraw != 0) ;
   TEST(testwin->redraw >= 1) ;

   // unprepare
   TEST(0 == free_cstring(&title)) ;
   WAITFOR(x11win->display, 10, false) ;

   return 0 ;
ONABORT:
   free_cstring(&title) ;
   return EINVAL ;
}

static int test_geometry(testwindow_t * testwin, testwindow_t * testwin_noframe)
{
   testwindow_t * testwins[2] = { testwin, testwin_noframe } ;
   int32_t        x, y ;
   int32_t        x2, y2 ;
   uint32_t       w, h ;

   for (unsigned ti = 0; ti < lengthof(testwins); ++ti) {
      x11window_t * x11win = &testwins[ti]->x11win ;

   // prepare
   WAITFOR(x11win->display, 10, x11win->state == x11window_Shown) ;
   TEST(0 == show_x11window(x11win)) ;
   WAITFOR(x11win->display, 10, x11win->state == x11window_Shown) ;
   TEST(x11win->state  == x11window_Shown) ;

   // TEST setpos_x11window, frame_x11window, geometry_x11window, pos_x11window, size_x11window
   for (int i = 0; i < 3; ++i) {
      WAITFOR(x11win->display, 1, false) ;
      int posx = 150 + 10*i ;
      int posy = 200 + 5*i ;
      TEST(0 == setpos_x11window(x11win, posx, posy)) ;
      ((testwindow_t*)x11win)->repos = 0 ;
      WAITFOR(x11win->display, 10, ((testwindow_t*)x11win)->x >= posx && ((testwindow_t*)x11win)->x <= posx+30) ;
      WAITFOR(x11win->display, 10, ((testwindow_t*)x11win)->repos) ;
      TEST(((testwindow_t*)x11win)->repos) ;
      if (0 == ti) {
         TEST(0 == frame_x11window(x11win, &x, &y, &w, &h)) ;
         TEST(w > 200) ;
         TEST(h > 100) ;
         TEST(0 == size_x11window(x11win, &w, &h)) ;
      } else {
         TEST(0 == geometry_x11window(x11win, &x, &y, &w, &h)) ;
      }
      TEST(x == posx) ;
      TEST(y == posy) ;
      TEST(w == 200) ;
      TEST(h == 100) ;
      TEST(0 == pos_x11window(x11win, &x, &y)) ;
      TEST(((testwindow_t*)x11win)->x      == x) ;
      TEST(((testwindow_t*)x11win)->y      == y) ;
      TEST(((testwindow_t*)x11win)->width  == w) ;
      TEST(((testwindow_t*)x11win)->height == h) ;
      TEST(x >= posx) ;
      TEST(y >= posy) ;
      if (ti) {
         TEST(x == 150 + 10*i) ;
         TEST(y == 200 + 5*i) ;
      }
   }
   TEST(0 == setpos_x11window(x11win, ti ? 0 : 100, ti ? 1 : 101)) ;
   ((testwindow_t*)x11win)->repos = 0 ;
   WAITFOR(x11win->display, 3, ((testwindow_t*)x11win)->repos) ;

   // TEST resize_x11window, frame_x11window, geometry_x11window, pos_x11window, size_x11window
   for (unsigned i = 2; i <= 2; --i) {
      WAITFOR(x11win->display, 1, false) ;
      TEST(0 == resize_x11window(x11win, 200u + 10u*i, 100u + 5u*i)) ;
      ((testwindow_t*)x11win)->resize = 0 ;
      WAITFOR(x11win->display, 10, ((testwindow_t*)x11win)->resize) ;
      TEST(((testwindow_t*)x11win)->resize) ;
      TEST(0 == size_x11window(x11win, &w, &h)) ;
      TEST(w == 200 + 10*i) ;
      TEST(h == 100 + 5*i) ;
      TEST(0 == frame_x11window(x11win, &x, &y, &w, &h)) ;
      TEST(x == (ti ? 0 : 100)) ;
      TEST(y == (ti ? 1 : 101)) ;
      TEST(w >= 200 + 10*i) ;
      TEST(h >= 100 + 5*i) ;
      TEST(0 == geometry_x11window(x11win, &x, &y, &w, &h)) ;
      TEST(w == 200 + 10*i) ;
      TEST(h == 100 + 5*i) ;
      TEST(x >= (ti ? 0 : 100)) ;
      TEST(y >= (ti ? 1 : 101)) ;
      if (!ti) {
         TEST(x >= 100) ;
         TEST(y >= 101) ;
      } else {
         TEST(x == 0) ;
         TEST(y == 1) ;
      }
      TEST(0 == pos_x11window(x11win, &x2, &y2)) ;
      TEST(x == x2) ;
      TEST(y == y2) ;
      TEST(((testwindow_t*)x11win)->x      == x) ;
      TEST(((testwindow_t*)x11win)->y      == y) ;
      TEST(((testwindow_t*)x11win)->width  == w) ;
      TEST(((testwindow_t*)x11win)->height == h) ;
   }
   TEST(0 == resize_x11window(x11win, 200, 100)) ;
   ((testwindow_t*)x11win)->resize = 0 ;
   WAITFOR(x11win->display, 10, ((testwindow_t*)x11win)->resize) ;

   } // repeat all test for other window

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_config(x11screen_t * x11screen)
{
   cstring_t   title  = cstring_INIT ;
   x11window_t x11win = x11window_INIT_FREEABLE ;
   XWindowAttributes winattr ;

   // TEST init_x11window: x11attribute_INIT_RGBA
   {
      x11attribute_t config[] = { x11attribute_INIT_RGBA(8, 8, 8, 0) } ;
      TEST(0 == init_x11window(&x11win, x11screen, 0, lengthof(config), config)) ;
      TEST(1 == XGetWindowAttributes(x11screen->display->sys_display, x11win.sys_window, &winattr)) ;
      TEST(0 == compare_visual(x11screen, winattr.visual, winattr.depth, 8, 0, 0)) ;
      TEST(0 == free_x11window(&x11win)) ;
   }

   // TEST init_x11window: x11attribute_INIT_WINPOS, x11attribute_INIT_WINSIZE
   {
      x11attribute_t config[] = { x11attribute_INIT_WINPOS(300, 340),  x11attribute_INIT_WINSIZE(123, 145)} ;
      TEST(0 == init_x11window(&x11win, x11screen, 0, lengthof(config), config)) ;
      TEST(1 == XGetWindowAttributes(x11screen->display->sys_display, x11win.sys_window, &winattr)) ;
      TEST(winattr.x      == 300) ;
      TEST(winattr.y      == 340) ;
      TEST(winattr.width  == 123) ;
      TEST(winattr.height == 145) ;
      TEST(0 == free_x11window(&x11win)) ;
   }

   // TEST init_x11window: x11attribute_INIT_ALPHAOPACITY
   {
      x11attribute_t config[] = { x11attribute_INIT_ALPHAOPACITY } ;
      TEST(0 == init_x11window(&x11win, x11screen, 0, lengthof(config), config)) ;
      TEST(1 == XGetWindowAttributes(x11screen->display->sys_display, x11win.sys_window, &winattr)) ;
      TEST(0 == compare_visual(x11screen, winattr.visual, winattr.depth, 0, 1, 0)) ;
      TEST(0 == free_x11window(&x11win)) ;
   }

   // TEST init_x11window: x11attribute_INIT_WINFRAME, x11attribute_INIT_WINTITLE
   {
      x11attribute_t config[] = { x11attribute_INIT_WINFRAME, x11attribute_INIT_WINTITLE("1TEXT2"), x11attribute_INIT_WINPOS(100, 110), x11attribute_INIT_WINSIZE(150, 185) } ;
      TEST(0 == init_x11window(&x11win, x11screen, 0, lengthof(config), config)) ;
      TEST(0 == show_x11window(&x11win)) ;
      WAITFOR(x11win.display, 10, x11win.state == x11window_Shown) ;
      TEST(x11win.state == x11window_Shown) ;
      TEST(1 == XGetWindowAttributes(x11screen->display->sys_display, x11win.sys_window, &winattr)) ;
      TEST(0 == frame_x11window(&x11win, &winattr.x, &winattr.y, 0, 0)) ;
      TEST(winattr.x      == 100) ;
      TEST(winattr.y      == 110) ;
      TEST(winattr.width  == 150) ;
      TEST(winattr.height == 185) ;
      TEST(0 == title_x11window(&x11win, &title)) ;
      TEST(0 == strcmp(str_cstring(&title), "1TEXT2")) ;
      clear_cstring(&title) ;
      TEST(0 == free_x11window(&x11win)) ;
   }

   // TEST init_x11window: x11attribute_INIT_WINMINSIZE, x11attribute_INIT_WINMAXSIZE
   {
      x11attribute_t config[] = { x11attribute_INIT_WINFRAME, x11attribute_INIT_WINMINSIZE(190, 191), x11attribute_INIT_WINSIZE(200, 201), x11attribute_INIT_WINMAXSIZE(210, 211) } ;
      TEST(0 == init_x11window(&x11win, x11screen, 0, lengthof(config), config)) ;
      TEST(0 == show_x11window(&x11win)) ;
      WAITFOR(x11win.display, 10, x11win.state == x11window_Shown) ;
      TEST(x11win.state == x11window_Shown) ;
      TEST(1 == XGetWindowAttributes(x11screen->display->sys_display, x11win.sys_window, &winattr)) ;
      TEST(winattr.width  == 200) ;
      TEST(winattr.height == 201) ;
      TEST(0 == resize_x11window(&x11win, 300, 300)) ; // resize will be clipped by WM
      for (int i = 0; i < 10; ++i) {
         WAITFOR(x11win.display, 1, false) ;
         TEST(1 == XGetWindowAttributes(x11screen->display->sys_display, x11win.sys_window, &winattr)) ;
         if (winattr.width != 200) break ;
      }
      TEST(winattr.width  == 210) ; // MAXSIZE
      TEST(winattr.height == 211) ; // MAXSIZE
      TEST(0 == resize_x11window(&x11win, 100, 100)) ; // resize will be clipped by WM
      for (int i = 0; i < 10; ++i) {
         WAITFOR(x11win.display, 1, false) ;
         TEST(1 == XGetWindowAttributes(x11screen->display->sys_display, x11win.sys_window, &winattr)) ;
         if (winattr.width != 210) break ;
      }
      TEST(winattr.width  == 190) ; // MINSIZE
      TEST(winattr.height == 191) ; // MINSIZE
      TEST(0 == free_x11window(&x11win)) ;
   }

   // unprepare
   TEST(0 == free_cstring(&title)) ;
   WAITFOR(x11screen->display, 1, false) ;

   return 0 ;
ONABORT:
   free_cstring(&title) ;
   free_x11window(&x11win) ;
   return EINVAL ;
}

static int compare_color(x11window_t * x11win, bool isRoot, uint32_t w, uint32_t h, bool isRed, bool isGreen, bool isBlue)
{
   XImage * ximg = 0 ;
   size_t   pixels ;
   uint32_t x, y ;

   if (isRoot) {
      Window root = RootWindow(x11win->display->sys_display, screen_x11window(x11win).nrscreen) ;
      Window windummy ;
      int32_t x2, y2 ;
      XTranslateCoordinates(x11win->display->sys_display, x11win->sys_window, root, 0, 0, &x2, &y2, &windummy) ;
      ximg = XGetImage(x11win->display->sys_display, root, x2, y2, w, h, (unsigned long)-1, ZPixmap) ;
   } else {
      ximg = XGetImage(x11win->display->sys_display, x11win->sys_window, 0, 0, w, h, (unsigned long)-1, ZPixmap) ;
   }
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

static int test_opacity(testwindow_t * testwin1, testwindow_t * testwin2)
{
   x11window_t * x11win1  = &testwin1->x11win ;
   x11window_t * x11win2  = &testwin2->x11win ;
   XColor         colred  = { .red = USHRT_MAX, .green = 0, .blue = 0, .flags = DoRed|DoGreen|DoBlue } ;
   XColor         colblue = { .red = 0, .green = 0, .blue = USHRT_MAX, .flags = DoRed|DoGreen|DoBlue } ;
   XColor         colblck = { .red = 0, .green = 0, .blue = 0, .flags = DoRed|DoGreen|DoBlue } ;
   GC             gc1 ;
   GC             gc2 ;

   // prepare
   XAllocColor(x11win1->display->sys_display, x11win1->sys_colormap, &colred) ;
   XAllocColor(x11win2->display->sys_display, x11win2->sys_colormap, &colblue) ;
   XAllocColor(x11win2->display->sys_display, x11win2->sys_colormap, &colblck) ;
   XGCValues gcvalues = { .foreground = colred.pixel  } ;
   gc1 = XCreateGC(x11win1->display->sys_display, x11win1->sys_window, GCForeground, &gcvalues) ;
   TEST(gc1) ;
   gcvalues.foreground = colblue.pixel ;
   gc2 = XCreateGC(x11win2->display->sys_display, x11win2->sys_window, GCForeground, &gcvalues) ;
   TEST(gc2) ;
   TEST(0 == show_x11window(x11win1)) ;
   TEST(0 == hide_x11window(x11win2)) ;
   WAITFOR(x11win1->display, 10, x11win1->state == x11window_Shown) ;
   WAITFOR(x11win2->display, 10, x11win2->state == x11window_Hidden) ;
   int32_t x, y ;
   TEST(0 == pos_x11window(x11win1, &x, &y)) ;
   TEST(0 == setpos_x11window(x11win2, x, y)) ;

   // TEST x11attribute_ALPHAOPACITY
   // draw background window red
   TEST(1 == XFillRectangle(x11win1->display->sys_display, x11win1->sys_window, gc1, 0, 0, 200, 100)) ;
   WAITFOR(x11win1->display, 1, false) ;
   TEST(0 == compare_color(x11win1, false, 200, 100, 1, 0, 0)) ;
   // draw overlay window blue
   TEST(0 == show_x11window(x11win2)) ;
   WAITFOR(x11win2->display, 10, x11win2->state == x11window_Shown) ;
   TEST(1 == XFillRectangle(x11win2->display->sys_display, x11win2->sys_window, gc2, 0, 0, 200, 100)) ;
   WAITFOR(x11win2->display, 1, false) ;
   TEST(0 == compare_color(x11win2, false, 200, 100, 0, 0, 1)) ;
   for (int i = 0; i < 20; ++i) {
      WAITFOR(x11win2->display, 1, false) ;
      if (0 == compare_color(x11win2, true, 200, 100, 0, 0, 1)) break ;
   }
   TEST(0 == compare_color(x11win2, true, 200, 100, 0, 0, 1)) ;
   // make overlay window transparent
   unsigned long alphamask = (colblue.pixel & colblck.pixel) ;
   XSetForeground(x11win2->display->sys_display, gc2, colblue.pixel ^ alphamask/*fully transparent*/) ;
   TEST(1 == XFillRectangle(x11win2->display->sys_display, x11win2->sys_window, gc2, 0, 0, 200, 100)) ;
   WAITFOR(x11win2->display, 1, false) ;
   TEST(0 == compare_color(x11win2, false, 200, 100, 0, 0, 1)) ;
   for (int i = 0; i< 20; ++i) {
      WAITFOR(x11win2->display, 1, false) ;
      if (0 == compare_color(x11win2, true, 200, 100, 1, 0, 1)) break ;
   }
   TEST(0 == compare_color(x11win2, true, 200, 100, 1, 0, 1)) ; // both colors visible

   // TEST setopacity_x11window
   XSetForeground(x11win2->display->sys_display, gc2, colblue.pixel /*fully opaque*/) ;
   TEST(1 == XFillRectangle(x11win2->display->sys_display, x11win2->sys_window, gc2, 0, 0, 200, 100)) ;
   WAITFOR(x11win2->display, 1, false) ;
   TEST(0 == compare_color(x11win2, false, 200, 100, 0, 0, 1)) ;
   for (int i = 0; i< 20; ++i) {
      WAITFOR(x11win2->display, 1, false) ;
      if (0 == compare_color(x11win2, true, 200, 100, 0, 0, 1)) break ;
   }
   TEST(0 == compare_color(x11win2, true, 200, 100, 0, 0, 1)) ;
   setopacity_x11window(x11win2, 0.5) ;
   for (int i = 0; i< 20; ++i) {
      WAITFOR(x11win2->display, 1, false) ;
      if (0 == compare_color(x11win2, true, 200, 100, 1, 0, 1)) break ;
   }
   TEST(0 == compare_color(x11win2, true, 200, 100, 1, 0, 1)) ; // both colors visible
   setopacity_x11window(x11win2, 0) ;
   for (int i = 0; i< 20; ++i) {
      WAITFOR(x11win2->display, 1, false) ;
      if (0 == compare_color(x11win2, true, 200, 100, 1, 0, 0)) break ;
   }
   TEST(0 == compare_color(x11win2, true, 200, 100, 1, 0, 0)) ; // overlay not visible
   setopacity_x11window(x11win2, 1) ;
   for (int i = 0; i< 20; ++i) {
      WAITFOR(x11win2->display, 1, false) ;
      if (0 == compare_color(x11win2, true, 200, 100, 0, 0, 1)) break ;
   }
   TEST(0 == compare_color(x11win2, true, 200, 100, 0, 0, 1)) ; // fully opaque

   // unprepare
   TEST(0 == setpos_x11window(x11win2, 0, 1)) ;
   XFreeGC(x11win1->display->sys_display, gc1) ;
   XFreeGC(x11win2->display->sys_display, gc2) ;
   WAITFOR(x11win1->display, 1, false) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_backbuffer(testwindow_t * testwin)
{
   x11window_t *  x11win   = &testwin->x11win ;
   XColor         colblue  = { .red = 0, .green = 0, .blue = USHRT_MAX, .flags = DoRed|DoGreen|DoBlue } ;
   XColor         colgreen = { .red = 0, .green = USHRT_MAX, .blue = 0, .flags = DoRed|DoGreen|DoBlue } ;
   x11drawable_t  backbuffer ;
   GC             gc ;

   // prepare
   TEST(isbackbuffer_x11window(x11win)) ;
   backbuffer = backbuffer_x11window(x11win) ;
   XAllocColor(x11win->display->sys_display, x11win->sys_colormap, &colblue) ;
   XAllocColor(x11win->display->sys_display, x11win->sys_colormap, &colgreen) ;
   XGCValues gcvalues = { .foreground = colgreen.pixel  } ;
   gc = XCreateGC(x11win->display->sys_display, x11win->sys_window, GCForeground, &gcvalues) ;
   TEST(gc) ;
   TEST(0 == setpos_x11window(x11win, 100, 100)) ;
   TEST(0 == show_x11window(x11win)) ;
   WAITFOR(x11win->display, 10, x11win->state == x11window_Shown) ;
   TEST(x11win->state == x11window_Shown) ;

   // TEST window foreground green
   TEST(1 == XFillRectangle(x11win->display->sys_display, x11win->sys_window, gc, 0, 0, 200, 100)) ;
   WAITFOR(x11win->display, 1, false) ;
   TEST(0 == compare_color(x11win, false, 200, 100, 0, 1, 0)) ;

   // TEST window background blue / foreground green
   gcvalues.foreground = colblue.pixel ;
   TEST(1 == XChangeGC(x11win->display->sys_display, gc, GCForeground, &gcvalues)) ;
   TEST(1 == XFillRectangle(x11win->display->sys_display, backbuffer.sys_drawable, gc, 0, 0, 200, 100)) ;
   WAITFOR(x11win->display, 1, false) ;
   TEST(0 == compare_color(x11win, false, 200, 100, 0, 1, 0)) ;

   // TEST window background blue / foreground blue
   TEST(0 == swapbuffer_x11window(x11win)) ;
   WAITFOR(x11win->display, 1, false) ;
   TEST(0 == compare_color(x11win, false, 200, 100, 0, 0, 1)) ;

   // unprepare
   TEST(0 == setpos_x11window(x11win, 0, 1)) ;
   XFreeGC(x11win->display->sys_display, gc) ;
   WAITFOR(x11win->display, 1, false) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

int unittest_platform_X11_x11window()
{
   x11display_t      x11disp   = x11display_INIT_FREEABLE ;
   testwindow_t      x11win    = testwindow_INIT_FREEABLE ;
   testwindow_t      x11win2   = testwindow_INIT_FREEABLE ;
   testwindow_it     iimpl     = x11window_it_INIT(_testwindow) ;
   x11screen_t       x11screen = x11screen_INIT_FREEABLE ;
   x11attribute_t    config[]  = {  x11attribute_INIT_WINFRAME,
                                    x11attribute_INIT_WINTITLE(WINDOW_TITLE),
                                    x11attribute_INIT_WINSIZE(200, 100)
                                 } ;
   x11attribute_t    config2[] = {  x11attribute_INIT_DOUBLEBUFFER,
                                    x11attribute_INIT_WINSIZE(200, 100),
                                    x11attribute_INIT_ALPHAOPACITY
                                 } ;
   resourceusage_t   usage     = resourceusage_INIT_FREEABLE ;

   // prepare
   TEST(0 == init_x11display(&x11disp, 0)) ;
   x11screen = defaultscreen_x11display(&x11disp) ;
   // with frame
   TEST(0 == init_x11window(&x11win.x11win, &x11screen, genericcast_x11windowit(&iimpl, testwindow_t), lengthof(config), config)) ;
   // with doublebuffer (backbuffer)
   TEST(0 == init_x11window(&x11win2.x11win, &x11screen, genericcast_x11windowit(&iimpl, testwindow_t), lengthof(config2), config2)) ;

   if (test_initfree(&x11screen))            goto ONABORT ;
   if (test_showhide(&x11win))               goto ONABORT ;
   if (test_update(&x11win))                 goto ONABORT ;
   if (test_geometry(&x11win,&x11win2))      goto ONABORT ;
   if (test_config(&x11screen))              goto ONABORT ;
   if (test_opacity(&x11win, &x11win2))      goto ONABORT ;
   if (test_backbuffer(&x11win2))            goto ONABORT ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_interface())                     goto ONABORT ;
   if (test_query(&x11screen, &x11win,
                  &x11win2))                 goto ONABORT ;
   if (test_update(&x11win))                 goto ONABORT ;
   if (test_geometry(&x11win,&x11win2))      goto ONABORT ;
   if (test_config(&x11screen))              goto ONABORT ;
   if (test_opacity(&x11win, &x11win2))      goto ONABORT ;
   if (test_backbuffer(&x11win2))            goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   // unprepare
   TEST(0 == free_x11window(&x11win.x11win)) ;
   TEST(0 == free_x11window(&x11win2.x11win)) ;
   TEST(0 == free_x11display(&x11disp)) ;

   return 0 ;
ONABORT:
   (void) free_x11window(&x11win.x11win) ;
   (void) free_x11window(&x11win2.x11win) ;
   (void) free_x11display(&x11disp) ;
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
