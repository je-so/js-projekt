/* title: EGL-Display

   Implements the binding of a native os-specific graphics display
   to OpenGL ES (and OpenGL).

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

   file: C-kern/api/platform/OpenGL/EGL/egldisplay.h
    Header file <EGL-Display>.

   file: C-kern/platform/OpenGL/EGL/egldisplay.c
    Implementation file <EGL-Display impl>.
*/
#ifndef CKERN_PLATFORM_OPENGL_EGL_EGLDISPLAY_HEADER
#define CKERN_PLATFORM_OPENGL_EGL_EGLDISPLAY_HEADER

// forward
struct x11display_t;

/* typedef: struct native_display_t
 * Export <native_display_t> into global namespace. */
typedef struct native_display_t  native_display_t;

/* typedef: struct egldisplay_t
 * Export <egldisplay_t> into global namespace. */
typedef struct native_display_t * egldisplay_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_platform_opengl_egl_egldisplay
 * Test <egldisplay_t> functionality. */
int unittest_platform_opengl_egl_egldisplay(void);
#endif


/* struct: egldisplay_t
 * Wraps the native display type in an EGL specific way. */
typedef struct native_display_t * egldisplay_t;

// group: lifetime

/* define: egldisplay_INIT_FREEABLE
 * Static initializer. */
#define egldisplay_INIT_FREEABLE    0

/* function: initdefault_egldisplay
 * Initializes egldisp with the default display connection.
 * Returns EINVAL if there is no default display connection
 * or EALLOC if the egl specific part could not be initialized. */
int initdefault_egldisplay(/*out*/egldisplay_t * egldisp);

/* function: initx11_egldisplay
 * Initializes egldisp with x11disp.
 * Returns EINVAL if x11disp is not initialized or invalid
 * or EALLOC if the egl specific part could not be initialized.
 * Do not free x11disp as long as egldisp is not freed. */
int initx11_egldisplay(/*out*/egldisplay_t * egldisp, struct x11display_t * x11disp);

/* function: free_egldisplay
 * Frees all associated resources with a display.
 * If one or more threads has a current context and surface
 * these resources are marked for deletion. */
int free_egldisplay(egldisplay_t * egldisp);


#endif
