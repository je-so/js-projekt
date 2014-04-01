/* title: Graphic-Pixel-Buffer

   Abstraction of a native OpenGL off-screen pixel buffer
   to make other modules OS independent.

   Supports OpenGL / GLES for drawing operations.

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

   file: C-kern/api/graphic/pixelbuffer.h
    Header file <Graphic-Pixel-Buffer>.

   file: C-kern/graphic/pixelbuffer.c
    Implementation file <Graphic-Pixel-Buffer impl>.
*/
#ifndef CKERN_GRAPHIC_PIXELBUFFER_HEADER
#define CKERN_GRAPHIC_PIXELBUFFER_HEADER

// forward
struct display_t;
struct gconfig_t;

#ifdef KONFIG_USERINTERFACE_EGL
#include "C-kern/api/platform/OpenGL/EGL/eglpbuffer.h"
#endif

#include "C-kern/api/graphic/surface.h"

/* typedef: struct pixelbuffer_t
 * Export <pixelbuffer_t> into global namespace. */
typedef struct pixelbuffer_t pixelbuffer_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_graphic_pixelbuffer
 * Test <pixelbuffer_t> functionality. */
int unittest_graphic_pixelbuffer(void);
#endif


/* struct: pixelbuffer_t
 * Wraps a native OpenGL off-screen pixel buffer.
 * You can use this drawing surface for off-screen rendering.
 * Call <setcurrent_gcontext> with a pixelbuffer as 3rd and 4th argument.
 * To read the pixels call OpenGL/ES function glReadPixels. */
struct pixelbuffer_t {
   surface_EMBED;
};

// group: lifetime

/* define: pixelbuffer_INIT_FREEABLE
 * Static initializer. */
#define pixelbuffer_INIT_FREEABLE \
         surface_INIT_FREEABLE

/* function: init_pixelbuffer
 * Creates a new native OpenGL off-screen pixel buffer.
 * It is returned in pbuf.
 * EALLOC is returned in case width or height are too large or not enough resources. */
int init_pixelbuffer(/*out*/pixelbuffer_t * pbuf, struct display_t * disp, struct gconfig_t * gconf, uint32_t width, uint32_t height);

/* function: free_pixelbuffer
 * Frees all associated resources.
 * Make sure that no thread uses pbuf as current drawing surface
 * before calling this function. See <setcurrent_gcontext>. */
int free_pixelbuffer(pixelbuffer_t * pbuf, struct display_t * disp);

// group: query

/* function: gl_pixelbuffer
 * Returns a pointer to a native opengl window. */
struct opengl_window_t * gl_pixelbuffer(const pixelbuffer_t * pbuf);

/* function: size_pixelbuffer
 * Returns the width and height of pbuf. */
int size_pixelbuffer(const pixelbuffer_t * pbuf, struct display_t * disp, /*out*/uint32_t * width, /*out*/uint32_t * height);

/* function: configid_pixelbuffer
 * Returns the configuration ID of pbuf in configid.
 * Use id in a call to <initfromconfigid_gconfig> for querying the configuration values
 * assigned during creation of pbuf. */
int configid_pixelbuffer(const pixelbuffer_t * pbuf, struct display_t * disp, /*out*/uint32_t * configid);


// section: inline implementation

// group: pixelbuffer_t

/* define: gl_pixelbuffer
 * Implements <pixelbuffer_t.gl_pixelbuffer>. */
#define gl_pixelbuffer(win) \
         gl_surface(win)

#ifdef KONFIG_USERINTERFACE_EGL

/* define: size_pixelbuffer
 * Implements <pixelbuffer_t.size_pixelbuffer>. */
#define size_pixelbuffer(pbuf, disp, width, height) \
         size_eglpbuffer(gl_pixelbuffer(pbuf), gl_display(disp), width, height)

/* define: configid_pixelbuffer
 * Implements <pixelbuffer_t.configid_pixelbuffer>. */
#define configid_pixelbuffer(pbuf, disp, configid) \
         configid_eglpbuffer(gl_pixelbuffer(pbuf), gl_display(disp), configid)

#else
   #error "Not implemented"
#endif

#endif
