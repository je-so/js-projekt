/* title: EGl-PBuffer

   Supports the creation of off-screen pixel buffers (surfaces)
   for use with OpenGL ES. The implementation uses the EGL API which
   adapts OpenGL to the native windowing system.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2014 Jörg Seebohn

   file: C-kern/api/platform/OpenGL/EGL/eglpbuffer.h
    Header file <EGl-PBuffer>.

   file: C-kern/platform/OpenGL/EGL/eglpbuffer.c
    Implementation file <EGl-PBuffer impl>.
*/
#ifndef CKERN_PLATFORM_OPENGL_EGL_EGLPBUFFER_HEADER
#define CKERN_PLATFORM_OPENGL_EGL_EGLPBUFFER_HEADER

// forward
struct opengl_config_t;
struct opengl_display_t;
struct opengl_surface_t;

/* typedef: struct eglpbuffer_t
 * Export <eglpbuffer_t> into global namespace. */
typedef struct opengl_surface_t * eglpbuffer_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_platform_opengl_egl_eglpbuffer
 * Test <eglpbuffer_t> functionality. */
int unittest_platform_opengl_egl_eglpbuffer(void);
#endif


/* struct: eglpbuffer_t
 * Points to an OpenGL pixel buffer.
 * The implementation uses os independent EGL API to create the context.
 * OpenGL(ES) allows the binding of an off-screen pixel buffer to texture.
 * This allows to render the content of the texture directly.
 * This version of the EGL implementation does not support binding to a texture.
 * ANother possibility */
typedef struct opengl_surface_t * eglpbuffer_t;

// group: lifetime

/* define: eglpbuffer_FREE
 * Static initializer. */
#define eglpbuffer_FREE 0

/* function: init_eglpbuffer
 * Allocates a pixel buffer with size (width, height) in pixels and returns it in eglpbuf.
 * The parameter egldisp must be a valid <egldisplay_t> and eglconf must be a valid <eglconfig_t>
 * with attribute <gconfig_TYPE> containing set bit <gconfig_value_TYPE_PBUFFER_BIT>. */
int init_eglpbuffer(/*out*/eglpbuffer_t * eglpbuf, struct opengl_display_t * egldisp, struct opengl_config_t * eglconf, uint32_t width, uint32_t height);

/* function: free_eglpbuffer
 * Frees all associated resources with eglpbuf.
 * Make sure that the pixel buffer is not active in any thread
 * before calling this function. */
int free_eglpbuffer(eglpbuffer_t * eglpbuf, struct opengl_display_t * egldisp);

// group: query

/* function: size_eglpbuffer
 * Returns the width and height of eglpbuf in width and height. */
int size_eglpbuffer(const eglpbuffer_t eglpbuf, struct opengl_display_t * egldisp, /*out*/uint32_t * width, /*out*/uint32_t * height);

/* function: configid_eglpbuffer
 * Returns the configuration ID of eglpbuf in configid.
 * Use id in a call to <initfromconfigid_eglconfig> for querying the configuration values assigned
 * during creation of eglpbuf. */
int configid_eglpbuffer(const eglpbuffer_t eglpbuf, struct opengl_display_t * egldisp, /*out*/uint32_t * configid);

#endif
