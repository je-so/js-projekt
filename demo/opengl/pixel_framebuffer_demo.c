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
#include "C-kern/api/test/assert.h"
#include "C-kern/api/graphic/display.h"
#include "C-kern/api/graphic/gconfig.h"
#include "C-kern/api/graphic/gcontext.h"
#include "C-kern/api/graphic/pixelbuffer.h"
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

#define PIXELBUFFER_SIZE   512

typedef struct coordinates_t coordinates_t;

struct coordinates_t {
   uint16_t x;
   uint16_t y;
   uint16_t width;
   uint16_t height;
};

static GLuint framebuffer = 0;
static GLuint renderbuffer = 0;
static GLuint vertprocid = 0;
static GLuint fragprocid = 0;
static GLuint progid     = 0;
static float xadd = 0;
static float yadd = 0;

static void drawline(const coordinates_t * line, const coordinates_t * viewport, /*out*/coordinates_t * linedrawn)
{
   glViewport(viewport->x, viewport->y, viewport->width, viewport->height);
   glClearColor(1, 1, 1, 1);
   glClear(GL_COLOR_BUFFER_BIT);

   float pos[6];

   float color[8] = {
      0, 0, 0, 0,
      0, 0, 0, 0,
   };

   glEnableVertexAttribArray(0/*p_pos*/);
   glEnableVertexAttribArray(1/*p_color*/);
   glVertexAttribPointer(0/*p_pos*/, 3, GL_FLOAT, GL_FALSE, 0, pos);
   glVertexAttribPointer(1/*p_color*/, 4, GL_FLOAT, GL_FALSE, 0, color);

   pos[0] = -1.0f + (2.0f*line->x) / (float) viewport->width;
   pos[1] = -1.0f + (2.0f*line->y) / (float) viewport->height;
   pos[2] = 0;
   pos[5] = 0;
   if (line->width) {
      pos[3] = -1.0f + (2.0f*((float)line->x + line->width)) / (float) viewport->width;
      pos[4] = pos[1];
   } else {
      pos[3] = pos[0];
      pos[4] = -1.0f + (2.0f*((float)line->y + line->height)) / (float) viewport->height;
   }
   pos[0] += xadd / viewport->width;
   pos[3] += xadd / viewport->width;
   pos[1] += yadd / viewport->height;
   pos[4] += yadd / viewport->height;

   glDrawArrays(GL_LINE_STRIP, 0, 2);
   glDisableVertexAttribArray(0);
   glDisableVertexAttribArray(1);

   static uint32_t pixels[PIXELBUFFER_SIZE][PIXELBUFFER_SIZE];
   glReadPixels(0, 0, PIXELBUFFER_SIZE, PIXELBUFFER_SIZE, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

   int sx, sy, ex, ey;
   for (sy = 0; sy < PIXELBUFFER_SIZE; ++sy)
      for (sx = 0; sx < PIXELBUFFER_SIZE; ++sx)
         if (pixels[sy][sx] == 0) {
            goto FOUND_START_PIXEL;
         }
   FOUND_START_PIXEL:

   linedrawn->x = (uint16_t) sx;
   linedrawn->y = (uint16_t) sy;
   linedrawn->width  = 0;
   linedrawn->height = 0;

   if (line->width) {
      for (ex = sx; ex < PIXELBUFFER_SIZE && pixels[sy][ex] == 0; ++ex)
         ;
      linedrawn->width  = (uint16_t) (ex-sx);

   } else {
      for (ey = sy; ey < PIXELBUFFER_SIZE && pixels[ey][sx] == 0; ++ey)
         ;
      linedrawn->height = (uint16_t) (ey-sy);
   }
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
   vertprocid = glCreateShader(GL_VERTEX_SHADER);
   fragprocid = glCreateShader(GL_FRAGMENT_SHADER);
   progid     = glCreateProgram();
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

static int create_frambufferobject(void)
{
   glGenFramebuffers(1, &framebuffer);
   glGenRenderbuffers(1, &renderbuffer);
   glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
   glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
   TEST(0 == glGetError());
   glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA4, PIXELBUFFER_SIZE, PIXELBUFFER_SIZE);
   glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, renderbuffer);
   TEST(GL_FRAMEBUFFER_COMPLETE == glCheckFramebufferStatus(GL_FRAMEBUFFER));
   TEST(0 == glGetError());
   glBindFramebuffer(GL_FRAMEBUFFER, 0);
   glBindRenderbuffer(GL_RENDERBUFFER, 0);
   TEST(0 == glGetError());

   return 0;
ONABORT:
   return EINVAL;
}

static int do_tests(int isFBO)
{
   coordinates_t viewport  = { 0, 0, PIXELBUFFER_SIZE, PIXELBUFFER_SIZE };
   coordinates_t viewport2 = { 0, 0, PIXELBUFFER_SIZE-1, PIXELBUFFER_SIZE-1 };
   coordinates_t hori      = { 0, 0, PIXELBUFFER_SIZE, 0 };
   coordinates_t vert      = { 0, 0, 0, PIXELBUFFER_SIZE };
   coordinates_t drawn;

   if (isFBO) {
      glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
      TEST(0 == glGetError());
      printf("====\nUse framebuffer object as render target\n");
   } else {
      printf("====\nUse pixelbuffer as render target\n");
   }

   xadd = 0;
   yadd = 0;

   // calibrate xadd
   while (xadd < 0.5f) {
      drawline(&vert, &viewport, &drawn);
      if (drawn.height) break;
      xadd += 0.1f;
   }

   // calibrate yadd
   while (yadd < 0.5f) {
      drawline(&hori, &viewport, &drawn);
      if (drawn.width) break;
      yadd += 0.1f;
   }

   printf("calibrated: xadd = %f, yadd = %f\n", xadd, yadd);

   drawline(&(coordinates_t) { 0, 0, PIXELBUFFER_SIZE, 0 }, &(coordinates_t) { 10, 10, 10, 10 }, &drawn);
   if (drawn.width != 10 || drawn.x != 10 || drawn.y != 10) {
      printf("test1: drawline failed\n");
      printf("drawn = (%d,%d,%d,%d)\n", drawn.x, drawn.y, drawn.width, drawn.height);
      return EINVAL;
   }

   drawline(&(coordinates_t) { 1, 0, 0, PIXELBUFFER_SIZE }, &(coordinates_t) { 20, 0, 100, 200 }, &drawn);
   if (drawn.height != 200 || drawn.x != 21 || drawn.y != 0) {
      printf("test2: drawline failed\n");
      printf("drawn = (%d,%d,%d,%d)\n", drawn.x, drawn.y, drawn.width, drawn.height);
      return EINVAL;
   }

   drawline(&(coordinates_t) { 0, 0, 0, 200 }, &viewport, &drawn);
   if (drawn.height != 200 || drawn.x != 0 || drawn.y != 0) {
      printf("test3: drawline failed\n");
      printf("drawn = (%d,%d,%d,%d)\n", drawn.x, drawn.y, drawn.width, drawn.height);
      return EINVAL;
   }

   drawline(&(coordinates_t) { 0, 0, 200, 0 }, &viewport, &drawn);
   if (drawn.width != 200 || drawn.x != 0 || drawn.y != 0) {
      printf("test4: drawline failed\n");
      printf("drawn = (%d,%d,%d,%d)\n", drawn.x, drawn.y, drawn.width, drawn.height);
      return EINVAL;
   }

   drawline(&(coordinates_t) { 0, 0, 201, 0 }, &viewport2, &drawn);
   if (drawn.width != 201 || drawn.x != 0 || drawn.y != 0) {
      printf("test5: drawline failed\n");
      printf("drawn = (%d,%d,%d,%d)\n", drawn.x, drawn.y, drawn.width, drawn.height);
      return EINVAL;
   }

   drawline(&(coordinates_t) { 0, 0, 0, 201 }, &viewport2, &drawn);
   if (drawn.height != 201 || drawn.x != 0 || drawn.y != 0) {
      printf("test6: drawline failed\n");
      printf("drawn = (%d,%d,%d,%d)\n", drawn.x, drawn.y, drawn.width, drawn.height);
      return EINVAL;
   }

   drawline(&(coordinates_t) { PIXELBUFFER_SIZE-1, 0, 0, PIXELBUFFER_SIZE }, &viewport, &drawn);
   if (drawn.height != PIXELBUFFER_SIZE || drawn.x != PIXELBUFFER_SIZE-1 || drawn.y != 0) {
      printf("test7: drawline failed\n");
      printf("drawn = (%d,%d,%d,%d)\n", drawn.x, drawn.y, drawn.width, drawn.height);
      return EINVAL;
   }

   drawline(&(coordinates_t) { 0, PIXELBUFFER_SIZE, PIXELBUFFER_SIZE, 0 }, &viewport, &drawn);
   if (drawn.width != 0 || drawn.height != 0) {
      printf("test8: drawline failed\n");
      printf("drawn = (%d,%d,%d,%d)\n", drawn.x, drawn.y, drawn.width, drawn.height);
      return EINVAL;
   }

   drawline(&(coordinates_t) { 0, PIXELBUFFER_SIZE-1, PIXELBUFFER_SIZE, 0 }, &viewport, &drawn);
   if (drawn.width != PIXELBUFFER_SIZE || drawn.x != 0 || drawn.y != PIXELBUFFER_SIZE-1)  {
      printf("test9: drawline failed\n");
      printf("drawn = (%d,%d,%d,%d)\n", drawn.x, drawn.y, drawn.width, drawn.height);
      return EINVAL;
   }

   printf("drawline experiments: OK\n");

   if (isFBO) {
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      TEST(0 == glGetError());
   }

   return 0;
ONABORT:
   return EINVAL;
}

int pixel_framebuffer_demo(maincontext_t * maincontext)
{
   (void) maincontext;
   display_t         disp;
   pixelbuffer_t     pbuffer;
   gconfig_t         gconf;
   gcontext_t        gcontext;
   int32_t           conf_attribs[] = {
      gconfig_BITS_BUFFER, 32,
      gconfig_BITS_DEPTH, 4,
      gconfig_TYPE, gconfig_value_TYPE_PBUFFER_BIT,
      gconfig_CONFORMANT, gconfig_value_CONFORMANT_ES2_BIT,
      gconfig_NONE
   };

   TEST(0 == initdefault_display(&disp));

   TEST(0 == init_gconfig(&gconf, &disp, conf_attribs));
   TEST(0 == init_pixelbuffer(&pbuffer, &disp, &gconf, PIXELBUFFER_SIZE, PIXELBUFFER_SIZE));
   TEST(0 == init_gcontext(&gcontext, &disp, &gconf, gcontext_api_OPENGLES));
   TEST(0 == setcurrent_gcontext(&gcontext, &disp, &pbuffer, &pbuffer));
   TEST(0 == create_opengles_program());
   TEST(0 == create_frambufferobject());

   for (int isFBO = 0; isFBO <= 1; ++isFBO) {

      TEST(0 == do_tests(isFBO));

   }

   TEST(0 == glGetError());
   glUseProgram(0);
   glDeleteProgram(progid);
   glDeleteShader(vertprocid);
   glDeleteShader(fragprocid);
   glDeleteFramebuffers(1, &framebuffer);
   glDeleteRenderbuffers(1, &renderbuffer);
   TEST(0 == glGetError());

   TEST(0 == releasecurrent_gcontext(&disp));
   TEST(0 == free_gcontext(&gcontext, &disp));
   TEST(0 == free_pixelbuffer(&pbuffer, &disp));
   TEST(0 == free_display(&disp));

   return 0;
ONABORT:
   return EINVAL;
}
