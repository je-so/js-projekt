/* title: EGL-Display

   Implements the binding of a native os-specific graphics display
   to OpenGL / OpenGLES. The implementation uses the EGL API which
   adapts OpenGL to the native windowing system.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2014 Jörg Seebohn

   file: C-kern/api/platform/OpenGL/EGL/egldisplay.h
    Header file <EGL-Display>.

   file: C-kern/platform/OpenGL/EGL/egldisplay.c
    Implementation file <EGL-Display impl>.
*/
#ifndef CKERN_PLATFORM_OPENGL_EGL_EGLDISPLAY_HEADER
#define CKERN_PLATFORM_OPENGL_EGL_EGLDISPLAY_HEADER

// forward
struct x11display_t;

/* typedef: sys_display_t
 * Type which tags the native implementation of a system display handle. */
typedef struct sys_display_t sys_display_t;

/* typedef: opengl_display_t
 * Type which tags the native implementation of an OpenGL capable display. */
typedef struct opengl_display_t  opengl_display_t;

/* typedef: struct egldisplay_t
 * Export <egldisplay_t> into global namespace. */
typedef struct opengl_display_t * egldisplay_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_platform_opengl_egl_egldisplay
 * Test <egldisplay_t> functionality. */
int unittest_platform_opengl_egl_egldisplay(void);
#endif


/* struct: egldisplay_t
 * Wrapper of a native display type.
 * Adds additional information used by OpenGL / EGL. */
typedef struct opengl_display_t * egldisplay_t;

// group: lifetime

/* define: egldisplay_FREE
 * Static initializer. */
#define egldisplay_FREE 0

/* function: initdefault_egldisplay
 * Initializes egldisp with the default display connection.
 * Returns EINVAL if there is no default display connection
 * or EALLOC if the egl specific part could not be initialized. */
int initdefault_egldisplay(/*out*/egldisplay_t * egldisp);

/* function: init_egldisplay
 * Initializes egldisp with sysdisp.
 * Returns EINVAL if sysdisp is not initialized or invalid
 * or EALLOC if the egl specific part could not be initialized.
 * Do not free sysdisp as long as egldisp is not freed. */
int init_egldisplay(/*out*/egldisplay_t * egldisp, struct sys_display_t * sysdisp);

/* function: free_egldisplay
 * Frees all associated resources with a display.
 * If one or more threads has a current context and surface
 * these resources are marked for deletion. */
int free_egldisplay(egldisplay_t * egldisp);


#endif
