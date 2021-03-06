/* title: Graphic-Surface

   Wraps an OS specific OpenGL surface to make
   other modules OS independent. A surface
   abstracts the frame buffer (color, depth, stencil, ...).

   Supports OpenGL / GLES for drawing operations.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2014 Jörg Seebohn

   file: C-kern/api/graphic/surface.h
    Header file <Graphic-Surface>.

   file: C-kern/graphic/surface.c
    Implementation file <Graphic-Surface impl>.
*/
#ifndef CKERN_GRAPHIC_SURFACE_HEADER
#define CKERN_GRAPHIC_SURFACE_HEADER

/* typedef: opengl_surface_t
 * Type which tags the native implementation of an OpenGL surface. */
typedef struct opengl_surface_t opengl_surface_t;

/* typedef: struct surface_t
 * Export <surface_t> into global namespace. */
typedef struct surface_t surface_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_graphic_surface
 * Test <surface_t> functionality. */
int unittest_graphic_surface(void);
#endif


/* struct: surface_t
 * Wraps a native OpenGL surface (frame buffer).
 * Use <gl_surface> to query for the native type. */
struct surface_t {
   struct opengl_surface_t * glsurface;
};

// group: lifetime

/* define: surface_FREE
 * Static initializer. */
#define surface_FREE \
         { 0 }

/* define: surface_FREE_EMBEDDED
 * Static initializer for types which inherit from <surface_t> with <surface_EMBED>. */
#define surface_FREE_EMBEDDED \
         0

// group: generic

/* define: surface_EMBED
 * Inherits from <surface_t> by embedding all data fields. */
#define surface_EMBED \
         struct opengl_surface_t * glsurface

// group: query

/* function: gl_surface
 * Returns the native OpenGL surface type tagged with (opengl_surface_t*). */
struct opengl_surface_t * gl_surface(surface_t * surf);

/* function: isfree_surface
 * Returns true if surf->glsurface is set to 0. */
bool isfree_surface(const surface_t * surf);


// section: inline implementation

// group: surface_t

/* define: gl_surface
 * Implements <surface_t.gl_surface>. */
#define gl_surface(surf) \
         ((surf)->glsurface)

/* define: isfree_surface
 * Implements <surface_t.isfree_surface>. */
#define isfree_surface(surf) \
         (0 == (surf)->glsurface)


#endif
