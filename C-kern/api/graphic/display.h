/* title: Graphic-Display

   Wraps the OS specific display initialization for
   the graphics display into a thin layer to make
   other modules OS independent.

   Supports OpenGL / GLES for drawing operations.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2014 Jörg Seebohn

   file: C-kern/api/graphic/display.h
    Header file <Graphic-Display>.

   file: C-kern/graphic/display.c
    Implementation file <Graphic-Display impl>.
*/
#ifndef CKERN_GRAPHIC_DISPLAY_HEADER
#define CKERN_GRAPHIC_DISPLAY_HEADER

#ifdef KONFIG_USERINTERFACE_X11
#include "C-kern/api/platform/X11/x11display.h"
#endif

#ifdef KONFIG_USERINTERFACE_EGL
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
#if defined(KONFIG_USERINTERFACE_X11) && defined(KONFIG_USERINTERFACE_EGL)
   x11display_t   osdisplay;
   egldisplay_t   gldisplay;
#else
   #error "display_t not implemented for definition of KONFIG_USERINTERFACE"
#endif
};

// group: lifetime

#if defined(KONFIG_USERINTERFACE_X11) && defined(KONFIG_USERINTERFACE_EGL)
/* define: display_FREE
 * Static initializer. */
#define display_FREE \
         { x11display_FREE, egldisplay_FREE }
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
 * This function is implemented as a macro and therefore
 * returns a pointer to the real native type and not void.
 * It is possible that gl_display returns the same pointer
 * except for the type. In case of EGL as OpenGL adaption layer
 * both pointers differ. */
void * os_display(const display_t * disp);

/* function: castfromos_display
 * Casts pointer to osdisplay into pointer to <display_t>. */
display_t * castfromos_display(const void * osdisplay);

// group: update


// section: inline implementation

// group: display_t

/* define: castfromos_display
 * Implements <display_t.castfromos_display>. */
#define castfromos_display(from_osdisplay)   \
         ( __extension__ ({                  \
            typeof(((display_t*)0)->osdisplay)  \
               * _from = (from_osdisplay);   \
            (display_t*) _from;              \
         }))

/* define: gl_display
 * Implements <display_t.gl_display>. */
#define gl_display(disp) \
         ((disp)->gldisplay)

/* define: os_display
 * Implements <display_t.os_display>. */
#define os_display(disp) \
         (&(disp)->osdisplay)

#endif
