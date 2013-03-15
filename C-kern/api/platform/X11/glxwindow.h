/* title: X11-OpenGL-Window
   Offers support for displaying OpenGL output
   in a window on a X11 display.

   Do not forget to include <X11-Window> before using some of the inlined functions.

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

   file: C-kern/api/platform/X11/glxwindow.h
    Header file of <X11-OpenGL-Window>.

   file: C-kern/platform/shared/X11/glxwindow.c
    Implementation file of <X11-OpenGL-Window impl>.
*/
#ifndef CKERN_PLATFORM_X11_OPENGL_WINDOW_HEADER
#define CKERN_PLATFORM_X11_OPENGL_WINDOW_HEADER

// forward
struct cstring_t ;
struct x11attribute_t ;
struct x11display_t ;
struct x11drawable_t ;
struct x11screen_t ;
struct x11window_it ;

/* typedef: struct glxwindow_t
 * Shortcut for <glxwindow_t>.
 * Holds X11 window supporting an OpenGL context. */
typedef struct glxwindow_t                glxwindow_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_platform_X11_glxwindow
 * Tests opening a window on local display. */
int unittest_platform_X11_glxwindow(void) ;
#endif


/* struct: glxwindow_t
 * Describes an OpenGL window on a X11 display <x11display_t>. */
struct glxwindow_t {
   /* variable: display
    * Reference to <x11display_t>. Every call to X library needs this parameter. */
   struct x11display_t* display ;
   /* variable: sys_window
    * X window ID. The ID describes a drawable of type window. */
   uint32_t             sys_window ;
   /* variable: sys_colormap
    * X window ID. The ID describes a colormap which is associated with the window.
    * A colormap is used to map the window pixel depth to the screen pixel depth. */
   uint32_t             sys_colormap ;
   /* variable: iimpl
    * Reference to <x11window_it> which handles events. */
   const struct x11window_it * iimpl ;
   /* variable: state
    * See <x11window_t.state>. */
   uint8_t              state ;
   /* variable: flags
    * See <x11window_t.flags>. */
   uint8_t              flags ;
} ;

// group: lifetime

/* define: glxwindow_INIT_FREEABLE
 * Static initializer: makes calling of <free_glxwindow> safe. */
#define glxwindow_INIT_FREEABLE           { 0, 0, 0, 0, 0, 0 }

/* function: init_glxwindow
 * Initializes glxwin and creates a hidden on window on x11screen.
 * The configuration is read from the array configuration[nrofattributes].
 * To handle any events set eventhandler to a valid value != 0.
 * Call <show_glxwindow> to make the window visible to the user. */
int init_glxwindow(/*out*/glxwindow_t * glxwin, struct x11screen_t * x11screen, const struct x11window_it * eventhandler, uint8_t nrofattributes, const struct x11attribute_t * configuration/*[nrofattributes]*/) ;

/* function: free_glxwindow
 * Delete the associated x11 window and clear glxwin. */
int free_glxwindow(glxwindow_t * glxwin) ;

// group: query

/* function: screen_glxwindow
 * See <x11window_t.screen_x11window>. */
struct x11screen_t screen_glxwindow(const glxwindow_t * glxwin) ;

/* function: flags_glxwindow
 * See <x11window_t.flags_x11window>. */
uint8_t flags_glxwindow(const glxwindow_t * glxwin) ;

/* function: state_glxwindow
 * See <x11window_t.state_x11window>. */
uint8_t state_glxwindow(const glxwindow_t * glxwin) ;

/* function: title_glxwindow
 * See <x11window_t.title_x11window>. */
int title_glxwindow(const glxwindow_t * glxwin, /*ret*/struct cstring_t * title) ;

/* function: pos_glxwindow
 * See <x11window_t.pos_x11window>. */
int pos_glxwindow(const glxwindow_t * glxwin, /*out*/int32_t * screen_x, /*out*/int32_t * screen_y) ;

/* function: size_glxwindow
 * See <x11window_t.size_x11window>. */
int size_glxwindow(const glxwindow_t * glxwin, /*out*/uint32_t * width, /*out*/uint32_t * height) ;

/* function: geometry_glxwindow
 * See <x11window_t.geometry_x11window>. */
int geometry_glxwindow(const glxwindow_t * glxwin, /*out*/int32_t * screen_x, /*out*/int32_t * screen_y, /*out*/uint32_t * width, /*out*/uint32_t * height) ;

/* function: frame_glxwindow
 * See <x11window_t.frame_x11window>. */
int frame_glxwindow(const glxwindow_t * glxwin, /*out*/int32_t * screen_x, /*out*/int32_t * screen_y, /*out*/uint32_t * width, /*out*/uint32_t * height) ;

// group: change

/* function: show_glxwindow
 * See <x11window_t.show_x11window>. */
int show_glxwindow(glxwindow_t * glxwin) ;

/* function: hide_glxwindow
 * See <x11window_t.hide_x11window>. */
int hide_glxwindow(glxwindow_t * glxwin) ;

/* function: setpos_glxwindow
 * See <x11window_t.setpos_x11window>. */
int setpos_glxwindow(glxwindow_t * glxwin, int32_t screen_x, int32_t screen_y) ;

/* function: resize_glxwindow
 * See <x11window_t.resize_x11window>. */
int resize_glxwindow(glxwindow_t * glxwin, uint32_t width, uint32_t height) ;

/* function: sendredraw_glxwindow
 * See <x11window_t.sendredraw_x11window>. */
int sendredraw_glxwindow(glxwindow_t * glxwin) ;

/* function: sendcloserequest_glxwindow
 * See <x11window_t.sendcloserequest_x11window>. */
int sendcloserequest_glxwindow(glxwindow_t * glxwin) ;

/* function: settitle_glxwindow
 * See <x11window_t.settitle_x11window>. */
int settitle_glxwindow(const glxwindow_t * glxwin, const char * title) ;

/* function: setopacity_glxwindow
 * See <x11window_t.setopacity_x11window>. */
int setopacity_glxwindow(glxwindow_t * glxwin, double opacity) ;


// section: inline implementation

// group: glxwindow_t

/* define: flags_glxwindow
 * Implements <glxwindow_t.flags_glxwindow>. */
#define flags_glxwindow(glxwin) \
         (flags_x11window(genericcast_x11window(glxwin)))

/* define: frame_glxwindow
 * Implements <glxwindow_t.frame_glxwindow>. */
#define frame_glxwindow(glxwin, screen_x, screen_y, width, height) \
         (frame_x11window(genericcast_x11window(glxwin), (screen_x), (screen_y), (width), (height)))

/* define: geometry_glxwindow
 * Implements <glxwindow_t.geometry_glxwindow>. */
#define geometry_glxwindow(glxwin, screen_x, screen_y, width, height) \
         (geometry_x11window(genericcast_x11window(glxwin), (screen_x), (screen_y), (width), (height)))

/* define: hide_glxwindow
 * Implements <glxwindow_t.hide_glxwindow>. */
#define hide_glxwindow(glxwin) \
         (hide_x11window(genericcast_x11window(glxwin)))

/* define: pos_glxwindow
 * Implements <glxwindow_t.pos_glxwindow>. */
#define pos_glxwindow(glxwin, screen_x, screen_y) \
         (pos_x11window(genericcast_x11window(glxwin), (screen_x), (screen_y)))

/* define: resize_glxwindow
 * Implements <glxwindow_t.resize_glxwindow>. */
#define resize_glxwindow(glxwin, width, height) \
         (resize_x11window(genericcast_x11window(glxwin), (width), (height)))

/* define: screen_glxwindow
 * Implements <glxwindow_t.screen_glxwindow>. */
#define screen_glxwindow(glxwin) \
         (screen_x11window(genericcast_x11window(glxwin)))

/* define: sendcloserequest_glxwindow
 * Implements <glxwindow_t.sendcloserequest_glxwindow>. */
#define sendcloserequest_glxwindow(glxwin) \
         (sendcloserequest_x11window(genericcast_x11window(glxwin)))

/* define: sendredraw_glxwindow
 * Implements <glxwindow_t.sendredraw_glxwindow>. */
#define sendredraw_glxwindow(glxwin) \
         (sendredraw_x11window(genericcast_x11window(glxwin)))

/* define: setopacity_glxwindow
 * Implements <glxwindow_t.setopacity_glxwindow>. */
#define setopacity_glxwindow(glxwin, opacity) \
         (setopacity_x11window(genericcast_x11window(glxwin), (opacity)))

/* define: setpos_glxwindow
 * Implements <glxwindow_t.setpos_glxwindow>. */
#define setpos_glxwindow(glxwin, screen_x, screen_y) \
         (setpos_x11window(genericcast_x11window(glxwin), (screen_x), (screen_y)))

/* define: settitle_glxwindow
 * Implements <glxwindow_t.settitle_glxwindow>. */
#define settitle_glxwindow(glxwin, title) \
         (settitle_x11window(genericcast_x11window(glxwin), (title)))

/* define: show_glxwindow
 * Implements <glxwindow_t.show_glxwindow>. */
#define show_glxwindow(glxwin) \
         (show_x11window(genericcast_x11window(glxwin)))

/* define: size_glxwindow
 * Implements <glxwindow_t.size_glxwindow>. */
#define size_glxwindow(glxwin, width, height) \
         (size_x11window(genericcast_x11window(glxwin), (width), (height)))

/* define: state_glxwindow
 * Implements <glxwindow_t.state_glxwindow>. */
#define state_glxwindow(glxwin) \
         (state_x11window(genericcast_x11window(glxwin)))

/* define: title_glxwindow
 * Implements <glxwindow_t.title_glxwindow>. */
#define title_glxwindow(glxwin, title) \
         (title_x11window(genericcast_x11window(glxwin), (title)))

#endif
