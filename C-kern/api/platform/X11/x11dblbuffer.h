/* title: X11-DoubleBuffer

   Add double buffer (back buffer) support to x11window_t.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2013 Jörg Seebohn

   file: C-kern/api/platform/X11/x11dblbuffer.h
    Header file <X11-DoubleBuffer>.

   file: C-kern/platform/X11/x11dblbuffer.c
    Implementation file <X11-DoubleBuffer impl>.
*/
#ifndef CKERN_PLATFORM_X11_X11DBLBUFFER_HEADER
#define CKERN_PLATFORM_X11_X11DBLBUFFER_HEADER

// forward
struct x11window_t ;

/* typedef: struct x11dblbuffer_t
 * Export <x11dblbuffer_t> into global namespace. */
typedef struct x11dblbuffer_t    x11dblbuffer_t;



// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_platform_X11_x11dblbuffer
 * Test <x11dblbuffer_t> functionality. */
int unittest_platform_X11_x11dblbuffer(void);
#endif


/* struct: x11dblbuffer_t
 * A drawable which describes the double buffer (back buffer) of a <x11window_t>. */
struct x11dblbuffer_t {
   /* variable: display
    * Reference to <x11display_t>. Every call to X library needs this parameter. */
   struct x11display_t* display;
   /* variable: sys_drawable
    * X window ID. The ID describes a drawable either of type window, backbuffer or pixmap. */
   uint32_t             sys_drawable;
   /* variable: sys_colormap
    * X window ID. The ID describes a colormap which is associated with the drawable.
    * A colormap is used to map the pixel depth of drawable to screen's pixel depth. */
   uint32_t             sys_colormap;
} ;

// group: lifetime

/* define: x11dblbuffer_FREE
 * Static initializer. */
#define x11dblbuffer_FREE \
         { 0, 0, 0 }

/* function: init_x11dblbuffer
 * Tries to allocate a double buffer associated with window. */
int init_x11dblbuffer(/*out*/x11dblbuffer_t * dblbuf, struct x11window_t * x11win);

/* function: free_x11dblbuffer
 * Frees and deallocates the double buffer associated with a window.
 * You need to call this function before you free the associated window. */
int free_x11dblbuffer(x11dblbuffer_t * dblbuf);



#endif
