/* title: X11-OpenGL-Window impl

   Implements window support on a X11 display to show OpenGL output.

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

   file: C-kern/api/platform/X11/glxwindow.h
    Header file of <X11-OpenGL-Window>.

   file: C-kern/platform/X11/glxwindow.c
    Implementation file <X11-OpenGL-Window impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/platform/X11/glxwindow.h"
#include "C-kern/api/err.h"
#include "C-kern/api/platform/X11/x11.h"
#include "C-kern/api/platform/X11/x11attribute.h"
#include "C-kern/api/platform/X11/x11display.h"
#include "C-kern/api/platform/X11/x11drawable.h"
#include "C-kern/api/platform/X11/x11screen.h"
#include "C-kern/api/platform/X11/x11window.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/resourceusage.h"
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/platform/task/thread.h"
#include "C-kern/api/string/cstring.h"
#endif
#include STR(C-kern/api/platform/KONFIG_OS/graphic/sysx11.h)
#include STR(C-kern/api/platform/KONFIG_OS/graphic/sysglx.h)



// section: glxwindow_t

// group: helper

static inline void compiletime_assert(void)
{
   static_assert( ((x11drawable_t*)0) == genericcast_x11drawable((glxwindow_t*)0),
                  "glxwindow_t can be cast to x11drawable_t");
   static_assert( ((x11window_t*)0) == genericcast_x11window((glxwindow_t*)0),
                  "glxwindow_t can be cast to x11window_t");
}

static int matchfbconfig_glxwindow(const x11screen_t * x11screen, /*out*/GLXFBConfig * matching_fbconfig, uint8_t nrofattributes, const x11attribute_t * configuration/*[nrofattributes]*/)
{
   int attrib_list[30] = {
                           GLX_RENDER_TYPE,     GLX_RGBA_BIT,
                           GLX_DRAWABLE_TYPE,   GLX_WINDOW_BIT | GLX_PBUFFER_BIT,
                           None
                         } ;
   int  next_index         = 4 ;
   int  index_DOUBLEBUFFER = 0 ;
   int  index_REDBITS      = 0 ;
   int  index_GREENBITS    = 0 ;
   int  index_BLUEBITS     = 0 ;
   int  index_ALPHABITS    = 0 ;
   int  index_DEPTHBITS    = 0 ;
   int  index_STENCILBITS  = 0 ;
   int  index_ACCUM_REDBITS = 0 ;
   int  index_ACCUM_BLUEBITS = 0 ;
   int  index_ACCUM_GREENBITS = 0 ;
   int  index_ACCUM_ALPHABITS = 0 ;
   bool isAlphaOpacity = false ;

   for (int i = 0; i < nrofattributes; ++i) {
      switch (configuration[i].name) {
#define CASE(x11attrname, glxattrname, valmember)                                         \
      case x11attribute_##x11attrname:                                                    \
         if (configuration[i].value.valmember > INT32_MAX) goto ONABORT ;                 \
         if (! index_##x11attrname) {                                                     \
            index_##x11attrname = next_index ;                                            \
            next_index += 2 ;                                                             \
            assert(next_index < (int)lengthof(attrib_list)) ;                             \
            attrib_list[next_index] = None ;                                              \
         }                                                                                \
         attrib_list[index_##x11attrname]     = glxattrname ;                             \
         attrib_list[index_##x11attrname + 1] = (int)configuration[i].value.valmember ;   \
         break
      CASE(DOUBLEBUFFER,    GLX_DOUBLEBUFFER,   isOn) ;
      CASE(REDBITS,         GLX_RED_SIZE,       u32) ;
      CASE(GREENBITS,       GLX_GREEN_SIZE,     u32) ;
      CASE(BLUEBITS,        GLX_BLUE_SIZE,      u32) ;
      CASE(ALPHABITS,       GLX_ALPHA_SIZE,     u32) ;
      CASE(DEPTHBITS,       GLX_DEPTH_SIZE,     u32) ;
      CASE(STENCILBITS,     GLX_STENCIL_SIZE,   u32) ;
      CASE(ACCUM_REDBITS,   GLX_ACCUM_RED_SIZE, u32) ;
      CASE(ACCUM_GREENBITS, GLX_ACCUM_GREEN_SIZE,  u32) ;
      CASE(ACCUM_BLUEBITS,  GLX_ACCUM_BLUE_SIZE,   u32) ;
      CASE(ACCUM_ALPHABITS, GLX_ACCUM_ALPHA_SIZE,  u32) ;
#undef CASE
      case x11attribute_ALPHAOPACITY:
         isAlphaOpacity = configuration[i].value.isOn ;
         break ;
      }
   }

   int            fbconfig_count = 0 ;
   GLXFBConfig *  fbconfigs      = glXChooseFBConfig(x11screen->display->sys_display, x11screen->nrscreen, attrib_list, &fbconfig_count) ;
   bool           isMatch        = false ;

   if (  fbconfigs
         && (!isAlphaOpacity || x11screen->display->xrender.isSupported)) {
      if (isAlphaOpacity) {
         for (int i = 0; i < fbconfig_count; ++i) {
            XVisualInfo * vinfo = glXGetVisualFromFBConfig(x11screen->display->sys_display, fbconfigs[i]) ;
            if (vinfo) {
               XRenderPictFormat * format = XRenderFindVisualFormat(x11screen->display->sys_display, vinfo->visual) ;
               XFree(vinfo) ;
               vinfo = 0 ;

               if (format && format->direct.alphaMask > 0) {
                  isMatch = true ;
                  *matching_fbconfig = fbconfigs[i] ;
                  break;
               }
            }
         }

      } else if (fbconfig_count) {
         isMatch = true ;
         *matching_fbconfig = fbconfigs[0] ;
      }

      XFree(fbconfigs) ;
   }

   if (! isMatch) goto ONABORT ;

   return 0 ;
ONABORT:
   return ESRCH ;
}

static int matchvisual_glxwindow(
      const x11screen_t *           x11screen,
      /*out*/Visual **              x11_visual,
      /*out*/int *                  x11_depth,
      uint8_t                       nrofattributes,
      const struct x11attribute_t * configuration/*[nrofattributes]*/)
{
   int err ;
   GLXFBConfig fbconfig ;
   XVisualInfo  * vinfo ;

   err = matchfbconfig_glxwindow(x11screen, &fbconfig, nrofattributes, configuration) ;
   if (err) goto ONABORT ;

   vinfo = glXGetVisualFromFBConfig(x11screen->display->sys_display, fbconfig) ;
   if (!vinfo) goto ONABORT ;

   *x11_visual = vinfo->visual ;
   *x11_depth  = vinfo->depth ;
   XFree(vinfo) ;

   return 0 ;
ONABORT:
   return ESRCH ;
}

// group: lifetime

int init_glxwindow(/*out*/glxwindow_t * glxwin, struct x11screen_t * x11screen, const struct x11window_it * eventhandler, uint8_t nrofattributes, const struct x11attribute_t * configuration/*[nrofattributes]*/)
{
   int err ;
   Visual * visual ;
   int      depth ;

   err = matchvisual_glxwindow(x11screen, &visual, &depth, nrofattributes, configuration) ;
   if (err) goto ONABORT ;

   err = initsys_x11window((x11window_t*)glxwin, eventhandler, x11screen->display, RootWindow(x11screen->display->sys_display, x11screen->nrscreen), visual, depth, nrofattributes, configuration) ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

int free_glxwindow(glxwindow_t * glxwin)
{
   int err, err2 ;
   x11display_t * x11disp = glxwin->display ;

   if (x11disp) {
      err = 0 ;

      // add other free calls here

      err2 = free_x11window((x11window_t*)glxwin) ;
      if (err2) err = err2 ;

      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORTFREE_ERRLOG(err) ;
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

typedef struct testwindow_t         testwindow_t ;

struct testwindow_t {
   glxwindow_t glxwin;
   int onclose;
   int ondestroy;
   int onredraw;
   int onreshape;
   int onvisible;
   uint32_t width;
   uint32_t height;
} ;

#define testwindow_INIT_FREEABLE    { glxwindow_INIT_FREEABLE, 0, 0, 0, 0, 0, 0, 0 }

x11window_it_DECLARE(testwindow_it, testwindow_t) ;

static void onclose_testwindow(testwindow_t * testwin)
{
   ++ testwin->onclose ;
}

static void ondestroy_testwindow(testwindow_t * testwin)
{
   ++ testwin->ondestroy;
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

static void onvisible_testwindow(testwindow_t * testwin)
{
   ++ testwin->onvisible ;
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

typedef float rgbacolor_t[4];

static void draw_background(glxwindow_t * glxwin, rgbacolor_t * color, uint8_t nrofattributes, const x11attribute_t * configuration/*[nrofattributes]*/)
{
   Display        * display  = (Display *) glxwin->display->sys_display ;
   GLXContext     glxcontext = 0 ;
   x11screen_t    x11screen  = screen_glxwindow(glxwin) ;
   GLXFBConfig    fbconfig ;

   if (0 == matchfbconfig_glxwindow(&x11screen, &fbconfig, nrofattributes, configuration)) {
      glXWaitX() ;
      glxcontext = glXCreateNewContext(display, fbconfig, GLX_RGBA_TYPE, 0, 1) ;
      glXMakeCurrent(display, glxwin->sys_drawable, glxcontext) ;
      glClearColor((*color)[0],(*color)[1], (*color)[2], (*color)[3]) ;
      glClear(GL_COLOR_BUFFER_BIT) ;
      glXSwapBuffers(display, glxwin->sys_drawable) ;
      glXMakeCurrent(display, 0, 0) ;
      glXDestroyContext(display, glxcontext) ;
      glXWaitGL() ;
      glXWaitX() ;
   }
}

static int test_initfree(x11screen_t * x11screen)
{
   testwindow_t   testwin  = testwindow_INIT_FREEABLE ;
   testwindow_it  iimpl    = x11window_it_INIT(_testwindow) ;
   glxwindow_t *  glxwin   = &testwin.glxwin ;
   x11attribute_t config[] = {   x11attribute_INIT_WINFRAME,
                                 x11attribute_INIT_WINTITLE("unittest: glxwindow_t"),
                                 x11attribute_INIT_WINSIZE(400, 100),
                              } ;
   x11display_t * x11disp = display_x11screen(x11screen) ;
   uint32_t       syswin  = 0 ;
   void *         object  = 0 ;

   static_assert(x11window_Destroyed == 0, "free_glxwindow sets glxwin->state to x11window_Destroyed") ;

   // TEST glxwindow_INIT_FREEABLE
   TEST(glxwin->display      == 0) ;
   TEST(glxwin->sys_drawable == 0) ;
   TEST(glxwin->sys_colormap == 0) ;
   TEST(glxwin->iimpl        == 0) ;
   TEST(glxwin->state        == 0) ;
   TEST(glxwin->flags        == 0) ;

   // TEST init_glxwindow, free_glxwindow
   TEST(0 == init_glxwindow(glxwin, x11screen, 0, lengthof(config), config)) ;
   TEST(glxwin->display      == x11disp) ;
   TEST(glxwin->sys_drawable != 0) ;
   TEST(glxwin->sys_colormap != 0) ;
   TEST(glxwin->iimpl        == 0) ;
   TEST(glxwin->flags        == (x11window_OwnColormap|x11window_OwnWindow)) ;
   TEST(glxwin->state        == x11window_Hidden) ;
   syswin = glxwin->sys_drawable;
   object = 0 ;
   TEST(0 == findobject_x11display(x11disp, &object, syswin)) ;
   TEST(glxwin == object) ;
   TEST(0 == free_glxwindow(glxwin)) ;
   TEST(glxwin->display      == 0) ;
   TEST(glxwin->sys_drawable == 0) ;
   TEST(glxwin->sys_colormap == 0) ;
   TEST(glxwin->iimpl        == 0) ;
   TEST(glxwin->flags        == 0) ;
   TEST(glxwin->state        == 0) ;
   TEST(ESRCH == findobject_x11display(x11disp, &object, syswin)) ;
   TEST(0 == free_glxwindow(glxwin)) ;
   TEST(glxwin->display      == 0) ;
   TEST(glxwin->sys_drawable == 0) ;
   TEST(glxwin->sys_colormap == 0) ;
   TEST(glxwin->iimpl        == 0) ;
   TEST(glxwin->flags        == 0) ;
   TEST(glxwin->state        == 0) ;

   // TEST free_glxwindow: XDestroyWindow called from outside free_glxwindow
   TEST(0 == init_glxwindow(glxwin, x11screen, genericcast_x11windowit(&iimpl, testwindow_t), lengthof(config), config)) ;
   TEST(glxwin->display      == x11disp) ;
   TEST(glxwin->sys_drawable != 0) ;
   TEST(glxwin->sys_colormap != 0) ;
   TEST(glxwin->iimpl        == genericcast_x11windowit(&iimpl, testwindow_t)) ;
   TEST(glxwin->flags        == (x11window_OwnColormap|x11window_OwnWindow)) ;
   TEST(glxwin->state        == x11window_Hidden) ;
   // destroy on behalf of user
   XDestroyWindow(x11disp->sys_display, glxwin->sys_drawable) ;
   syswin = glxwin->sys_drawable ;
   TEST(0 == findobject_x11display(x11disp, &object, syswin)) ;
   TEST(glxwin == object) ;
   testwin.ondestroy = 0 ;
   WAITFOR(x11disp, 10, glxwin->state == x11window_Destroyed) ;
   TEST(testwin.ondestroy    == 1) ;
   TEST(glxwin->display      == x11disp) ;
   TEST(glxwin->sys_drawable == 0) ;
   TEST(glxwin->sys_colormap != 0) ;
   TEST(glxwin->iimpl        == genericcast_x11windowit(&iimpl, testwindow_t)) ;
   TEST(glxwin->flags        == (x11window_OwnColormap)) ;
   TEST(glxwin->state        == x11window_Destroyed) ;
   TEST(ESRCH == findobject_x11display(x11disp, &object, syswin)) ;
   TEST(0 == free_glxwindow(glxwin)) ;
   TEST(glxwin->display      == 0) ;
   TEST(glxwin->sys_drawable == 0) ;
   TEST(glxwin->sys_colormap == 0) ;
   TEST(glxwin->iimpl        == 0) ;
   TEST(glxwin->flags        == 0) ;
   TEST(glxwin->state        == 0) ;

   // TEST sendclose_glxwindow
   TEST(0 == init_glxwindow(glxwin, x11screen, genericcast_x11windowit(&iimpl, testwindow_t), lengthof(config), config)) ;
   TEST(glxwin->display      == x11disp) ;
   TEST(glxwin->sys_drawable != 0) ;
   TEST(glxwin->sys_colormap != 0) ;
   TEST(glxwin->iimpl        == genericcast_x11windowit(&iimpl, testwindow_t)) ;
   TEST(glxwin->flags        == (x11window_OwnColormap|x11window_OwnWindow)) ;
   TEST(glxwin->state        == x11window_Hidden) ;
   TEST(0 == sendclose_glxwindow(glxwin)) ;
   testwin.onclose = 0 ;
   WAITFOR(x11disp, 10, testwin.onclose) ;
   TEST(testwin.onclose == 1) ;
   TEST(glxwin->display      == x11disp) ;
   TEST(glxwin->sys_drawable != 0) ;
   TEST(glxwin->sys_colormap != 0) ;
   TEST(glxwin->iimpl        == genericcast_x11windowit(&iimpl, testwindow_t)) ;
   TEST(glxwin->flags        == (x11window_OwnColormap|x11window_OwnWindow)) ;
   TEST(glxwin->state        == x11window_Hidden) ;
   syswin = glxwin->sys_drawable ;
   TEST(0 == findobject_x11display(x11disp, &object, syswin)) ;
   TEST(glxwin == object) ;
   TEST(0 == free_glxwindow(glxwin)) ;
   TEST(glxwin->display      == 0) ;
   TEST(glxwin->sys_drawable == 0) ;
   TEST(glxwin->sys_colormap == 0) ;
   TEST(glxwin->iimpl        == 0) ;
   TEST(glxwin->flags        == 0) ;
   TEST(glxwin->state        == 0) ;
   TEST(ESRCH == findobject_x11display(x11disp, &object, syswin)) ;
   WAITFOR(x11disp, 2, false) ;

   return 0;
ONABORT:
   free_glxwindow(glxwin) ;
   return EINVAL ;
}

static int test_query(x11screen_t * x11screen, glxwindow_t * glxwin)
{
   x11display_t * x11disp = glxwin->display ;
   cstring_t      title   = cstring_INIT ;
   int32_t        x, y ;
   int32_t        x2, y2 ;
   uint32_t       w, h ;
   uint32_t       w2, h2 ;

   // TEST flags_glxwindow
   for (int i = 15; i >= 0; --i) {
      glxwindow_t dummy ;
      dummy.flags = (uint8_t)i ;
      TEST(flags_glxwindow(&dummy) == (uint8_t)i) ;
   }

   // TEST state_glxwindow
   for (int i = 15; i >= 0; --i) {
      glxwindow_t dummy ;
      dummy.state = (uint8_t)i ;
      TEST(state_glxwindow(&dummy) == (uint8_t)i) ;
   }

   // TEST screen_glxwindow
   x11screen_t x11screen2 = screen_glxwindow(glxwin) ;
   TEST(display_x11screen(x11screen) == display_x11screen(&x11screen2)) ;
   TEST(number_x11screen(x11screen)  == number_x11screen(&x11screen2)) ;

   // TEST title_glxwindow
   TEST(0 == title_glxwindow(glxwin, &title)) ;
   TEST(0 == strcmp(str_cstring(&title), "unittest: glxwindow_t")) ;
   clear_cstring(&title) ;

   // TEST frame_glxwindow
   TEST(0 == show_glxwindow(glxwin)) ;
   WAITFOR(x11disp, 10, x11window_Shown == glxwin->state) ;
   TEST(0 == frame_glxwindow(glxwin, &x, &y, &w, &h)) ;
   TEST(100 == x) ;
   TEST(102 == y) ;
   TEST(400 <= w) ;
   TEST(200 <= h) ;

   // TEST geometry_glxwindow
   TEST(0 == geometry_glxwindow(glxwin, &x, &y, &w, &h)) ;
   TEST(100 <= x) ;
   TEST(102 <= y) ;
   TEST(400 == w) ;
   TEST(200 == h) ;

   // TEST pos_glxwindow
   TEST(0 == show_glxwindow(glxwin)) ;
   WAITFOR(x11disp, 10, x11window_Shown == glxwin->state) ;
   TEST(0 == pos_glxwindow(glxwin, &x2, &y2)) ;
   TEST(x == x2) ;
   TEST(y == y2) ;

   // TEST size_glxwindow
   TEST(0 == show_glxwindow(glxwin)) ;
   WAITFOR(x11disp, 10, x11window_Shown == glxwin->state) ;
   TEST(0 == size_glxwindow(glxwin, &w2, &h2)) ;
   TEST(w == w2) ;
   TEST(h == h2) ;

   // unprepare
   TEST(0 == free_cstring(&title)) ;
   WAITFOR(x11disp, 2, false) ;

   return 0;
ONABORT:
   free_cstring(&title) ;
   return EINVAL ;
}

static int test_change(testwindow_t * testwin)
{
   glxwindow_t  * glxwin  = &testwin->glxwin ;
   x11display_t * x11disp = glxwin->display ;
   cstring_t      title   = cstring_INIT ;
   int32_t        x, y ;
   uint32_t       w, h ;

   // TEST show_glxwindow
   TEST(0 == show_glxwindow(glxwin)) ;
   WAITFOR(x11disp, 10, x11window_Shown == glxwin->state) ;
   TEST(x11window_Shown == glxwin->state) ;

   // TEST hide_glxwindow
   TEST(0 == hide_glxwindow(glxwin)) ;
   WAITFOR(x11disp, 10, x11window_Hidden == glxwin->state) ;
   TEST(x11window_Hidden == glxwin->state) ;

   // TEST setpos_glxwindow
   TEST(0 == show_glxwindow(glxwin)) ;
   WAITFOR(x11disp, 10, glxwin->state == x11window_Shown) ;
   TEST(glxwin->state == x11window_Shown) ;
   TEST(0 == setpos_glxwindow(glxwin, 200, 202)) ;
   for (int i = 0; i < 10; ++i) {
      TEST(0 == frame_glxwindow(glxwin, &x, &y, &w, &h)) ;
      if (x == 200 && y == 202) break ;
      WAITFOR(x11disp, 1, false) ;
   }
   TEST(x == 200) ;
   TEST(y == 202) ;
   TEST(w >= 400) ;
   TEST(h >= 200) ;
   TEST(0 == setpos_glxwindow(glxwin, 100, 102)) ;
   for (int i = 0; i < 10; ++i) {
      TEST(0 == frame_glxwindow(glxwin, &x, &y, &w, &h)) ;
      if (x == 100 && y == 102) break ;
      WAITFOR(x11disp, 1, false) ;
   }
   TEST(x == 100) ;
   TEST(y == 102) ;
   TEST(w >= 400) ;
   TEST(h >= 200) ;

   // TEST resize_glxwindow
   TEST(0 == resize_glxwindow(glxwin, 300, 300)) ;
   for (int i = 0; i < 10; ++i) {
      TEST(0 == size_glxwindow(glxwin, &w, &h)) ;
      if (w == 300 && h == 300) break ;
      WAITFOR(x11disp, 1, false) ;
   }
   TEST(w == 300) ;
   TEST(h == 300) ;
   TEST(0 == resize_glxwindow(glxwin, 400, 200)) ;
   for (int i = 0; i < 10; ++i) {
      TEST(0 == size_glxwindow(glxwin, &w, &h)) ;
      if (w == 400 && h == 200) break ;
      WAITFOR(x11disp, 1, false) ;
   }
   TEST(w == 400) ;
   TEST(h == 200) ;

   // TEST sendredraw_glxwindow
   testwin->onredraw = 0 ;
   WAITFOR(x11disp, 2, testwin->onredraw != 0) ;
   testwin->onredraw = 0 ;
   TEST(0 == sendredraw_glxwindow(glxwin)) ;
   WAITFOR(x11disp, 2, testwin->onredraw > 0) ;
   TEST(testwin->onredraw > 0) ;

   // TEST settitle_glxwindow
   TEST(0 == settitle_glxwindow(glxwin, "test: glxwindow_t")) ;
   TEST(0 == title_glxwindow(glxwin, &title)) ;
   TEST(0 == strcmp(str_cstring(&title), "test: glxwindow_t")) ;
   clear_cstring(&title) ;

   // unprepare
   TEST(0 == free_cstring(&title)) ;

   return 0 ;
ONABORT:
   free_cstring(&title) ;
   return EINVAL ;
}

static int compare_color(glxwindow_t * glxwin, bool isRoot, uint32_t w, uint32_t h, bool isRed, bool isGreen, bool isBlue)
{
   XImage * ximg = 0 ;
   size_t   pixels ;
   uint32_t x, y ;

   if (isRoot) {
      Window root = RootWindow(glxwin->display->sys_display, screen_glxwindow(glxwin).nrscreen) ;
      Window windummy ;
      int32_t x2, y2 ;
      XTranslateCoordinates(glxwin->display->sys_display, glxwin->sys_drawable, root, 0, 0, &x2, &y2, &windummy) ;
      ximg = XGetImage(glxwin->display->sys_display, root, x2, y2, w, h, (unsigned long)-1, ZPixmap) ;
   } else {
      ximg = XGetImage(glxwin->display->sys_display, glxwin->sys_drawable, 0, 0, w, h, (unsigned long)-1, ZPixmap) ;
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

static int test_transparency(testwindow_t * testwin)
{
   glxwindow_t * glxwin    = &testwin->glxwin ;
   glxwindow_t   topwin    = glxwindow_INIT_FREEABLE ;
   x11display_t* x11disp   = glxwin->display ;
   x11screen_t   x11screen = screen_glxwindow(glxwin) ;
   rgbacolor_t      blue   = { 0, 0, 1, 1 } ;
   rgbacolor_t      blue2  = { 0, 0, 1, 0 } ;
   rgbacolor_t       red   = { 1, 0, 0, 1 } ;
   x11attribute_t tconf[]  = {   x11attribute_INIT_RGBA(1,1,1,1) } ;
   x11attribute_t config[] = {   x11attribute_INIT_WINPOS(100, 102),
                                 x11attribute_INIT_WINSIZE(400, 200),
                                 x11attribute_INIT_RGBA(1,1,1,1),
                                 x11attribute_INIT_ALPHAOPACITY
                              } ;
   int32_t        x, y ;

   // prepare
   TEST(0 == show_glxwindow(glxwin)) ;
   WAITFOR(x11disp, 10, glxwin->state == x11window_Shown) ;
   draw_background(glxwin, &red, lengthof(tconf), tconf) ;
   TEST(0 == pos_glxwindow(glxwin, &x, &y)) ;
   config[0].value.x = x ;
   config[0].value.y = y ;

   // TEST setopacity_glxwindow
   // normal window has red background => test
   WAITFOR(x11disp, 1, false) ;
   TEST(0 == compare_color(glxwin, false, 400, 200, 1, 0, 0)) ;
   TEST(0 == init_glxwindow(&topwin, &x11screen, 0, lengthof(config), config)) ;
   TEST(0 == show_glxwindow(&topwin)) ;
   WAITFOR(x11disp, 10, topwin.state == x11window_Shown) ;
   // set 50 % transparency
   TEST(0 == setopacity_glxwindow(&topwin, 0.5)) ;
   draw_background(&topwin, &blue, lengthof(tconf), tconf) ;
   WAITFOR(x11disp, 1, false) ;
   TEST(0 == compare_color(&topwin, false, 400, 200, 0, 0, 1)) ;   // topwin is blue
   for (int i = 0; i < 20; ++i) {
      WAITFOR(x11disp, 1, false) ;
      if (0 == compare_color(&topwin, true, 400, 200, 1, 0, 1)) break ;
   }
   TEST(0 == compare_color(&topwin, true, 400, 200, 1, 0, 1)) ;   // root contains the blending
   // set 100 % transparency
   TEST(0 == setopacity_glxwindow(&topwin, 0)) ;
   draw_background(&topwin, &blue, lengthof(config), config) ;
   WAITFOR(x11disp, 1, false) ;
   TEST(0 == compare_color(&topwin, false, 400, 200, 0, 0, 1)) ;   // topwin is blue
   for (int i = 0; i < 20; ++i) {
      WAITFOR(x11disp, 1, false) ;
      if (0 == compare_color(glxwin, true, 400, 200, 1, 0, 0)) break ;
   }
   TEST(0 == compare_color(glxwin, true, 400, 200, 1, 0, 0)) ;    // root contains only red
   // set 0 % transparency
   TEST(0 == setopacity_glxwindow(&topwin, 1)) ;
   WAITFOR(x11disp, 1, false) ;
   for (int i = 0; i < 20; ++i) {
      WAITFOR(x11disp, 1, false) ;
      if (0 == compare_color(glxwin, true, 400, 200, 0, 0, 1)) break ;
   }
   TEST(0 == compare_color(glxwin, true, 400, 200, 0, 0, 1)) ;    // root contains only blue

   // TEST x11attribute_INIT_ALPHAOPACITY (100% transaprent but blending adds together)
   draw_background(&topwin, &blue2, lengthof(config), config) ;
   WAITFOR(x11disp, 1, false) ;
   TEST(0 == compare_color(&topwin, false, 400, 200, 0, 0, 1)) ;   // topwin is blue
   for (int i = 0; i < 20; ++i) {
      WAITFOR(x11disp, 1, false) ;
      if (0 == compare_color(glxwin, true, 400, 200, 1, 0, 1)) break ;
   }
   TEST(0 == compare_color(glxwin, true, 400, 200, 1, 0, 1)) ;    // root contains blending

   // unprepare
   TEST(0 == free_glxwindow(&topwin)) ;
   WAITFOR(x11disp, 1, false) ;

   return 0 ;
ONABORT:
   free_glxwindow(&topwin) ;
   return EINVAL ;
}

static int test_openglconfig(x11screen_t * x11screen)
{
   GLXFBConfig    fbconfig ;
   x11display_t * x11disp  = display_x11screen(x11screen) ;


   // TEST init_glxwindow, matchfbconfig_glxwindow: ESRCH
   {
      x11attribute_t attr[]  = { x11attribute_INIT_RGBA(20000,1,1,1) } ;
      glxwindow_t    glxwin  = glxwindow_INIT_FREEABLE ;
      TEST(ESRCH == matchfbconfig_glxwindow(x11screen, &fbconfig, 1, attr)) ;
      TEST(ESRCH == init_glxwindow(&glxwin, x11screen, 0, 1, attr)) ;
   }

   // TEST matchfbconfig_glxwindow: glxattribute_INIT_DOUBLEBUFFER
   {
      x11attribute_t attr_on[]  = { x11attribute_INIT_REDBITS(1), x11attribute_INIT_DOUBLEBUFFER } ;
      x11attribute_t attr_off[] = { x11attribute_INIT_REDBITS(1) } ;
      int dblbuf ;
      TEST(0 == matchfbconfig_glxwindow(x11screen, &fbconfig, lengthof(attr_on), attr_on)) ;
      dblbuf = 0 ;
      glXGetFBConfigAttrib(x11disp->sys_display, fbconfig, GLX_DOUBLEBUFFER, &dblbuf) ;
      TEST(dblbuf > 0) ;
      TEST(0 == matchfbconfig_glxwindow(x11screen, &fbconfig, 1, attr_off))
      dblbuf = 1 ;
      glXGetFBConfigAttrib(x11disp->sys_display, fbconfig, GLX_DOUBLEBUFFER, &dblbuf) ;
      TEST(dblbuf == 0) ;
   }

   // TEST matchfbconfig_glxwindow: glxattribute_INIT_DEPTH
   {
      x11attribute_t attr[] = { x11attribute_INIT_DEPTHBITS(1) } ;
      TEST(0 == matchfbconfig_glxwindow(x11screen, &fbconfig, 1, attr)) ;
      int depth = 0 ;
      glXGetFBConfigAttrib(x11disp->sys_display, fbconfig, GLX_DEPTH_SIZE, &depth) ;
      TEST(depth > 0) ;
   }

   // TEST matchfbconfig_glxwindow: glxattribute_INIT_STENCIL
   {
      x11attribute_t  attr_on[] = { x11attribute_INIT_STENCILBITS(1) } ;
      x11attribute_t attr_off[] = { x11attribute_INIT_STENCILBITS(0) } ;
      int stencil ;
      TEST(0 == matchfbconfig_glxwindow(x11screen, &fbconfig, 1, attr_on)) ;
      stencil = 0 ;
      glXGetFBConfigAttrib(x11disp->sys_display, fbconfig, GLX_STENCIL_SIZE, &stencil) ;
      TEST(stencil > 0) ;
      TEST(0 == matchfbconfig_glxwindow(x11screen, &fbconfig, 1, attr_off)) ;
      stencil = 1 ;
      glXGetFBConfigAttrib(x11disp->sys_display, fbconfig, GLX_STENCIL_SIZE, &stencil) ;
      TEST(stencil == 0) ;
   }

   // TEST matchfbconfig_glxwindow: all attributes set twice
   {
      x11attribute_t  attr_all[] = {   x11attribute_INIT_ALPHAOPACITY,
                                       x11attribute_INIT_DOUBLEBUFFER,
                                       x11attribute_INIT_REDBITS(1),
                                       x11attribute_INIT_GREENBITS(1),
                                       x11attribute_INIT_BLUEBITS(1),
                                       x11attribute_INIT_ALPHABITS(1),
                                       x11attribute_INIT_DEPTHBITS(1),
                                       x11attribute_INIT_STENCILBITS(1),
                                       x11attribute_INIT_ACCUM_REDBITS(1),
                                       x11attribute_INIT_ACCUM_GREENBITS(1),
                                       x11attribute_INIT_ACCUM_BLUEBITS(1),
                                       x11attribute_INIT_ACCUM_ALPHABITS(1),
                                       x11attribute_INIT_ALPHAOPACITY,
                                       x11attribute_INIT_DOUBLEBUFFER,
                                       x11attribute_INIT_REDBITS(1),
                                       x11attribute_INIT_GREENBITS(1),
                                       x11attribute_INIT_BLUEBITS(1),
                                       x11attribute_INIT_ALPHABITS(1),
                                       x11attribute_INIT_DEPTHBITS(1),
                                       x11attribute_INIT_STENCILBITS(1),
                                       x11attribute_INIT_ACCUM_REDBITS(1),
                                       x11attribute_INIT_ACCUM_GREENBITS(1),
                                       x11attribute_INIT_ACCUM_BLUEBITS(1),
                                       x11attribute_INIT_ACCUM_ALPHABITS(1)
                                       } ;
      (void) matchfbconfig_glxwindow(x11screen, &fbconfig, lengthof(attr_all), attr_all) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int childprocess_unittest(void)
{
   x11display_t      x11disp   = x11display_INIT_FREEABLE ;
   x11screen_t       x11screen = x11screen_INIT_FREEABLE ;
   testwindow_t      testwin   = testwindow_INIT_FREEABLE ;
   testwindow_it     iimpl     = x11window_it_INIT(_testwindow) ;
   glxwindow_t *     glxwin    = &testwin.glxwin ;
   resourceusage_t   usage     = resourceusage_INIT_FREEABLE ;
   x11attribute_t    config[]  = {  x11attribute_INIT_WINFRAME,
                                    x11attribute_INIT_WINTITLE("unittest: glxwindow_t"),
                                    x11attribute_INIT_WINPOS(100, 102),
                                    x11attribute_INIT_WINSIZE(400, 200),
                                    x11attribute_INIT_WINMINSIZE(200, 100),
                                    x11attribute_INIT_RGBA(1,1,1,1),
                                    x11attribute_INIT_DOUBLEBUFFER
                                 } ;

   // prepare
   TEST(0 == init_x11display(&x11disp, ":0")) ;
   x11screen = defaultscreen_x11display(&x11disp) ;
   TEST(0 == init_glxwindow(glxwin, &x11screen, genericcast_x11windowit(&iimpl, testwindow_t), lengthof(config), config)) ;

   // X11/GLX has memory leaks or caching or malloc consumes additional management space
   if (test_initfree(&x11screen))         goto ONABORT ;
   if (test_query(&x11screen, glxwin))    goto ONABORT ;
   if (test_change(&testwin))             goto ONABORT ;
   if (test_transparency(&testwin))       goto ONABORT ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_openglconfig(&x11screen))     goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   // unprepare
   TEST(0 == free_glxwindow(glxwin)) ;
   TEST(0 == free_x11display(&x11disp)) ;

   return 0 ;
ONABORT:
   (void) free_glxwindow(glxwin) ;
   (void) free_x11display(&x11disp) ;
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

int unittest_platform_X11_glxwindow()
{
   int err;

   TEST(0 == execasprocess_unittest(&childprocess_unittest, &err));

   return err;
ONABORT:
   return EINVAL;
}

#endif
