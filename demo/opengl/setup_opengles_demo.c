/* title: Setup-Opengl-ES-Demo

   Creates a native window, initializes an EGL context
   and draws a triangle into the window.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2014 Jörg Seebohn

   file: tutorial/opengl/setup_opengles_demo.c
    Implementation file <Setup-Opengl-ES-Demo>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/test/assert.h"
#include "C-kern/api/graphic/display.h"
#include "C-kern/api/graphic/gconfig.h"
#include "C-kern/api/graphic/gcontext.h"
#include "C-kern/api/graphic/window.h"
#include "C-kern/api/graphic/windowconfig.h"
#include "C-kern/api/platform/X11/x11.h"
#include "C-kern/api/platform/X11/x11screen.h"
#include "C-kern/api/platform/X11/x11window.h"
#include "C-kern/api/graphic/gles2api.h"

typedef struct demowindow_t demowindow_t;

#define TEST(CONDITION) \
         if (!(CONDITION)) { \
            printf("%s:%d: ERROR\n", __FILE__, __LINE__); \
            goto ONABORT; \
         }

struct demowindow_t {
   window_t super;
   bool     isClosed;
};

window_evh_DECLARE(demowindow_evh_t, demowindow_t);

static void onclose_demowindow(demowindow_t * win)
{
   win->isClosed = true;
}

static void ondestroy_demowindow(demowindow_t * win)
{
   (void) win;
}

static void onredraw_demowindow(demowindow_t * win)
{
   glClearColor(0, 0, 0, 0);
   glClearDepthf(1);
   glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

   static const float pos[] = {
      0,     0,     1,
      1,     0,     1,
      0,     1,     1,
      1,     1,     1,
     -1,    -1,    -1,
      0.1f, -1,    -1,
     -1,     0.1f, -1,
      0.1f,  0.1f, -1,
   };

   static const float color[] = {
      1, 0, 0, 1,
      0, 1, 0, 1,
      0, 0, 1, 1,
      1, 1, 0, 1,
      1, 0, 1, 1,
      0, 1, 0, 1,
      0, 1, 1, 1,
      1, 1, 1, 1,
   };

   glEnableVertexAttribArray(0/*p_pos*/);
   glEnableVertexAttribArray(1/*p_color*/);
   // set pointer to p_pos parameter array
   glVertexAttribPointer(0/*p_pos*/, 3, GL_FLOAT, GL_FALSE, 0, pos);
   // set pointer to p_color parameter array
   glVertexAttribPointer(1/*p_color*/, 4, GL_FLOAT, GL_FALSE, 0, color);
   glDepthFunc(GL_LEQUAL);
   glEnable(GL_DEPTH_TEST);
   // draw first rectangle (send parameter from index 4 to 7)
   glDrawArrays(GL_TRIANGLE_STRIP, 4, 4);
   // draw 2nd rectangle (send parameter from index 0 to 3)
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   glDisableVertexAttribArray(0);
   glDisableVertexAttribArray(1);
   TEST(0 == swapbuffer_window(&win->super, display_window(&win->super)));

   return;
ONABORT:
   win->isClosed = true;
   return;
}

static void onreshape_demowindow(demowindow_t * win, uint32_t width, uint32_t height)
{
   (void) win;
   glViewport(0, 0, (int)width, (int)height);
}

static void onvisible_demowindow(demowindow_t * win, bool isVisible)
{
   (void) win;
   (void) isVisible;
}

static int create_opengles_program(void)
{
   const char * vertex_procedure =
      "attribute mediump vec4 p_pos;\n"   // position parameter
      "attribute lowp vec4 p_color;\n"    // color parameter
      "varying lowp vec4 color;\n"        // color sent to fragment shader
      "void main(void)\n"
      "{\n"
      "   gl_Position = p_pos;\n"         // set position
      "   color = p_color;\n"             // attach color information
      "}";

   const char * fragment_procedure =
      "varying lowp vec4 color;\n"        // color parameter
      "void main(void)\n"
      "{\n"
      "   gl_FragColor = color;\n"        // set color of pixel
      "}";

   // create program/procedure IDs
   uint32_t vertprocid = glCreateShader(GL_VERTEX_SHADER);
   uint32_t fragprocid = glCreateShader(GL_FRAGMENT_SHADER);
   uint32_t progid     = glCreateProgram();
   TEST(vertprocid != 0 && fragprocid != 0 && progid != 0);
   glAttachShader(progid, vertprocid);
   glAttachShader(progid, fragprocid);

   // compile procedures
   GLboolean iscompiler;
   glGetBooleanv(GL_SHADER_COMPILER, &iscompiler);
   TEST(iscompiler != 0);
   glShaderSource(vertprocid, 1, &vertex_procedure, 0);
   glCompileShader(vertprocid);
   glShaderSource(fragprocid, 1, &fragment_procedure, 0);
   glCompileShader(fragprocid);
   GLint isok = 0;
   glGetShaderiv(vertprocid, GL_COMPILE_STATUS, &isok);
   TEST(isok);
   isok = 0;
   glGetShaderiv(fragprocid, GL_COMPILE_STATUS, &isok);
   TEST(isok);

   // link to program
   glBindAttribLocation(progid, 0, "p_pos");
   glBindAttribLocation(progid, 1, "p_color");
   glLinkProgram(progid);
   isok = 0;
   glGetProgramiv(progid, GL_LINK_STATUS, &isok);
   TEST(isok);
   TEST(0 == glGetAttribLocation(progid,  "p_pos"));
   TEST(1 == glGetAttribLocation(progid,  "p_color"));

   // activate program
   glGetError(); // clear error
   glUseProgram(progid);
   TEST(GL_NO_ERROR == glGetError()); // check error of glUseProgram

   return 0;
ONABORT:
   return EINVAL;
}

int setup_opengles_demo(maincontext_t * maincontext)
{
   (void) maincontext;
   display_t         disp;
   uint32_t          snr;
   demowindow_t      win = { .super = window_FREE, .isClosed = false };
   gconfig_t         gconf;
   gcontext_t        gcontext;
   int32_t           conf_attribs[] = {
      gconfig_BITS_BUFFER, 32,
      gconfig_BITS_DEPTH, 4,
      gconfig_CONFORMANT, gconfig_value_CONFORMANT_ES2_BIT,
      gconfig_NONE
   };
   windowconfig_t winattr[] = {
      windowconfig_INIT_FRAME,
      windowconfig_INIT_TITLE("setup_opengles_demo"),
      windowconfig_INIT_SIZE(400, 400),
      windowconfig_INIT_POS(100, 100),
      windowconfig_INIT_NONE
   };
   demowindow_evh_t   eventhandler = window_evh_INIT(_demowindow);

   TEST(0 == initdefault_display(&disp));
   snr = defaultscreennr_display(&disp);

   TEST(0 == init_gconfig(&gconf, &disp, conf_attribs));
   TEST(0 == init_window(&win.super, &disp, snr, genericcast_windowevh(&eventhandler, demowindow_t), &gconf, winattr));
   TEST(0 == init_gcontext(&gcontext, &disp, &gconf, gcontext_api_OPENGLES));
   TEST(0 == setcurrent_gcontext(&gcontext, &disp, &win.super, &win.super));

   TEST(0 == create_opengles_program());

   TEST(0 == show_window(&win.super));
   while (!win.isClosed) {
      TEST(0 == nextevent_X11(os_display(&disp)));
   }

   TEST(0 == releasecurrent_gcontext(&disp));
   TEST(0 == free_gcontext(&gcontext, &disp));
   TEST(0 == free_window(&win.super));
   TEST(0 == free_display(&disp));

   return 0;
ONABORT:
   return EINVAL;
}
