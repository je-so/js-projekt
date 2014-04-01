/* title: Graphic-Display

   Wraps the OS specific display initialization for
   the graphics display into a thin layer to maker
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

   file: C-kern/api/graphic/display.h
    Header file <Graphic-Display>.

   file: C-kern/graphic/display.c
    Implementation file <Graphic-Display impl>.
*/
#ifndef CKERN_GRAPHIC_DISPLAY_HEADER
#define CKERN_GRAPHIC_DISPLAY_HEADER

#define KONFIG_opengl_egl 1
#define KONFIG_opengl_glx 2
#define KONFIG_x11        4

#if ((KONFIG_USERINTERFACE)&KONFIG_x11)
#include "C-kern/api/platform/X11/x11display.h"
#endif

#if ((KONFIG_USERINTERFACE)&KONFIG_opengl_egl)
#include "C-kern/api/platform/OpenGL/EGL/egldisplay.h"
#endif

/* typedef: struct display_t
 * Export <display_t> into global namespace. */
typedef struct display_t display_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_graphic_display
 * Test <display_t> functionality. */
int unittest_graphic_display(void);
#endif


/* struct: display_t
   Wraps the OS specific graphics display.
   Support OpenGL / GLES. */
struct display_t {
#if ((KONFIG_USERINTERFACE)&KONFIG_x11) && ((KONFIG_USERINTERFACE)&KONFIG_opengl_egl)
   x11display_t   osdisplay;
   egldisplay_t   gldisplay;
#else
   #error "display_t not implemented for definition of KONFIG_USERINTERFACE"
#endif
};

// group: lifetime

/* define: display_INIT_FREEABLE
 * Static initializer. */
#if ((KONFIG_USERINTERFACE)&KONFIG_x11) && ((KONFIG_USERINTERFACE)&KONFIG_opengl_egl)
#define display_INIT_FREEABLE \
         { x11display_INIT_FREEABLE, egldisplay_INIT_FREEABLE }
#endif

/* function: initdefault_display
 * Initializes disp with a connection to the default display. */
int initdefault_display(/*out*/display_t * disp);

/* function: free_display
 * Frees all resources associated wth the display.
 * You should free other resources which depend on the
 * diplay connection before calling this function. */
int free_display(display_t * disp);

// group: query

/* function: defaultscreennr_display
 * Returns the number of the default screens attached to this display.
 * The returns value is always less than the value returned by <nrofscreens_display>. */
uint32_t defaultscreennr_display(const display_t * disp);

/* function: nrofscreens_display
 * Returns the number of different screens attached to this display.
 * A display represents a set of graphics cards driven by a single driver.
 * If more than one display monitor is attached, it is possible to configure
 * the display to have a single screen as as large as the screen estate of
 * all monitors together or to configure it to have multiple screens controlled
 * independently. */
uint32_t nrofscreens_display(const display_t * disp);

/* function: gl_display
 * Returns a pointer to a native opengl display. */
struct opengl_display_t * gl_display(const display_t * disp);

/* function: os_display
 * Returns a pointer to a native display.
 * This functino is implemented as a macro and therefore
 * returns a pointer to the real native type not only void.
 * It is possible that gl_display returns the same pointer
 * except for the type. In case of EGL as OpenGL adaption layer
 * both displays differ. */
void * os_display(const display_t * disp);

// group: update


// section: inline implementation

// group: display_t

/* define: gl_display
 * Implements <display_t.gl_display>. */
#define gl_display(disp) \
         ((disp)->gldisplay)

/* define: os_display
 * Implements <display_t.os_display>. */
#define os_display(disp) \
         (&(disp)->osdisplay)

#undef KONFIG_opengl_egl
#undef KONFIG_opengl_glx
#undef KONFIG_x11

#endif