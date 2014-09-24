/* title: X11-Drawable

   Describes an object which supports writing text and drawing lines and shapes.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2013 Jörg Seebohn

   file: C-kern/api/platform/X11/x11drawable.h
    Header file <X11-Drawable>.

   file: C-kern/platform/X11/x11drawable.c
    Implementation file <X11-Drawable impl>.
*/
#ifndef CKERN_PLATFORM_X11_X11DRAWABLE_HEADER
#define CKERN_PLATFORM_X11_X11DRAWABLE_HEADER

/* typedef: struct x11drawable_t
 * Export <x11drawable_t> into global namespace. */
typedef struct x11drawable_t x11drawable_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_platform_X11_x11drawable
 * Test <x11drawable_t> functionality. */
int unittest_platform_X11_x11drawable(void);
#endif


/* struct: x11drawable_t
 * Base class of <x11window_t> or <glxwindow_t> (or Pixmap).
 * You can use text and drawing functions to draw into a drawable. */
struct x11drawable_t {
   /* variable: display
    * Reference to <x11display_t>. Every call to X library needs this parameter. */
   struct x11display_t* display;
   /* variable: sys_drawable
    * X window ID. The ID describes a drawable either of type window, backbuffer or pixmap. */
   uint32_t             sys_drawable;
   /* variable: sys_colormap
    * X window ID. The ID describes a colormap which is associated with the drawable.
    * A colormap is used to map the drawable pixel depth to the screen pixel depth. */
   uint32_t             sys_colormap;
};

// group: lifetime

/* define: x11drawable_FREE
 * Static initializer. */
#define x11drawable_FREE \
         { 0, 0, 0 }

/* define: x11drawable_INIT
 * Static initializer. See <x11drawable_t> for the description of the parameter. */
#define x11drawable_INIT(display, sys_drawable, sys_colormap) \
         { display, sys_drawable, sys_colormap }

// group: query

/* function: cast_x11drawable
 * Casts drawable into pointer to <x11drawable_t> if that is possible.
 * The first fields of drawable must match the data fields in <x11drawable_t>. */
x11drawable_t* cast_x11drawable(void * drawable);

// group: draw-lines

// group: draw-shapes

// group: draw-text



// section: inline implementation

// group: x11drawable_t

/* define: cast_x11drawable
 * Implements <x11drawable_t.cast_x11drawable>. */
#define cast_x11drawable(drawable) \
         ( __extension__ ({               \
            typeof(drawable) _d;          \
            _d = (drawable);              \
           static_assert(                 \
               &_d->display               \
               == &((x11drawable_t*)      \
                     _d)->display         \
               && &_d->sys_drawable       \
                  == &((x11drawable_t*)   \
                        _d)->sys_drawable \
               && &_d->sys_colormap       \
                  == &((x11drawable_t*)   \
                     _d)->sys_colormap,   \
               "compatible structure"     \
            );                            \
            (x11drawable_t*) _d;          \
         }))



#endif
