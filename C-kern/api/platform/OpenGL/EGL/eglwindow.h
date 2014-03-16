/* title: EGL-Window

   Wraps a native window into an EGL window surface.
   The EGL surface serves as a drawing target for
   OpenGL and OpenGL ES drawing commands.

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

   file: C-kern/api/platform/OpenGL/EGL/eglwindow.h
    Header file <EGL-Window>.

   file: C-kern/platform/OpenGL/EGL/eglwindow.c
    Implementation file <EGL-Window impl>.
*/
#ifndef CKERN_PLATFORM_OPENGL_EGL_EGLWINDOW_HEADER
#define CKERN_PLATFORM_OPENGL_EGL_EGLWINDOW_HEADER

// forward
struct native_surfconfig_t;
struct native_display_t;
struct x11window_t;

/* typedef: struct native_window_t
 * Export <native_window_t> into global namespace. */
typedef struct native_window_t native_window_t;

/* typedef: struct eglwindow_t
 * Export <eglwindow_t> into global namespace. */
typedef struct native_window_t * eglwindow_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_platform_opengl_egl_eglwindow
 * Test <eglwindow_t> functionality. */
int unittest_platform_opengl_egl_eglwindow(void);
#endif


/* struct: eglwindow_t
 * Wraps a native window into an EGL specific type. */
typedef struct native_window_t * eglwindow_t;

// group: lifetime

/* define: eglwindow_INIT_FREEABLE
 * Static initializer. */
#define eglwindow_INIT_FREEABLE  0

/* function: initx11_eglwindow
 * Initializes eglwin with x11win.
 * Returns EINVAL if any parameter is not initialized or invalid
 * or EALLOC if the egl specific part of the window could not be initialized.
 * Parameter display must be a valid egldisplay_t and parameter eglconf a valid eglconfig_t.
 * Do not free x11win as long as eglwin is not freed. */
int initx11_eglwindow(/*out*/eglwindow_t * eglwin, struct native_display_t * egldisp, struct native_surfconfig_t * eglconf, struct x11window_t * x11win);

/* function: free_eglwindow
 * Frees resources allocated by eglwin window surface.
 * Parameter display must point to a valid egl display. */
int free_eglwindow(eglwindow_t * eglwin, struct native_display_t * display);

// group: query

// group: update


#endif
