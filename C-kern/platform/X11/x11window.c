/* title: X11-Window impl

   Implements <X11-Window>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2013 Jörg Seebohn

   file: C-kern/api/platform/X11/x11window.h
    Header file <X11-Window>.

   file: C-kern/platform/X11/x11window.c
    Implementation file <X11-Window impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/platform/X11/x11window.h"
#include "C-kern/api/err.h"
#include "C-kern/api/graphic/display.h"
#include "C-kern/api/graphic/gconfig.h"
#include "C-kern/api/graphic/windowconfig.h"
#include "C-kern/api/platform/X11/x11.h"
#include "C-kern/api/platform/X11/x11display.h"
#include "C-kern/api/platform/X11/x11drawable.h"
#include "C-kern/api/string/cstring.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/resourceusage.h"
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/platform/task/thread.h"
#endif
#include STR(C-kern/api/platform/KONFIG_OS/graphic/sysx11.h)


// section: x11window_t

// group: helper

static inline void compiletime_assert(void)
{
   static_assert( ((x11drawable_t*)0) == cast_x11drawable((x11window_t*)0),
                  "x11window_t can be cast to x11drawable_t");
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

static int matchvisual_x11window(
      /*out*/VisualID *    visualid,
      x11display_t   *     x11disp,
      uint32_t             screennr,
      const int32_t *      gconf_attrib)
{
   int32_t  rgbbits   = 0;
   int32_t  alphabits = 0;
   int32_t  buffbits  = 0;
   bool     isOpacity = false;

   for (unsigned i = 0; gconf_attrib[i] != gconfig_NONE; i += 2) {
      if (i >= 2*gconfig__NROF) {
         return E2BIG;
      }
      switch ((gconfig_e)gconf_attrib[i]) {
      default:
         return EINVAL;
      case gconfig_NONE:
      case gconfig_CONFORMANT:
      case gconfig_TYPE:
      case gconfig_BITS_DEPTH:
      case gconfig_BITS_STENCIL:
         // ignore
         break;
      case gconfig_BITS_BUFFER:
         if (gconf_attrib[i+1] > buffbits) {
            buffbits = gconf_attrib[i+1];
         }
         break;
      case gconfig_BITS_RED:
      case gconfig_BITS_GREEN:
      case gconfig_BITS_BLUE:
         if (gconf_attrib[i+1] > rgbbits) {
            rgbbits = gconf_attrib[i+1];
         }
         break ;
      case gconfig_BITS_ALPHA:
         alphabits = gconf_attrib[i+1];
         break ;
      case gconfig_TRANSPARENT_ALPHA:
         isOpacity = (gconf_attrib[i+1] != 0);
         break ;
      }
   }

   if (isOpacity && alphabits == 0) {
      alphabits = 1 ;
   }

   XVisualInfo    vinfo_pattern = { .class = TrueColor, .screen = (int32_t) screennr } ;
   int            vinfo_length  = 0 ;
   XVisualInfo *  vinfo         = XGetVisualInfo(x11disp->sys_display, VisualClassMask|VisualScreenMask, &vinfo_pattern, &vinfo_length);

   bool isMatch = false;

   if (  vinfo
         && (!alphabits || x11disp->xrender.isSupported)) {
      for (int i = 0; i < vinfo_length; ++i) {
         if (  rgbbits <= vinfo[i].bits_per_rgb
               && 3 * vinfo[i].bits_per_rgb + alphabits <= vinfo[i].depth
               && buffbits <= vinfo[i].depth) {
            if (alphabits) {  // check alpha
               // alphaMask gives the number of bits (always beginning from 1)
               // alpha gives the shift to the left
               XRenderPictFormat * format = XRenderFindVisualFormat(x11disp->sys_display, vinfo[i].visual) ;
               if (!format || format->direct.alphaMask < (((int32_t)1 << alphabits)-1)) continue ;
            }
            isMatch   = true ;
            *visualid = vinfo[i].visualid ;
            break;
         }
      }
   }

   if (vinfo)     XFree(vinfo) ;
   if (! isMatch) goto ONERR;

   return 0 ;
ONERR:
   return ESRCH ;
}

// group: lifetime


/* function: initvid_x11window
 * Called from <x11window_t.init_x11window>. */
int initvid_x11window(
      /*out*/x11window_t *          x11win,
      struct x11display_t *         x11disp,
      uint32_t                      screennr,
      const struct x11window_evh_t* eventhandler,
      /*(X11) VisualID*/xid_t       config_visualid,
      const struct windowconfig_t*  winconf_attrib
      )
{
   int err;
   Window      win;
   Colormap    colormap;
   uint8_t     flags      = 0;
   uint8_t     isFrame    = false;
   uint8_t     opacity    = 255;
   const char* title      = 0;
   XWMHints    wm_hints   = { .flags = StateHint, .initial_state = NormalState /*or WithdrawnState or IconicState*/ };
   XSizeHints  size_hints = { .flags = PBaseSize|PWinGravity,
                              .x = 0, .y = 0,
                              .base_width  = 100, .base_height = 100,
                              .win_gravity = NorthWestGravity
                           };
   XColor      colwhite   = { .red = (unsigned short)-1, .green = (unsigned short)-1, .blue = (unsigned short)-1, .flags = DoRed|DoGreen|DoBlue };
   uint32_t    maxnr      = (uint32_t) ScreenCount(x11disp->sys_display);
   Display*    display    = x11disp->sys_display;

   VALIDATE_INPARAM_TEST(screennr < maxnr, ONERR, );

   Window      parent_win = RootWindow(display, screennr);
   Visual *    visual;
   int         depth;

   // convert visualid into visual/depth
   {
      int            vinfo_length;
      XVisualInfo    vinfo_pattern = { .visualid = config_visualid, .screen = (int32_t) screennr};
      XVisualInfo *  vinfo         = XGetVisualInfo(x11disp->sys_display, VisualIDMask|VisualScreenMask, &vinfo_pattern, &vinfo_length);

      if (!vinfo){
         err = EINVAL;
         goto ONERR;
      }

      visual = vinfo->visual;
      depth  = vinfo->depth;

      XFree(vinfo);
   }

   // process x11 window specific attributes
   if (winconf_attrib) {
      uint_fast8_t i = 0;
      for (windowconfig_e type; windowconfig_NONE != (type = readtype_windowconfig(winconf_attrib, &i)); ) {
         if (i > 3*windowconfig__NROF) {
            err = E2BIG;
            goto ONERR;
         }
         switch (type) {
         default:
            err= EINVAL;
            goto ONERR;
         case windowconfig_FRAME:
            isFrame = true;
            break;
         case windowconfig_MAXSIZE:
            size_hints.flags |= PMaxSize;
            readmaxsize_windowconfig(winconf_attrib, &i, &size_hints.max_width, &size_hints.max_height);
            break;
         case windowconfig_MINSIZE:
            size_hints.flags |= PMinSize;
            readminsize_windowconfig(winconf_attrib, &i, &size_hints.min_width, &size_hints.min_height);
            break;
         case windowconfig_POS:
            size_hints.flags |= PPosition;
            readpos_windowconfig(winconf_attrib, &i, &size_hints.x, &size_hints.y);
            break;
         case windowconfig_SIZE:
            readsize_windowconfig(winconf_attrib, &i, &size_hints.base_width, &size_hints.base_height);
            break;
         case windowconfig_TITLE:
            title = readtitle_windowconfig(winconf_attrib, &i);
            break;
         case windowconfig_TRANSPARENCY:
            opacity = readtransparency_windowconfig(winconf_attrib, &i);
            break;
         }
      }
   }

   // allocate colormap
   flags = x11window_flags_OWNCOLORMAP;
   colormap = XCreateColormap(display, parent_win, visual, AllocNone);
   XAllocColor(display, colormap, &colwhite);

   // create window
   {
      XSetWindowAttributes attr;
      attr.background_pixmap = None;
      attr.event_mask        = ExposureMask | KeyPressMask | StructureNotifyMask;
      attr.override_redirect = ! isFrame;
      attr.colormap          = colormap;
      attr.border_pixel      = colwhite.pixel; // this ensures that non standard visuals do not generate protocol error

      win = XCreateWindow( display, parent_win, size_hints.x, size_hints.y,
                           (uint32_t)size_hints.base_width, (uint32_t)size_hints.base_height, 0, depth, InputOutput, visual,
                           CWBackPixmap | CWEventMask | CWOverrideRedirect | (flags ? (CWColormap|CWBorderPixel): 0),
                           &attr );
      flags |= x11window_flags_OWNWINDOW;
   }

   // set opacity (if less than 100%)
   setwinopacity_x11window(x11disp, win, opacity*(uint32_t)0x01010101);
   // set size and title
   Xutf8SetWMProperties(display, win, title ? title : "", 0, 0, 0, &size_hints, &wm_hints, 0);
   // closing a window only sends a request no actual destroying takes place
   Atom wm_delete_window = x11disp->atoms.WM_DELETE_WINDOW;
   XSetWMProtocols(display, win, &wm_delete_window, 1);

   err = insertobject_x11display(x11disp, x11win, win);
   if (err) goto ONERR;

   x11win->display      = x11disp;
   x11win->sys_drawable = win;
   x11win->sys_colormap = colormap;
   x11win->evhimpl      = eventhandler;
   x11win->state        = x11window_state_HIDDEN;
   x11win->flags        = flags;

   return 0;
ONERR:
   if (0 != (flags & x11window_flags_OWNWINDOW)) {
      XDestroyWindow(display, win);
   }
   if (0 != (flags & x11window_flags_OWNCOLORMAP)) {
      XFreeColormap(display, colormap);
   }
   TRACEEXIT_ERRLOG(err);
   return err;
}

int init_x11window(/*out*/x11window_t * x11win, x11display_t * x11disp, uint32_t screennr, const struct x11window_evh_t * eventhandler, const int32_t * gconf_attrib, const struct windowconfig_t * winconf_attrib)
{
   int err ;
   uint32_t maxnr = (uint32_t) ScreenCount(x11disp->sys_display);
   VisualID visualid;

   VALIDATE_INPARAM_TEST(screennr < maxnr, ONERR, );

   visualid = XVisualIDFromVisual(DefaultVisual(x11disp->sys_display, screennr));

   if (gconf_attrib) {
      err = matchvisual_x11window(&visualid, x11disp, screennr, gconf_attrib) ;
      if (err) goto ONERR;
   }

   err = initvid_x11window(x11win, x11disp, screennr, eventhandler, visualid, winconf_attrib);
   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}


int initmove_x11window(/*out*/x11window_t * dest_x11win, x11window_t * src_x11win)
{
   int err;

   err = replaceobject_x11display(src_x11win->display, dest_x11win, src_x11win->sys_drawable);
   if (err) goto ONERR;

   static_assert( sizeof(*dest_x11win) == sizeof(*src_x11win)
                  && sizeof(*src_x11win) == sizeof(x11window_t), "make memmove/memset safe");
   memmove(dest_x11win, src_x11win, sizeof(x11window_t));
   memset(src_x11win, 0, sizeof(x11window_t));

   return 0 ;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err ;
}

int free_x11window(x11window_t * x11win)
{
   int err;
   x11display_t * x11disp = x11win->display;

   if (x11disp) {
      err = 0 ;

      if (0 != (x11win->flags & x11window_flags_OWNCOLORMAP)) {
         XFreeColormap(x11disp->sys_display, x11win->sys_colormap) ;
      }

      if (0 != (x11win->flags & x11window_flags_OWNWINDOW)) {

         err = removeobject_x11display(x11disp, x11win->sys_drawable) ;

         XDestroyWindow(x11disp->sys_display, x11win->sys_drawable) ;
      }

      x11win->display      = 0 ;
      x11win->sys_drawable = 0 ;
      x11win->sys_colormap = 0 ;
      x11win->evhimpl      = 0 ;
      x11win->state        = x11window_state_DESTROYED ;
      x11win->flags        = 0 ;

      if (err) goto ONERR;
   }

   return 0 ;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err ;
}

// group: query

uint32_t screen_x11window(const x11window_t * x11win)
{
   XWindowAttributes winattr ;
   int32_t           nrscreen ;

   if (!XGetWindowAttributes(x11win->display->sys_display, x11win->sys_drawable, &winattr)) {
      TRACESYSCALL_ERRLOG("XGetWindowAttributes", EINVAL) ;
      nrscreen = 0 ;
   } else {
      nrscreen = XScreenNumberOfScreen(winattr.screen) ;
   }

   return (uint32_t) nrscreen;
}

int title_x11window(const x11window_t * x11win, /*ret*/struct cstring_t * title)
{
   int err ;
   XTextProperty  textprop  = { .value = 0, .nitems = 0 } ;
   char **        textlist  = 0 ;
   int            textcount = 0 ;

   VALIDATE_INPARAM_TEST(x11win->state != x11window_state_DESTROYED, ONERR, ) ;

   if (  0 == XGetWMName(x11win->display->sys_display, x11win->sys_drawable, &textprop)
         || Success != Xutf8TextPropertyToTextList(x11win->display->sys_display, &textprop, &textlist, &textcount)) {
      err = EINVAL ;
      goto ONERR;
   }

   err = textcount ? append_cstring(title, strlen(textlist[0]), textlist[0]) : 0 ;

   XFree(textprop.value) ;
   XFreeStringList(textlist) ;

   if (err) goto ONERR;

   return 0 ;
ONERR:
   if (textprop.value)  XFree(textprop.value) ;
   if (textlist)        XFreeStringList(textlist) ;
   TRACEEXIT_ERRLOG(err);
   return err ;
}

int geometry_x11window(const x11window_t * x11win, /*out*/int32_t * screen_x, /*out*/int32_t * screen_y, /*out*/uint32_t * width, /*out*/uint32_t * height)
{
   int err ;
   Window   root ;
   Window   windummy ;
   uint32_t udummy ;
   int32_t  idummy ;

   VALIDATE_INPARAM_TEST(x11win->state != x11window_state_DESTROYED, ONERR, ) ;


   if (! XGetGeometry(x11win->display->sys_display, x11win->sys_drawable, &root, &idummy, &idummy,
                      width ? width : &udummy, height ? height : &udummy, &udummy, &udummy)) {
      err = EINVAL ;
      goto ONERR;
   }

   if (screen_x || screen_y) {
      XTranslateCoordinates(x11win->display->sys_display, x11win->sys_drawable, root, 0, 0, screen_x ? screen_x : &idummy, screen_y ? screen_y : &idummy, &windummy) ;
   }

   return 0 ;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err ;
}

int frame_x11window(const x11window_t * x11win, /*out*/int32_t * screen_x, /*out*/int32_t * screen_y, /*out*/uint32_t * width, /*out*/uint32_t * height)
{
   int err ;
   int32_t     x, y ;
   uint32_t    w, h ;
   uint32_t *  data = 0 ;

   VALIDATE_INPARAM_TEST(x11win->state != x11window_state_DESTROYED, ONERR, ) ;

   err = geometry_x11window(x11win, &x, &y, &w, &h) ;
   if (err) goto ONERR;

   Atom           type ;
   int            format ;
   unsigned long  items ;
   unsigned long  unread_bytes ;

   if (  0 == XGetWindowProperty(x11win->display->sys_display, x11win->sys_drawable, x11win->display->atoms._NET_FRAME_EXTENTS,
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
ONERR:
   if (data) XFree(data) ;
   TRACEEXIT_ERRLOG(err);
   return err ;
}

// group: update

int show_x11window(x11window_t * x11win)
{
   int err ;

   VALIDATE_INPARAM_TEST(x11win->state != x11window_state_DESTROYED, ONERR, ) ;

   XMapRaised(x11win->display->sys_display, x11win->sys_drawable) ;

   return 0 ;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err ;
}

int hide_x11window(x11window_t * x11win)
{
   int err ;

   VALIDATE_INPARAM_TEST(x11win->state != x11window_state_DESTROYED, ONERR, ) ;

   XUnmapWindow(x11win->display->sys_display, x11win->sys_drawable) ;

   return 0 ;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err ;
}

int sendclose_x11window(x11window_t * x11win)
{
   int err ;

   VALIDATE_INPARAM_TEST(x11win->state != x11window_state_DESTROYED, ONERR, ) ;

   XEvent xevent ;
   xevent.xclient.type    = ClientMessage ;
   xevent.xclient.window  = x11win->sys_drawable ;
   xevent.xclient.message_type = x11win->display->atoms.WM_PROTOCOLS ;
   xevent.xclient.data.l[0]    = (int32_t) x11win->display->atoms.WM_DELETE_WINDOW ;
   xevent.xclient.format       = 32 ;
   XSendEvent(x11win->display->sys_display, x11win->sys_drawable, 1, 0, &xevent) ;

   return 0 ;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err ;
}

int sendredraw_x11window(x11window_t * x11win)
{
   int err ;

   VALIDATE_INPARAM_TEST(x11win->state != x11window_state_DESTROYED, ONERR, ) ;

   // the background is set to none => only Expose events are generated, background is not cleared !
   XClearArea(x11win->display->sys_display, x11win->sys_drawable, 0, 0, 0, 0, True) ;

   return 0 ;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err ;
}

int settitle_x11window(const x11window_t * x11win, /*ret*/const char * title)
{
   int err ;
   XTextProperty textprop ;
   char  *       textlist = CONST_CAST(char, title) ;

   VALIDATE_INPARAM_TEST(x11win->state != x11window_state_DESTROYED, ONERR, ) ;

   if (Success != Xutf8TextListToTextProperty(x11win->display->sys_display, &textlist, 1, XUTF8StringStyle, &textprop)) {
      err = EINVAL ;
      goto ONERR;
   }

   XSetWMName(x11win->display->sys_display, x11win->sys_drawable, &textprop) ;
   XFree(textprop.value) ;

   return 0 ;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err ;
}

int setopacity_x11window(x11window_t * x11win, double opacity)
{
   int err ;

   VALIDATE_INPARAM_TEST(x11win->state != x11window_state_DESTROYED, ONERR, ) ;
   VALIDATE_INPARAM_TEST(0 <=opacity && opacity <= 1, ONERR, ) ;

   uint32_t cardinal_opacity = (uint32_t) (opacity * UINT32_MAX) ;

   setwinopacity_x11window(x11win->display, x11win->sys_drawable, cardinal_opacity) ;

   return 0 ;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err ;
}

int setpos_x11window(x11window_t * x11win, int32_t screen_x, int32_t screen_y)
{
   int err ;

   VALIDATE_INPARAM_TEST(x11win->state != x11window_state_DESTROYED, ONERR, ) ;

   XMoveWindow(x11win->display->sys_display, x11win->sys_drawable, screen_x, screen_y) ;

   return 0 ;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err ;
}

int resize_x11window(x11window_t * x11win, uint32_t width, uint32_t height)
{
   int err ;

   VALIDATE_INPARAM_TEST(x11win->state != x11window_state_DESTROYED, ONERR, ) ;

   XResizeWindow(x11win->display->sys_display, x11win->sys_drawable, width, height) ;

   return 0 ;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err ;
}

int swapbuffer_x11window(x11window_t * x11win)
{
   int err ;

   VALIDATE_INPARAM_TEST(x11win->state != x11window_state_DESTROYED, ONERR, ) ;

   XdbeSwapInfo swapInfo ;
   swapInfo.swap_window = x11win->sys_drawable ;
   swapInfo.swap_action = XdbeUndefined ;

   if (!XdbeSwapBuffers(x11win->display->sys_display, &swapInfo, 1)) {
      err = EINVAL ;
      goto ONERR;
   }

   return 0 ;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err ;
}

// group: helper

static bool matchfirstfilter_x11window(gconfig_t * gconf, struct display_t * display, int32_t visualid, void * user)
{
   (void) gconf;
   (void) display;
   (void) visualid;
   (void) user;
   return true;
}

static bool matchtransparentalphafilter_x11window(gconfig_t * gconf, struct display_t * display, int32_t visualid, void * user)
{
   (void) gconf;
   (void) display;
   (void) user;

   x11display_t * x11disp = os_display(display);
   bool           isMatch = false;
   int            vinfo_length;
   XVisualInfo    vinfo_pattern = { .visualid = (uint32_t)visualid };
   XVisualInfo *  vinfo = XGetVisualInfo(x11disp->sys_display, VisualIDMask, &vinfo_pattern, &vinfo_length);

   if (vinfo) {
      XRenderPictFormat * format = XRenderFindVisualFormat(x11disp->sys_display, vinfo->visual);
      isMatch = (format && format->direct.alphaMask > 0);
      XFree(vinfo) ;
   }

   return isMatch;
}

int configfilter_x11window(/*out*/gconfig_filter_t * filter, const int32_t config_attributes[])
{
   for (unsigned i = 0; config_attributes[i] != gconfig_NONE; i += 2) {
      if (i >= 2*gconfig__NROF) {
         return E2BIG;
      }

      if (config_attributes[i] == gconfig_TRANSPARENT_ALPHA) {
         if (config_attributes[i+1] != 0) {
            *filter = (gconfig_filter_t) gconfig_filter_INIT(&matchtransparentalphafilter_x11window, 0);
            return 0;
         }
         break;
      }
   }

   *filter = (gconfig_filter_t) gconfig_filter_INIT(&matchfirstfilter_x11window, 0);
   return 0;
}


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

#define WINDOW_TITLE "test üöä title"

typedef struct testwindow_t         testwindow_t ;

struct testwindow_t {
   x11window_t x11win;
   int onclose;
   int ondestroy;
   int onredraw;
   int onreshape;
   int onvisible;
   bool isVisible;
   uint32_t width;
   uint32_t height;
} ;

#define testwindow_FREE { x11window_FREE, 0, 0, 0, 0, 0, 0, 0, 0 }

x11window_evh_DECLARE(testwindow_evh_t, testwindow_t) ;

static void onclose_testwindow(testwindow_t * testwin)
{
   ++ testwin->onclose ;
}

static void ondestroy_testwindow(testwindow_t * testwin)
{
   ++ testwin->ondestroy ;
}

static void onredraw_testwindow(testwindow_t * testwin)
{
   ++ testwin->onredraw;
}

static void onreshape_testwindow(testwindow_t * testwin, uint32_t width, uint32_t height)
{
   ++ testwin->onreshape;
   testwin->width  = width;
   testwin->height = height;
}

static void onvisible_testwindow(testwindow_t * testwin, bool isVisible)
{
   testwin->isVisible = isVisible;
   ++ testwin->onvisible;
}

static int test_interface(void)
{
   testwindow_t      testwin = testwindow_FREE ;
   testwindow_evh_t  evhimpl = x11window_evh_INIT(_testwindow) ;

   // TEST x11window_evh_DECLARE
   static_assert(offsetof(testwindow_evh_t, onclose)   == 0 * sizeof(void(*)(void)), "") ;
   static_assert(offsetof(testwindow_evh_t, ondestroy) == 1 * sizeof(void(*)(void)), "") ;
   static_assert(offsetof(testwindow_evh_t, onredraw)  == 2 * sizeof(void(*)(void)), "") ;
   static_assert(offsetof(testwindow_evh_t, onreshape) == 3 * sizeof(void(*)(void)), "") ;
   static_assert(offsetof(testwindow_evh_t, onvisible) == 4 * sizeof(void(*)(void)), "") ;
   static_assert(offsetof(x11window_evh_t, onclose)    == 0 * sizeof(void(*)(void)), "") ;
   static_assert(offsetof(x11window_evh_t, ondestroy)  == 1 * sizeof(void(*)(void)), "") ;
   static_assert(offsetof(x11window_evh_t, onredraw)   == 2 * sizeof(void(*)(void)), "") ;
   static_assert(offsetof(x11window_evh_t, onreshape)  == 3 * sizeof(void(*)(void)), "") ;
   static_assert(offsetof(x11window_evh_t, onvisible)  == 4 * sizeof(void(*)(void)), "") ;

   // TEST x11window_evh_INIT
   TEST(evhimpl.onclose   == &onclose_testwindow);
   TEST(evhimpl.ondestroy == &ondestroy_testwindow);
   TEST(evhimpl.onredraw  == &onredraw_testwindow);
   TEST(evhimpl.onreshape == &onreshape_testwindow);
   TEST(evhimpl.onvisible == &onvisible_testwindow);

   // TEST cast_x11windowevh (x11window_evh_DECLARE)
   TEST(cast_x11windowevh(&evhimpl, testwindow_t) == (x11window_evh_t*)&evhimpl);

   // TEST onclose_testwindow
   for (int i = 0; i < 10; ++i) {
      TEST(testwin.onclose == i) ;
      evhimpl.onclose(&testwin) ;
   }
   TEST(testwin.onclose == 10) ;

   // TEST ondestroy_testwindow
   for (int i = 0; i < 10; ++i) {
      TEST(testwin.ondestroy == i) ;
      evhimpl.ondestroy(&testwin) ;
   }
   TEST(testwin.ondestroy == 10) ;

   // TEST onredraw_testwindow
   for (int i = 0; i < 10; ++i) {
      TEST(testwin.onredraw == i) ;
      evhimpl.onredraw(&testwin) ;
   }
   TEST(testwin.onredraw == 10) ;

   // TEST onreshape_testwindow
   for (int i = 0; i < 10; ++i) {
      TEST(testwin.onreshape == i) ;
      evhimpl.onreshape(&testwin, 0, 0) ;
   }
   TEST(testwin.onreshape == 10) ;

   // TEST onvisible_testwindow
   for (int i = 0; i < 10; ++i) {
      TEST(testwin.onvisible == i);
      evhimpl.onvisible(&testwin, 0);
   }
   TEST(testwin.onvisible == 10) ;

   return 0 ;
ONERR:
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

static int test_initfree(x11display_t * x11disp)
{
   testwindow_t      testwin = testwindow_FREE ;
   testwindow_evh_t  evhimpl = x11window_evh_INIT(_testwindow);
   x11window_t *     x11win  = &testwin.x11win ;
   uint32_t          snr     = defaultscreennr_x11display(x11disp);
   xid_t             syswin ;
   x11window_t  *    object ;

   static_assert(x11window_state_DESTROYED == 0, "x11window_FREE and free_x11window sets x11window_t.state to x11window_state_DESTROYED") ;

   // TEST x11window_FREE
   TEST(x11win->display      == 0) ;
   TEST(x11win->sys_drawable == 0) ;
   TEST(x11win->sys_colormap == 0) ;
   TEST(x11win->evhimpl      == 0) ;
   TEST(x11win->state        == 0) ;
   TEST(x11win->flags        == 0) ;

   // TEST init_x11window, free_x11window
   TEST(0 == init_x11window(x11win, x11disp, snr, 0, 0, 0)) ;
   TEST(x11win->display      == x11disp) ;
   TEST(x11win->sys_drawable != 0) ;
   TEST(x11win->sys_colormap != 0) ;
   TEST(x11win->evhimpl      == 0) ;
   TEST(x11win->state        == x11window_state_HIDDEN) ;
   TEST(x11win->flags        == (x11window_flags_OWNWINDOW|x11window_flags_OWNCOLORMAP)) ;
   syswin = x11win->sys_drawable;
   object = 0 ;
   TEST(0 == tryfindobject_x11display(x11disp, &object, syswin)) ;
   TEST(x11win == object) ;
   TEST(0 == free_x11window(x11win)) ;
   TEST(x11win->display      == 0) ;
   TEST(x11win->sys_drawable == 0) ;
   TEST(x11win->sys_colormap == 0) ;
   TEST(x11win->evhimpl      == 0) ;
   TEST(x11win->state        == 0) ;
   TEST(x11win->flags        == 0) ;
   // object does no longer exist and also window handle is removed
   TEST(ESRCH == tryfindobject_x11display(x11disp, &object, syswin)) ;
   TEST(0 == free_x11window(x11win)) ;
   TEST(x11win->display      == 0) ;
   TEST(x11win->sys_drawable == 0) ;
   TEST(x11win->sys_colormap == 0) ;
   TEST(x11win->evhimpl      == 0) ;
   TEST(x11win->state        == 0) ;
   TEST(x11win->flags        == 0) ;
   WAITFOR(x11disp, 5, false) ;

   // TEST free_x11window: XDestroyWindow called outside free_x11window (or another process for example)
   TEST(0 == init_x11window(x11win, x11disp, snr, cast_x11windowevh(&evhimpl, testwindow_t), 0, 0)) ;
   TEST(x11win->display      == x11disp) ;
   TEST(x11win->sys_drawable != 0) ;
   TEST(x11win->sys_colormap != 0) ;
   TEST(x11win->evhimpl      == cast_x11windowevh(&evhimpl, testwindow_t)) ;
   TEST(x11win->state        == x11window_state_HIDDEN) ;
   TEST(x11win->flags        == (x11window_flags_OWNWINDOW|x11window_flags_OWNCOLORMAP)) ;
   syswin = x11win->sys_drawable;
   object = 0 ;
   TEST(0 != XDestroyWindow(x11disp->sys_display, x11win->sys_drawable)) ;
   TEST(0 == tryfindobject_x11display(x11disp, &object, syswin)) ;
   TEST(x11win == object) ;
   testwin.ondestroy = 0 ;
   WAITFOR(x11disp, 5, x11win->state == x11window_state_DESTROYED) ;
   TEST(testwin.ondestroy    == 1) ;
   TEST(x11win->display      == x11disp) ;
   TEST(x11win->sys_drawable == 0) ;
   TEST(x11win->sys_colormap != 0) ;
   TEST(x11win->evhimpl      == cast_x11windowevh(&evhimpl, testwindow_t)) ;
   TEST(x11win->state        == x11window_state_DESTROYED) ;
   TEST(x11win->flags        == (x11window_flags_OWNCOLORMAP)) ;
   // removed from id -> object map
   TEST(ESRCH == tryfindobject_x11display(x11disp, &object, syswin)) ;
   TEST(0 == free_x11window(x11win)) ;
   TEST(x11win->display      == 0) ;
   TEST(x11win->sys_drawable == 0) ;
   TEST(x11win->sys_colormap == 0) ;
   TEST(x11win->state        == 0) ;
   TEST(x11win->flags        == 0) ;

   // TEST initmove_x11window
   x11window_t x11win2 = x11window_FREE;
   x11window_t x11win3 = x11window_FREE;
   TEST(0 == init_x11window(&x11win3, x11disp, snr, 0, 0, 0));
   memcpy(&x11win2, &x11win3, sizeof(x11win2));
   TEST(0 == initmove_x11window(x11win, &x11win3));
   TEST(x11win3.display      == 0) ;
   TEST(x11win3.sys_drawable == 0) ;
   TEST(x11win3.sys_colormap == 0) ;
   TEST(x11win3.evhimpl      == 0) ;
   TEST(x11win3.state        == 0) ;
   TEST(x11win3.flags        == 0) ;
   object = 0;
   TEST(0 == tryfindobject_x11display(x11disp, &object, x11win2.sys_drawable)) ;
   TEST(x11win == object) ;
   TEST(x11win->display      == x11win2.display);
   TEST(x11win->sys_drawable == x11win2.sys_drawable);
   TEST(x11win->sys_colormap == x11win2.sys_colormap);
   TEST(x11win->evhimpl      == x11win2.evhimpl);
   TEST(x11win->state        == x11win2.state);
   TEST(x11win->flags        == x11win2.flags);
   TEST(0 == free_x11window(x11win)) ;

   return 0 ;
ONERR:
   free_x11window(x11win) ;
   return EINVAL ;
}

static int compare_visual(x11display_t * x11disp, uint32_t screennr, xid_t visualid, int minrgbbits, int minalphabits, bool isDouble)
{
   XVisualInfo             vinfo_pattern = { .visualid = visualid, .screen = (int32_t) screennr };
   int                     vinfo_length  = 0 ;
   XVisualInfo *           vinfo         = XGetVisualInfo(x11disp->sys_display, VisualIDMask|VisualScreenMask, &vinfo_pattern, &vinfo_length);
   XdbeScreenVisualInfo *  vinfodb       = 0 ;

   if (isDouble) {
      int nrscreen = 1 ;
      Drawable screens = RootWindow(x11disp->sys_display, screennr) ;
      vinfodb = XdbeGetVisualInfo(x11disp->sys_display, &screens, &nrscreen) ;
      TEST(vinfodb) ;
      int i ;
      for (i = 0; i < vinfodb[0].count; ++i) {
         if (vinfodb[0].visinfo[i].visual == (VisualID)visualid) {
            break ;
         }
      }
      TEST(i < vinfodb[0].count) ;
   }

   TEST(vinfo != 0) ;
   TEST(vinfo_length == 1) ;
   TEST(vinfo->visualid     == (VisualID)visualid) ;
   TEST(vinfo->bits_per_rgb >= minrgbbits) ;

   if (minalphabits) {
      XRenderPictFormat * format = XRenderFindVisualFormat(x11disp->sys_display, vinfo->visual) ;
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
ONERR:
   if (vinfodb)   XdbeFreeVisualInfo(vinfodb) ;
   if (vinfo)     XFree(vinfo) ;
   return EINVAL ;
}

static int test_query(x11display_t * x11disp, testwindow_t * testwin, testwindow_t * testwin_noframe)
{
   x11window_t *  x11win  = &testwin->x11win ;
   x11window_t *  x11win2 = &testwin_noframe->x11win ;
   uint32_t       snr     = defaultscreennr_x11display(x11disp);
   cstring_t      title   = cstring_INIT ;
   x11window_t    dummy   = x11window_FREE ;
   VisualID       visualid ;

   // TEST matchvisual_x11window
   {
      int32_t attr[] = { gconfig_TRANSPARENT_ALPHA, 1, gconfig_NONE } ;
      TEST(0 == matchvisual_x11window(&visualid, x11disp, snr, attr)) ;
      TEST(0 == compare_visual(x11disp, snr, visualid, 0, 1, 0)) ;
   }  {
      int32_t attr[] = { gconfig_BITS_ALPHA, 8, gconfig_NONE } ;
      TEST(0 == matchvisual_x11window(&visualid, x11disp, snr, attr)) ;
      TEST(0 == compare_visual(x11disp, snr, visualid, 0, 8, 0)) ;
   }  {
      int32_t attr[] = { gconfig_BITS_RED, 8,  gconfig_NONE } ;
      TEST(0 == matchvisual_x11window(&visualid, x11disp, snr, attr)) ;
      TEST(0 == compare_visual(x11disp, snr, visualid, 8, 0, 0)) ;
   }  {
      int32_t attr[] = { gconfig_BITS_GREEN, 8,  gconfig_NONE } ;
      TEST(0 == matchvisual_x11window(&visualid, x11disp, snr, attr)) ;
      TEST(0 == compare_visual(x11disp, snr, visualid, 8, 0, 0)) ;
   }  {
      int32_t attr[] = { gconfig_BITS_BLUE, 8, gconfig_NONE } ;
      TEST(0 == matchvisual_x11window(&visualid, x11disp, snr, attr)) ;
      TEST(0 == compare_visual(x11disp, snr, visualid, 8, 0, 0)) ;
   }  {
      int32_t attr[] = { gconfig_BITS_BUFFER, 24,  gconfig_NONE } ;
      TEST(0 == matchvisual_x11window(&visualid, x11disp, snr, attr)) ;
      TEST(0 == compare_visual(x11disp, snr, visualid, 8, 0, 0)) ;
   }

   // TEST display_x11window
   for (uintptr_t i = 15; i <= 15; --i) {
      dummy.display = (x11display_t*) i;
      TEST((x11display_t*)i == display_x11window(&dummy));
   }

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

   // TEST syswindow_x11window
   dummy.sys_drawable = x11win->sys_drawable;
   TEST(syswindow_x11window(&dummy) == (struct sys_window_t*)(uintptr_t)x11win->sys_drawable);
   for (uintptr_t i = 15; i <= 15; --i) {
      dummy.sys_drawable = i;
      TEST(syswindow_x11window(&dummy) == (struct sys_window_t*)i);
   }

   // TEST screen_x11window
   TEST(snr == screen_x11window(x11win));

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
   TEST(x >= 0) ;
   TEST(y >= 0) ;
   TEST(w == 200) ;
   TEST(h == 100) ;
   TEST(0 == geometry_x11window(x11win2, &x, &y, &w, &h)) ;
   TEST(x >= 0) ;
   TEST(y >= 0) ;
   TEST(w == 200) ;
   TEST(h == 100) ;

   // TEST frame_x11window
   int32_t  fx, fy ;
   uint32_t fw, fh ;
   TEST(0 == frame_x11window(x11win, &fx, &fy, &fw, &fh)) ;
   TEST(fx >= 0) ;
   TEST(fy >= 0) ;
   TEST(fw >= 200) ;
   TEST(fh >= 100) ;
   TEST(0 == frame_x11window(x11win2, &fx, &fy, &fw, &fh)) ;
   TEST(fx >= 0) ;
   TEST(fy >= 0) ;
   TEST(fw == 200) ;
   TEST(fh == 100) ;

   // TEST pos_x11window
   int32_t x2, y2 ;
   TEST(0 == geometry_x11window(x11win, &x, &y, &w, &h)) ;
   TEST(0 == pos_x11window(x11win, &x2, &y2)) ;
   TEST(x2 == x) ;
   TEST(y2 == y) ;
   TEST(0 == geometry_x11window(x11win2, &x, &y, &w, &h)) ;
   TEST(0 == pos_x11window(x11win2, &x2, &y2)) ;
   TEST(x2 == x) ;
   TEST(y2 == y) ;

   // TEST size_x11window
   w = h = 0;
   TEST(0 == size_x11window(x11win, &w, &h)) ;
   TEST(w == 200) ;
   TEST(h == 100) ;
   w = h = 0;
   TEST(0 == size_x11window(x11win2, &w, &h)) ;
   TEST(w == 200) ;
   TEST(h == 100) ;

   // TEST isfree_x11window
   TEST(0 == isfree_x11window(x11win));
   TEST(0 == isfree_x11window(x11win2));
   TEST(1 == isfree_x11window(&dummy));
   dummy.display = x11win2->display;
   TEST(0 == isfree_x11window(&dummy));
   dummy.display = 0;
   dummy.sys_drawable = x11win2->sys_drawable;
   TEST(0 == isfree_x11window(&dummy));
   dummy.sys_drawable = 0;
   TEST(1 == isfree_x11window(&dummy));

   // unprepare
   TEST(0 == free_cstring(&title)) ;
   WAITFOR(x11win->display, 1, false) ;

   return 0 ;
ONERR:
   free_cstring(&title) ;
   return EINVAL ;
}

static int test_showhide(testwindow_t * testwin)
{
   x11window_t * x11win = &testwin->x11win ;

   // precondition
   TEST(0 == show_x11window(x11win)) ;
   WAITFOR(x11win->display, 20, state_x11window(x11win) == x11window_state_SHOWN) ;
   TEST(state_x11window(x11win) == x11window_state_SHOWN) ;

   // TEST hide_x11window
   testwin->onvisible = 0;
   TEST(0 == hide_x11window(x11win)) ;
   WAITFOR(x11win->display, 20, state_x11window(x11win) == x11window_state_HIDDEN) ;
   TEST(state_x11window(x11win) == x11window_state_HIDDEN) ;
   TEST(1 == testwin->onvisible);
   TEST(0 == testwin->isVisible);

   // TEST show_x11window
   testwin->onvisible = 0;
   TEST(state_x11window(x11win) == x11window_state_HIDDEN) ;
   TEST(0 == show_x11window(x11win));
   WAITFOR(x11win->display, 20, state_x11window(x11win) == x11window_state_SHOWN);
   TEST(state_x11window(x11win) == x11window_state_SHOWN);
   TEST(1 <= testwin->onvisible);
   TEST(1 == testwin->isVisible);

   return 0 ;
ONERR:
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

   // TEST sendclose_x11window
   WAITFOR(x11win->display, 2, false) ;
   testwin->onclose = 0 ;
   TEST(0 == sendclose_x11window(x11win)) ;
   TEST(testwin->onclose == 0) ;
   WAITFOR(x11win->display, 10, testwin->onclose != 0) ;
   TEST(testwin->onclose == 1) ;

   // TEST sendredraw_x11window
   TEST(0 == show_x11window(x11win)) ;
   WAITFOR(x11win->display, 3, state_x11window(x11win) == x11window_state_SHOWN) ;
   WAITFOR(x11win->display, 3, testwin->onredraw != 0) ;
   TEST(0 == sendredraw_x11window(x11win)) ;
   testwin->onredraw = 0 ;
   WAITFOR(x11win->display, 10, testwin->onredraw != 0) ;
   TEST(testwin->onredraw >= 1) ;

   // unprepare
   TEST(0 == free_cstring(&title)) ;
   WAITFOR(x11win->display, 10, false) ;

   return 0 ;
ONERR:
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
   TEST(0 == show_x11window(x11win));
   WAITFOR(x11win->display, 20, x11win->state == x11window_state_SHOWN);
   TEST(x11win->state == x11window_state_SHOWN);

   // TEST setpos_x11window, frame_x11window, geometry_x11window, pos_x11window, size_x11window
   for (unsigned i = 0; i < 3; ++i) {
      WAITFOR(x11win->display, 1, false) ;
      int posx = (int) (150u + 10u*i);
      int posy = (int) (200u + 5u*i);
      TEST(0 == setpos_x11window(x11win, posx, posy)) ;
      for (unsigned wi = 0; wi < 10; ++wi) {
         // wait until all ConfigureNotify messages has been processed
         ((testwindow_t*)x11win)->onreshape = 0;
         WAITFOR(x11win->display, 10, ((testwindow_t*)x11win)->onreshape);
         if (wi && !((testwindow_t*)x11win)->onreshape) break;
         TEST(((testwindow_t*)x11win)->onreshape);
      }
      if (0 == ti) {
         TEST(0 == frame_x11window(x11win, &x, &y, &w, &h)) ;
         TEST(w >= 200) ;
         TEST(h >  100) ;
         TEST(0 == size_x11window(x11win, &w, &h)) ;
      } else {
         TEST(0 == geometry_x11window(x11win, &x, &y, &w, &h)) ;
      }
      TEST(x == posx) ;
      TEST(y == posy) ;
      TEST(w == 200) ;
      TEST(h == 100) ;
      TEST(0 == pos_x11window(x11win, &x, &y)) ;
      TEST(((testwindow_t*)x11win)->width  == w) ;
      TEST(((testwindow_t*)x11win)->height == h) ;
      TEST(x >= posx) ;
      TEST(y >= posy) ;
      if (ti) {
         TEST(x == posx) ;
         TEST(y == posy) ;
      }
   }
   TEST(0 == setpos_x11window(x11win, ti ? 0 : 100, ti ? 1 : 101)) ;
   ((testwindow_t*)x11win)->onreshape = 0;
   WAITFOR(x11win->display, 3, ((testwindow_t*)x11win)->onreshape);

   // TEST resize_x11window, frame_x11window, geometry_x11window, pos_x11window, size_x11window
   for (unsigned i = 2; i <= 2; --i) {
      WAITFOR(x11win->display, 1, false) ;
      uint32_t neww = 200u + 10u*i;
      uint32_t newh = 100u + 5u*i;
      TEST(0 == resize_x11window(x11win, neww, newh)) ;
      ((testwindow_t*)x11win)->onreshape = 0;
      WAITFOR(x11win->display, 20, ((testwindow_t*)x11win)->width == neww) ;
      TEST(((testwindow_t*)x11win)->onreshape) ;
      TEST(0 == size_x11window(x11win, &w, &h)) ;
      TEST(w == neww) ;
      TEST(h == newh) ;
      TEST(0 == frame_x11window(x11win, &x, &y, &w, &h)) ;
      TEST(x == (ti ? 0 : 100)) ;
      TEST(y == (ti ? 1 : 101)) ;
      TEST(w >= neww) ;
      TEST(h >= newh) ;
      TEST(0 == geometry_x11window(x11win, &x, &y, &w, &h)) ;
      TEST(w == neww) ;
      TEST(h == newh) ;
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
      TEST(((testwindow_t*)x11win)->width  == w) ;
      TEST(((testwindow_t*)x11win)->height == h) ;
   }
   TEST(0 == resize_x11window(x11win, 200, 100));
   ((testwindow_t*)x11win)->onreshape = 0;
   WAITFOR(x11win->display, 10, ((testwindow_t*)x11win)->onreshape);

   } // repeat all test for other window

   return 0 ;
ONERR:
   return EINVAL ;
}

static int test_config(x11display_t * x11disp)
{
   cstring_t   title  = cstring_INIT ;
   x11window_t x11win = x11window_FREE ;
   uint32_t    snr    = defaultscreennr_x11display(x11disp);
   XWindowAttributes winattr ;

   // TEST init_x11window: gconfig_BITS_BUFFER
   {
      int32_t config[] = { gconfig_BITS_BUFFER, 24,  gconfig_NONE } ;
      TEST(0 == init_x11window(&x11win, x11disp, snr, 0, config, 0)) ;
      TEST(1 == XGetWindowAttributes(x11disp->sys_display, x11win.sys_drawable, &winattr)) ;
      TEST(0 == compare_visual(x11disp, snr, winattr.visual->visualid, 8, 0, 0)) ;
      TEST(0 == free_x11window(&x11win)) ;
   }

   // TEST init_x11window: windowconfig_INIT_POS, windowconfig_INIT_SIZE
   {
      windowconfig_t config[] = { windowconfig_INIT_POS(300, 340), windowconfig_INIT_SIZE(123, 145), windowconfig_INIT_NONE };
      TEST(0 == init_x11window(&x11win, x11disp, snr, 0, 0, config)) ;
      TEST(1 == XGetWindowAttributes(x11disp->sys_display, x11win.sys_drawable, &winattr)) ;
      TEST(winattr.x      == 300) ;
      TEST(winattr.y      == 340) ;
      TEST(winattr.width  == 123) ;
      TEST(winattr.height == 145) ;
      TEST(0 == free_x11window(&x11win)) ;
   }

   // TEST init_x11window: gconfig_TRANSPARENT_ALPHA
   {
      int32_t config[] = { gconfig_TRANSPARENT_ALPHA, 1,  gconfig_NONE } ;
      TEST(0 == init_x11window(&x11win, x11disp, snr, 0, config, 0)) ;
      TEST(1 == XGetWindowAttributes(x11disp->sys_display, x11win.sys_drawable, &winattr)) ;
      TEST(0 == compare_visual(x11disp, snr, winattr.visual->visualid, 0, 1, 0)) ;
      TEST(0 == free_x11window(&x11win)) ;
   }

   // TEST init_x11window: windowconfig_INIT_FRAME, windowconfig_INIT_TITLE
   {
      windowconfig_t config[] = { windowconfig_INIT_FRAME, windowconfig_INIT_TITLE("1TEXT2"),
                                 windowconfig_INIT_POS(100, 110), windowconfig_INIT_SIZE(150, 185), windowconfig_INIT_NONE };
      TEST(0 == init_x11window(&x11win, x11disp, snr, 0, 0, config)) ;
      TEST(0 == show_x11window(&x11win)) ;
      WAITFOR(x11win.display, 10, x11win.state == x11window_state_SHOWN) ;
      TEST(x11win.state == x11window_state_SHOWN) ;
      TEST(1 == XGetWindowAttributes(x11disp->sys_display, x11win.sys_drawable, &winattr)) ;
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

   // TEST init_x11window: windowconfig_INIT_MINSIZE, windowconfig_INIT_MAXSIZE
   {
      windowconfig_t config[] = { windowconfig_INIT_FRAME, windowconfig_INIT_MINSIZE(190, 191),
                                 windowconfig_INIT_MAXSIZE(210, 211), windowconfig_INIT_SIZE(200, 201), windowconfig_INIT_NONE };
      TEST(0 == init_x11window(&x11win, x11disp, snr, 0, 0, config)) ;
      TEST(0 == show_x11window(&x11win)) ;
      WAITFOR(x11win.display, 20, x11win.state == x11window_state_SHOWN) ;
      TEST(x11win.state == x11window_state_SHOWN) ;
      TEST(1 == XGetWindowAttributes(x11disp->sys_display, x11win.sys_drawable, &winattr)) ;
      TEST(winattr.width  == 200) ;
      TEST(winattr.height == 201) ;
      TEST(0 == resize_x11window(&x11win, 300, 300)) ; // resize will be clipped by WM
      for (int i = 0; i < 10; ++i) {
         WAITFOR(x11win.display, 1, false) ;
         TEST(1 == XGetWindowAttributes(x11disp->sys_display, x11win.sys_drawable, &winattr)) ;
         if (winattr.width != 200) break ;
      }
      TEST(winattr.width  == 210) ; // MAXSIZE
      TEST(winattr.height == 211) ; // MAXSIZE
      TEST(0 == resize_x11window(&x11win, 100, 100)) ; // resize will be clipped by WM
      for (int i = 0; i < 10; ++i) {
         WAITFOR(x11win.display, 1, false) ;
         TEST(1 == XGetWindowAttributes(x11disp->sys_display, x11win.sys_drawable, &winattr)) ;
         if (winattr.width != 210) break ;
      }
      TEST(winattr.width  == 190) ; // MINSIZE
      TEST(winattr.height == 191) ; // MINSIZE
      TEST(0 == free_x11window(&x11win)) ;
   }

   // unprepare
   TEST(0 == free_cstring(&title)) ;
   WAITFOR(x11disp, 1, false) ;

   return 0 ;
ONERR:
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
      Window root = RootWindow(x11win->display->sys_display, (int32_t) screen_x11window(x11win)) ;
      Window windummy ;
      int32_t x2, y2 ;
      XTranslateCoordinates(x11win->display->sys_display, x11win->sys_drawable, root, 0, 0, &x2, &y2, &windummy) ;
      ximg = XGetImage(x11win->display->sys_display, root, x2, y2, w, h, (unsigned long)-1, ZPixmap) ;
   } else {
      ximg = XGetImage(x11win->display->sys_display, x11win->sys_drawable, 0, 0, w, h, (unsigned long)-1, ZPixmap) ;
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
   gc1 = XCreateGC(x11win1->display->sys_display, x11win1->sys_drawable, GCForeground, &gcvalues) ;
   TEST(gc1) ;
   gcvalues.foreground = colblue.pixel ;
   gc2 = XCreateGC(x11win2->display->sys_display, x11win2->sys_drawable, GCForeground, &gcvalues) ;
   TEST(gc2) ;
   TEST(0 == show_x11window(x11win1)) ;
   TEST(0 == hide_x11window(x11win2)) ;
   WAITFOR(x11win1->display, 10, x11win1->state == x11window_state_SHOWN) ;
   WAITFOR(x11win2->display, 10, x11win2->state == x11window_state_HIDDEN) ;
   int32_t x, y ;
   TEST(0 == pos_x11window(x11win1, &x, &y)) ;
   TEST(0 == setpos_x11window(x11win2, x, y)) ;

   // TEST gconfig_TRANSPARENT_ALPHA
   // draw background window red
   TEST(1 == XFillRectangle(x11win1->display->sys_display, x11win1->sys_drawable, gc1, 0, 0, 200, 100)) ;
   WAITFOR(x11win1->display, 1, false) ;
   TEST(0 == compare_color(x11win1, false, 200, 100, 1, 0, 0)) ;
   // draw overlay window blue
   TEST(0 == show_x11window(x11win2)) ;
   WAITFOR(x11win2->display, 10, x11win2->state == x11window_state_SHOWN) ;
   TEST(1 == XFillRectangle(x11win2->display->sys_display, x11win2->sys_drawable, gc2, 0, 0, 200, 100)) ;
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
   TEST(1 == XFillRectangle(x11win2->display->sys_display, x11win2->sys_drawable, gc2, 0, 0, 200, 100)) ;
   WAITFOR(x11win2->display, 1, false) ;
   TEST(0 == compare_color(x11win2, false, 200, 100, 0, 0, 1)) ;
   for (int i = 0; i< 20; ++i) {
      WAITFOR(x11win2->display, 1, false) ;
      if (0 == compare_color(x11win2, true, 200, 100, 1, 0, 1)) break ;
   }
   TEST(0 == compare_color(x11win2, true, 200, 100, 1, 0, 1)) ; // both colors visible

   // TEST setopacity_x11window
   XSetForeground(x11win2->display->sys_display, gc2, colblue.pixel /*fully opaque*/) ;
   TEST(1 == XFillRectangle(x11win2->display->sys_display, x11win2->sys_drawable, gc2, 0, 0, 200, 100)) ;
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
ONERR:
   return EINVAL ;
}

static int test_configfilter(x11display_t * x11disp)
{
   // prepare
   int            vinfo_length;
   XVisualInfo    vinfo_pattern;
   XVisualInfo *  vinfo    = XGetVisualInfo(x11disp->sys_display, VisualNoMask, &vinfo_pattern, &vinfo_length);
   gconfig_filter_t filter = gconfig_filter_FREE;
   int            config_attributes[2*gconfig__NROF+2];

   // TEST configfilter_x11window: E2BIG
   for (unsigned i = 0; i < lengthof(config_attributes)-1; i += 2) {
      config_attributes[i]   = gconfig_BITS_RED;
      config_attributes[i+1] = 1;
   }
   TEST(E2BIG == configfilter_x11window(&filter, config_attributes));
   TEST(0 == filter.user);
   TEST(0 == filter.accept);

   // TEST configfilter_x11window: TRANSPARENT_ALPHA == 0
   config_attributes[0] = gconfig_TRANSPARENT_ALPHA;
   config_attributes[1] = 0;
   config_attributes[2] = gconfig_NONE;
   TEST(0 == configfilter_x11window(&filter, config_attributes));
   TEST(filter.user   == 0);
   TEST(filter.accept == &matchfirstfilter_x11window);

   // TEST configfilter_x11window: TRANSPARENT_ALPHA == 0
   config_attributes[0] = gconfig_TRANSPARENT_ALPHA;
   config_attributes[1] = 1;
   config_attributes[2] = gconfig_NONE;
   TEST(0 == configfilter_x11window(&filter, config_attributes));
   TEST(filter.user   == 0);
   TEST(filter.accept == &matchtransparentalphafilter_x11window);

   // TEST matchfirstfilter_x11window
   for (int i = 0; i < vinfo_length; ++i) {
      TEST(true == matchfirstfilter_x11window(0, 0, (int32_t) vinfo[i].visualid, 0));
   }

   // TEST matchtransparentalphafilter_x11window
   for (int i = 0; i < vinfo_length; ++i) {
      display_t           disp   = { .osdisplay = *x11disp };
      XRenderPictFormat * format = XRenderFindVisualFormat(x11disp->sys_display, vinfo[i].visual);
      TEST(0 != format);
      bool isMatch = (format->direct.alphaMask > 0);
      TEST(isMatch == matchtransparentalphafilter_x11window(0, &disp, (int32_t) vinfo[i].visualid, 0));
   }

   // unprepare
   XFree(vinfo);

   return 0 ;
ONERR:
   if (vinfo) XFree(vinfo);
   return EINVAL ;
}

static int childprocess_unittest(void)
{
   x11display_t      x11disp   = x11display_FREE ;
   uint32_t          snr       = 0;
   testwindow_t      x11win    = testwindow_FREE ;
   testwindow_t      x11win2   = testwindow_FREE ;
   testwindow_evh_t  evhimpl   = x11window_evh_INIT(_testwindow) ;
   int32_t           gconf[]   = { gconfig_BITS_RED, 8, gconfig_NONE };
   windowconfig_t    config[]  = {  windowconfig_INIT_FRAME,
                                    windowconfig_INIT_TITLE(WINDOW_TITLE),
                                    windowconfig_INIT_SIZE(200, 100),
                                    windowconfig_INIT_NONE
                                 };
   int32_t           gconf2[]  = { gconfig_BITS_RED, 8, gconfig_TRANSPARENT_ALPHA, 1, gconfig_NONE };
   windowconfig_t    config2[] = {  windowconfig_INIT_SIZE(200, 100),
                                    windowconfig_INIT_NONE
                                 };
   resourceusage_t   usage     = resourceusage_FREE ;

   // prepare
   TEST(0 == init_x11display(&x11disp, 0)) ;
   snr = defaultscreennr_x11display(&x11disp) ;
   // with frame
   TEST(0 == init_x11window(&x11win.x11win, &x11disp, snr, cast_x11windowevh(&evhimpl, testwindow_t), gconf, config)) ;
   // without frame
   TEST(0 == init_x11window(&x11win2.x11win, &x11disp, snr, cast_x11windowevh(&evhimpl, testwindow_t), gconf2, config2)) ;

   if (test_initfree(&x11disp))           goto ONERR;
   if (test_query(&x11disp, &x11win,
                  &x11win2))              goto ONERR;
   if (test_showhide(&x11win))            goto ONERR;
   if (test_geometry(&x11win,&x11win2))   goto ONERR;
   if (test_config(&x11disp))             goto ONERR;
   if (test_opacity(&x11win, &x11win2))   goto ONERR;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_interface())                  goto ONERR;
   if (test_initfree(&x11disp))           goto ONERR;
   if (test_query(&x11disp, &x11win,
                  &x11win2))              goto ONERR;
   if (test_geometry(&x11win,&x11win2))   goto ONERR;
   if (test_config(&x11disp))             goto ONERR;
   if (test_opacity(&x11win, &x11win2))   goto ONERR;
   if (test_configfilter(&x11disp))       goto ONERR;

   TEST(0 == same_resourceusage(&usage));
   TEST(0 == free_resourceusage(&usage));

   if (test_update(&x11win))              goto ONERR;

   // unprepare
   TEST(0 == free_x11window(&x11win.x11win)) ;
   TEST(0 == free_x11window(&x11win2.x11win)) ;
   TEST(0 == free_x11display(&x11disp)) ;

   return 0 ;
ONERR:
   (void) free_x11window(&x11win.x11win) ;
   (void) free_x11window(&x11win2.x11win) ;
   (void) free_x11display(&x11disp) ;
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

int unittest_platform_X11_x11window()
{
   int err;

   TEST(0 == execasprocess_unittest(&childprocess_unittest, &err));

   return err;
ONERR:
   return EINVAL;
}

#endif
