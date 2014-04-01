/* title: Graphic-Window

   Wraps the OS specific window and its OpenGL
   specific extension into a thin layer to make
   other modules OS independent.

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

   file: C-kern/api/graphic/window.h
    Header file <Graphic-Window>.

   file: C-kern/graphic/window.c
    Implementation file <Graphic-Window impl>.
*/
#ifndef CKERN_GRAPHIC_WINDOW_HEADER
#define CKERN_GRAPHIC_WINDOW_HEADER

// forward
struct display_t;
struct surfaceconfig_t;
struct windowconfig_t;

#ifdef KONFIG_USERINTERFACE_X11
#include "C-kern/api/platform/X11/x11window.h"
#endif

#ifdef KONFIG_USERINTERFACE_EGL
#include "C-kern/api/platform/OpenGL/EGL/eglwindow.h"
#endif

/* typedef: struct window_t
 * Export <window_t> into global namespace. */
typedef struct window_t window_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_graphic_window
 * Test <window_t> functionality. */
int unittest_graphic_window(void);
#endif


/* struct: window_t
 * Wraps a native window and its OpenGL specific wrapper (if needed). */
struct window_t {
#if defined(KONFIG_USERINTERFACE_X11) && defined(KONFIG_USERINTERFACE_EGL)
   x11window_t   oswindow;
   eglwindow_t   glwindow;
#else
   #error "window_t not implemented for definition of KONFIG_USERINTERFACE"
#endif
};

// group: lifetime

/* define: window_INIT_FREEABLE
 * Static initializer. */
#if defined(KONFIG_USERINTERFACE_X11) && defined(KONFIG_USERINTERFACE_EGL)
#define window_INIT_FREEABLE \
         { x11window_INIT_FREEABLE, eglwindow_INIT_FREEABLE }
#endif

/* function: init_window
 * Creates a new native window and its OpenGL extension/wrapper type.
 * // TODO: add support for eventhandler. */
int init_window(/*out*/window_t * win, struct display_t * disp, uint32_t screennr, struct surfaceconfig_t * surfconf, struct windowconfig_t * winattr);

/* function: free_window
 * Frees win and its associated resources like native windows.
 * Before you call this function make sure that every other resource
 * which depends on win is freed. */
int free_window(window_t * win, struct display_t * disp);

// group: query

/* function: gl_window
 * Returns a pointer to a native opengl window. */
struct opengl_window_t * gl_window(const window_t * win);

/* function: os_window
 * Returns a pointer to a native window.
 * This function is implemented as a macro and therefore
 * returns a pointer to the real native type and not void.
 * It is possible that gl_window returns the same pointer
 * except for the type. In case of EGL as OpenGL adaption layer
 * both pointers differ. */
void * os_window(const window_t * win);

// group: update



// section: inline implementation

// group: window_t

/* define: gl_window
 * Implements <window_t.gl_window>. */
#define gl_window(win) \
         ((win)->glwindow)

/* define: os_window
 * Implements <window_t.os_window>. */
#define os_window(win) \
         (&(win)->oswindow)


#endif
