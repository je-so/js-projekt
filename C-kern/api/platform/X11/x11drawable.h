/* title: X11-Drawable

   Describes an object which supports writing text and drawing lines and shapes.

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
   (C) 2013 Jörg Seebohn

   file: C-kern/api/platform/X11/x11drawable.h
    Header file <X11-Drawable>.

   file: C-kern/platform/shared/X11/x11drawable.c
    Implementation file <X11-Drawable impl>.
*/
#ifndef CKERN_PLATFORM_X11_X11DRAWABLE_HEADER
#define CKERN_PLATFORM_X11_X11DRAWABLE_HEADER

/* typedef: struct x11drawable_t
 * Export <x11drawable_t> into global namespace. */
typedef struct x11drawable_t              x11drawable_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_platform_X11_x11drawable
 * Test <x11drawable_t> functionality. */
int unittest_platform_X11_x11drawable(void) ;
#endif


/* struct: x11drawable_t
 * Base class of <x11window_t> or <glxwindow_t> (or Pixmap).
 * You can use text and drawing functions to draw into a drawable. */
struct x11drawable_t {
   /* variable: display
    * Reference to <x11display_t>. Every call to X library needs this parameter. */
   struct x11display_t  * display ;
   /* variable: sys_drawable
    * X window ID. The ID describes a drawable either of type window, backbuffer or pixmap. */
   uint32_t             sys_drawable ;
   /* variable: sys_colormap
    * X window ID. The ID describes a colormap which is associated with the window.
    * A colormap is used to map the window pixel depth to the screen pixel depth. */
   uint32_t             sys_colormap ;
} ;

// group: lifetime

/* define: x11drawable_INIT_FREEABLE
 * Static initializer. */
#define x11drawable_INIT_FREEABLE \
         { 0, 0, 0 }

/* define: x11drawable_INIT_FREEABLE
 * Static initializer. See <x11drawable_t> for the description of the parameter. */
#define x11drawable_INIT(display, sys_drawable, sys_colormap) \
            { display, sys_drawable, sys_colormap }

// group: query

// group: draw-lines

// group: draw-shapes

// group: draw-text


// section: inline implementation

// group: x11drawable_t


#endif
