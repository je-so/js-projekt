/* title: Setup-Opengl-ES-Demo

   Creates a native window, initializes an EGL context
   and draws a triangle into the window.

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
   (C) 2014 Jörg Seebohn

   file: tutorial/opengl/setup_opengles_demo.c
    Implementation file <Setup-Opengl-ES-Demo>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/graphic/display.h"
#include "C-kern/api/graphic/surfaceconfig.h"
#include "C-kern/api/graphic/window.h"
#include "C-kern/api/graphic/windowconfig.h"
#include "C-kern/api/platform/X11/x11.h"
#include "C-kern/api/platform/X11/x11screen.h"
#include "C-kern/api/platform/X11/x11window.h"
#include STR(C-kern/api/platform/KONFIG_OS/graphic/sysegl.h)
#include STR(C-kern/api/platform/KONFIG_OS/graphic/sysgles2.h)

#define TEST(CONDITION) \
         if (!(CONDITION)) { \
            printf("%s:%d: ERROR\n", __FILE__, __LINE__); \
            goto ONABORT; \
         }

static bool          s_isClosed = false;
static EGLDisplay    s_egldisplay;
static EGLSurface    s_eglwindow;

static void onclose_demowindow(x11window_t * x11win)
{
   (void) x11win;
   s_isClosed = true;
}

static void ondestroy_demowindow(x11window_t * x11win)
{
   (void) x11win;
}

static void onredraw_demowindow(x11window_t * x11win)
{
   (void) x11win;
   glClearColor(0, 0, 0, 1);
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
   eglSwapBuffers(s_egldisplay, s_eglwindow);
}

static void onreshape_demowindow(x11window_t * x11win, uint32_t width, uint32_t height)
{
   (void) x11win;
   glViewport(0, 0, (int)width, (int)height);
}

static void onvisible_demowindow(x11window_t * x11win, x11window_state_e state)
{
   (void) x11win;
   (void) state;
}

static int create_opengl_program(void)
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
   window_t          win;
   surfaceconfig_t   surfconf;
   EGLContext        eglcontext;
   int32_t           conf_attribs[] = {
      surfaceconfig_BITS_BUFFER, 32,
      surfaceconfig_BITS_DEPTH, 4,
      surfaceconfig_CONFORMANT, surfaceconfig_value_CONFORMANT_ES2_BIT,
      surfaceconfig_NONE
   };
   windowconfig_t winattr[] = {
      windowconfig_INIT_FRAME,
      windowconfig_INIT_TITLE("setup_opengles_demo"),
      windowconfig_INIT_SIZE(400, 400),
      windowconfig_INIT_POS(100, 100),
      windowconfig_INIT_NONE
   };
   x11window_it   eventhandler = x11window_it_INIT(_demowindow);

   TEST(0 == initdefault_display(&disp));
   snr = defaultscreennr_display(&disp);
   s_egldisplay = (void*)gl_display(&disp);

   TEST(0 == init_surfaceconfig(&surfconf, &disp, conf_attribs));
   TEST(0 == init_window(&win, &disp, snr, &surfconf, winattr));
   os_window(&win)->iimpl = &eventhandler; // TODO: remove when init_window supports eventhandler
   s_eglwindow = (void*)gl_window(&win);

   eglcontext = eglCreateContext(gl_display(&disp), surfconf.config, EGL_NO_CONTEXT, (EGLint[]){EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE});
   TEST(EGL_NO_CONTEXT != eglcontext);
   TEST(EGL_TRUE == eglMakeCurrent(gl_display(&disp), (void*)gl_window(&win), (void*)gl_window(&win), eglcontext));

   TEST(0 == create_opengl_program());

   TEST(0 == show_x11window(os_window(&win)));
   while (!s_isClosed) {
      TEST(0 == nextevent_X11(os_display(&disp)));
   }

   TEST(EGL_TRUE == eglMakeCurrent(gl_display(&disp), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
   TEST(EGL_TRUE == eglDestroyContext(gl_display(&disp), eglcontext));
   TEST(0 == free_window(&win, &disp));
   TEST(0 == free_display(&disp));

   return 0;
ONABORT:
   return EINVAL;
}
