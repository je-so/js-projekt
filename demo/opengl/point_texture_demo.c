/* title: Point-Texture-Opengl-ES-Demo

   Loads a texture and draws a rectangle
   with the texture on it.
   Draws points with size > 1 also with the texture
   drawn on them.
   This reduces the number of coordinates to send
   to the vertex shader.

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
   unsigned textureid;
   unsigned progid;
   unsigned vertprocid;
   unsigned fragprocid;
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
   glClearColor(0, 0, 0.5f, 0);
   glClearDepthf(1);
   glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

   static const float pos[] = {
      -0.5f,  0.5f,     1,
      0.7f,  0,     1,
      0,     0.9f,     1,
      0.8f,  0.8f,     1,
     -1,    -1,    -1,
      0.1f, -1,    -1,
     -1,     0.1f, -1,
      0.1f,  0.1f, -1,
   };

   static const float texcoord[] = {
      0, 0,
      1, 0,
      0, 1,
      1, 1,
      0, 0,
      1, 0,
      0, 1,
      1, 1,
   };

   glEnableVertexAttribArray(0/*a_pos*/);
   glEnableVertexAttribArray(1/*a_texcoord*/);
   glVertexAttribPointer(0/*a_pos*/, 3, GL_FLOAT, GL_FALSE, 0, pos);
   glVertexAttribPointer(1/*a_texcoord*/, 2, GL_FLOAT, GL_FALSE, 0, texcoord);
   glDepthFunc(GL_LEQUAL);
   glEnable(GL_DEPTH_TEST);
   int ispointsprite = glGetUniformLocation(win->progid,  "u_ispointsprite");
   TEST(-1 != ispointsprite);
   glUniform1f(ispointsprite, 0.0);
   // draw rectangle with texture
   glDrawArrays(GL_TRIANGLE_STRIP, 4, 4);
   // draw points of size 32.0 with texture
   glUniform1f(ispointsprite, 1.0);
   glDrawArrays(GL_POINTS, 0, 4);
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

static int create_opengles_program(demowindow_t * win)
{
   const char * vertex_procedure =
      "attribute mediump vec4 a_pos;\n"   // position parameter
      "attribute mediump vec2 a_texcoord;\n" // texture coordinate parameter
      "varying mediump vec2 v_texcoord;\n"   // texture coord. sent to fragment shader
      "void main(void)\n"
      "{\n"
      "   gl_Position = a_pos;\n"         // set position
      "   gl_PointSize = 32.0;\n"
      "   v_texcoord = a_texcoord;\n"     // attach texture information
      "}";

   const char * fragment_procedure =
      "uniform sampler2D u_texunit;\n"       // active texture unit where the image is stored
      "uniform float     u_ispointsprite;\n" // switch between point sprite tex coordinates and v_texcoord
      "varying mediump vec2 v_texcoord;\n"   // texture coord parameter (0,0) left bottom, (1,1) right top
      "void main(void)\n"
      "{\n"
      "   mediump vec2 texcoord;\n"
      "   texcoord = (1.0-u_ispointsprite) * v_texcoord + u_ispointsprite * gl_PointCoord;\n"
      "   gl_FragColor = texture2D(u_texunit, texcoord);\n"  // set color of pixel from texture image
      "}";

   // create program/procedure IDs
   unsigned vertprocid = glCreateShader(GL_VERTEX_SHADER);
   unsigned fragprocid = glCreateShader(GL_FRAGMENT_SHADER);
   unsigned progid     = glCreateProgram();
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
   glBindAttribLocation(progid, 0, "a_pos");
   glBindAttribLocation(progid, 1, "a_texcoord");
   glLinkProgram(progid);
   isok = 0;
   glGetProgramiv(progid, GL_LINK_STATUS, &isok);
   TEST(isok);
   TEST(0 == glGetAttribLocation(progid,  "a_pos"));
   TEST(1 == glGetAttribLocation(progid,  "a_texcoord"));

   // activate program
   glGetError(); // clear error
   glUseProgram(progid);
   TEST(GL_NO_ERROR == glGetError()); // check error of glUseProgram

   // set texture unit to 0
   int texunit = glGetUniformLocation(progid,  "u_texunit");
   TEST(-1 != texunit);
   glUniform1i(texunit, 0);

   int ispointsprite = glGetUniformLocation(progid,  "u_ispointsprite");
   TEST(-1 != ispointsprite);
   glUniform1f(ispointsprite, 1.0);

   // set out values
   win->progid = progid;
   win->vertprocid = vertprocid;
   win->fragprocid = fragprocid;

   return 0;
ONABORT:
   return EINVAL;
}

static int load_texture(demowindow_t * win)
{
   uint8_t image[32][32][4];
   // draw texture image (double arrow head)
   memset(image, 0, sizeof(image));
   for (unsigned y = 0; y < 32; ++y) {
      memset(image[y][y%16], 255, 4*2*(16-y%16));
   }

   // texcoord(0,0) == image[0][0]
   // texcoord(1,0) == image[0][31]
   // texcoord(0,1) == image[31][0]
   // texcoord(1,1) == image[31][31]

   // texutre unit 0 is also the default
   glActiveTexture(GL_TEXTURE0);

   // generate texture object and load image data into object
   GLuint tid;
   glGenTextures(1, &tid);
   TEST(0 == glGetError());
   glBindTexture(GL_TEXTURE_2D, tid);
   TEST(0 == glGetError());
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 32, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
   glGenerateMipmap(GL_TEXTURE_2D);
   TEST(0 == glGetError());

   // ////
   // if only 1 texture is used the default could be used
   // glBindTexture(GL_TEXTURE_2D, 0) is the default and is active at program start
   // ////

   // set out values
   win->textureid = tid;

   return 0;
ONABORT:
   return EINVAL;
}

int point_texture_demo(maincontext_t * maincontext)
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
   TEST(0 == init_window(&win.super, &disp, snr, cast_windowevh(&eventhandler, demowindow_t), &gconf, winattr));
   TEST(0 == init_gcontext(&gcontext, &disp, &gconf, gcontext_api_OPENGLES));
   TEST(0 == setcurrent_gcontext(&gcontext, &disp, &win.super, &win.super));

   TEST(0 == load_texture(&win));
   TEST(0 == create_opengles_program(&win));

   TEST(0 == show_window(&win.super));
   while (!win.isClosed) {
      TEST(0 == nextevent_X11(os_display(&disp)));
   }

   GLuint objectid;
   glUseProgram(0);
   glBindTexture(GL_TEXTURE_2D, 0);
   glDeleteProgram(win.progid);
   glDeleteShader(win.vertprocid);
   glDeleteShader(win.fragprocid);
   objectid = win.textureid;
   glDeleteTextures(1, &objectid);
   TEST(0 == glGetError());
   TEST(0 == releasecurrent_gcontext(&disp));
   TEST(0 == free_gcontext(&gcontext, &disp));
   TEST(0 == free_window(&win.super));
   TEST(0 == free_display(&disp));

   return 0;
ONABORT:
   return EINVAL;
}
