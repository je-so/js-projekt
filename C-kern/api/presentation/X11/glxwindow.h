/* title: X11-OpenGL-Window
   Offers support for displaying OpenGL output
   in a window on a X11 display.

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
   (C) 2011 Jörg Seebohn

   file: C-kern/api/presentation/X11/glxwindow.h
    Header file of <X11-OpenGL-Window>.

   file: C-kern/presentation/glxwindow.c
    Implementation file of <X11-OpenGL-Window>.
*/
#ifndef CKERN_PRESENTATION_X11_OPENGL_WINDOW_HEADER
#define CKERN_PRESENTATION_X11_OPENGL_WINDOW_HEADER


/* typedef: glxwindow_t typedef
 * Shortcut for <glxwindow_t>.
 * Holds X11 window supporting an OpenGL context. */
typedef struct glxwindow_t                glxwindow_t ;

/* typedef: glxwindow_configuration_t typedef
 * Shortcut for <glxwindow_configuration_t>.
 * Holds Parameter for <init_glxwindow>. */
typedef struct glxwindow_configuration_t  glxwindow_configuration_t ;

/* typedef: glxattribute_t typedef
 * Shortcut for <glxattribute_t>.
 * Describes an OpenGL attribute type and its value. */
typedef struct glxattribute_t             glxattribute_t ;

/* enums: glxwindow_state_e
 * glxwindowDestroyed  - The window was destroyed from outside before <free_glxwindow> was called.
 * glxwindowHidden     - The window is created but not shown to the user (iconic or minimized state).
 * glxwindowVisible    - The window is created and shown and visible to the user.
 * */
enum glxwindow_state_e {
    glxwindowDestroyed
   ,glxwindowHidden
   ,glxwindowVisible
   ,glxwindowObscured
} ;

typedef enum glxwindow_state_e glxwindow_state_e ;

/* enums: glxattribute_type_e
 *
 * glxattribute_DOUBLEBUFFER - Indicates with *true*(1) or *false*(0) if a double buffer is supported.
 * glxattribute_REDBITS      - The number of red bits the color buffer supports, e.g. 8 on current hardware.
 * glxattribute_GREENBITS    - The number of green bits the color buffer supports, e.g. 8 on current hardware.
 * glxattribute_BLUEBITS     - The number of blue bits the color buffer supports, e.g. 8 on current hardware.
 * glxattribute_ALPHABITS    - The number of alpha bits the color buffer supports, e.g. 8 on current hardware.
 * glxattribute_DEPTHBITS    - The number of bits the depth buffer supports. If set to 0 no depth buffer is supported.
 * glxattribute_STENCILBITS  - The number of bits the stencil buffer supports. If set to 0 no stencil buffer is supported.
 * glxattribute_ACCUM_REDBITS   - The number of red bits the accumulation buffer supports, e.g. 16 on current hardware.
 * glxattribute_ACCUM_GREENBITS - The number of green bits the accumulation buffer supports, e.g. 16 on current hardware.
 * glxattribute_ACCUM_BLUEBITS  - The number of blue bits the accumulation buffer supports, e.g. 16 on current hardware.
 * glxattribute_ACCUM_ALPHABITS - The number of alpha bits the accumulation buffer supports, e.g. 16 on current hardware.
 * glxattribute_TRANSPARENT_X_VISUAL   - Tries to create an OpenGL configuration where the window manager interprets the
 *                                       alpha channel as blending value with the underlying window background.
 *                                       The blending function is color = color(window) + (1 - alpha) * color(background). */
enum glxattribute_type_e {
    glxattribute_DOUBLEBUFFER
   ,glxattribute_REDBITS
   ,glxattribute_GREENBITS
   ,glxattribute_BLUEBITS
   ,glxattribute_ALPHABITS
   ,glxattribute_DEPTHBITS
   ,glxattribute_STENCILBITS
   ,glxattribute_ACCUM_REDBITS
   ,glxattribute_ACCUM_GREENBITS
   ,glxattribute_ACCUM_BLUEBITS
   ,glxattribute_ACCUM_ALPHABITS
   ,glxattribute_TRANSPARENT_X_VISUAL
} ;

typedef enum glxattribute_type_e glxattribute_type_e ;

// forward
struct x11display_t ;


// section: Functions


// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_presentation_X11_glxwindow
 * Tests opening a window on local display. */
extern int unittest_presentation_X11_glxwindow(void) ;
#endif


struct glxattribute_t {
   glxattribute_type_e  type ;
   int32_t              value ;
} ;

// group: init

#define GLX_ATTRIB_DOUBLEBUFFER(on_off)   { .type = glxattribute_DOUBLEBUFFER,   .value = on_off }
#define GLX_ATTRIB_RGBA(redbits,greenbits,bluebits,alphabits)                                        \
                                          { .type = glxattribute_REDBITS,        .value = redbits   } \
                                         ,{ .type = glxattribute_GREENBITS,      .value = greenbits } \
                                         ,{ .type = glxattribute_BLUEBITS,       .value = bluebits  } \
                                         ,{ .type = glxattribute_ALPHABITS,      .value = alphabits }
#define GLX_ATTRIB_DEPTH(bits)            { .type = glxattribute_DEPTHBITS,      .value = bits }
#define GLX_ATTRIB_STENCIL(bits)          { .type = glxattribute_STENCILBITS,    .value = bits }
/* define: GLX_ATTRIB_TRANSPARENT
 * Makes content of window transparent if set to 1.
 * Tries to choose an X11 RGBA visual (X Render Extension) which interprets the alpha color channel
 * as blending value with the background.
 *
 * Meaning of alpha values:
 * 1 - fully opaque
 * 0 - fully transparent.
 *
 * Blending function:
 * > color = color(opengl buffer) + (1 - alpha) * color(background behind window) *
 * The blending function assumes that all color values in the opengl color buffer
 * are premultiplied by alpha.
 * > color.rgb(opengl buffer)´ = color.rgb(opengl buffer) * color.alpha(opengl buffer) */
#define GLX_ATTRIB_TRANSPARENT(on_off)    { .type = glxattribute_TRANSPARENT_X_VISUAL, .value = on_off }

/* struct: glxwindow_configuration_t
 * Contains *in* parameters of <init_glxwindow> function call.
 * If a parameter is set to 0 it is considered »undefined«.
 * At least the <display> and <width> and <height> should be set. */
struct glxwindow_configuration_t {
   /* variable: display
    * The display (default screen) the window should appear on. */
   struct x11display_t * display ;
   /* variable: window_title
    * The text which appears on the window title bar. */
   const char        * window_title ;
   /* variable: wm_no_frame
    * True means the window manager is out of the way.
    * Useable for popups or other overlaying toplevel
    * windows which do not need a wm frame. */
   bool              wm_no_frame ;
   /* variable: wm_chooses_xypos
    * True means the window manager chooses xpos and ypos. */
   bool              wm_chooses_xypos ;
   /* variable: wm_is_resizable
    * True means the window can not be resized. */
   bool              wm_not_resizable ;
   /* variable: xpos
    * The x coordinate (0 is left of screen) of the newly created window. */
   int32_t           xpos ;
   /* variable: ypos
    * The y coordinate (0 is top of screen) of the newly created window. */
   int32_t           ypos ;
   /* variable: width
    * The width in pixels of the newly created window. */
   uint32_t          width ;
   /* variable: height
    * The height in pixels of the newly created window. */
   uint32_t          height ;
   uint8_t           glxattrib_count ;
   glxattribute_t    * glxattrib/*[glxattrib_count]*/ ;
} ;


/* struct: glxwindow_t
 * Describes an OpenGL window on a X11 display <x11display_t>. */
struct glxwindow_t {
   struct x11display_t   * display ;
   uint32_t             sys_window ;
   uint32_t             sys_colormap ;
   /* variable: state
    * Current state of window (visible, destroyed, hidden).
    *
    * glxwindowDestroyed:
    * Normally the window is destroyed in the destructor <free_glxwindow>.
    * Therefore if the window is in a destroyed state
    * before calling it some other program must have done this. */
   glxwindow_state_e    state ;

   /* variable: user_requested_close
    * If true then user wanted to close window.
    * The window is not closed but only a message is
    * received which sets this flags. */
   bool                 user_requested_close ;
   /* variable: need_redraw
    * Window needs drawing of content.
    * This flag is set if the window after is
    * shown for the first time or if it was
    * hidden by another window. */
   bool                 need_redraw ;
} ;

// group: lifetime

/* define: glxwindow_INIT_FREEABLE
 * Static initializer: makes calling of <free_glxwindow> safe. */
#define glxwindow_INIT_FREEABLE     { .display = 0, .sys_window = 0, .sys_colormap = 0 }

/* function: init_glxwindow
 * */
extern int init_glxwindow( /*out*/glxwindow_t * glxwin, const glxwindow_configuration_t * config ) ;

/* function: free_glxwindow
 * */
extern int free_glxwindow( glxwindow_t * glxwin ) ;

/* function: initmove_glxwindow
 * Must be called if address of <glxwindow_t> changes.
 * A simple memcpy from source to destination does not work.
 *
 * Not implemented. */
extern int initmove_glxwindow( /*out*/glxwindow_t * destination, glxwindow_t * source ) ;

// group: query

/* function: pos_glxwindow
 * Returns the position of the window in screen coordinates.
 * The value (screen_x == 0, screeny_ == 0) denotes the top left corner of the screen. */
extern int pos_glxwindow( glxwindow_t * glxwin, /*out*/int32_t * screen_x, /*out*/int32_t * screen_y ) ;

/* function: size_glxwindow
 * Returns the width and height of the window in pixels. */
extern int size_glxwindow( glxwindow_t * glxwin, /*out*/uint32_t * width, /*out*/uint32_t * height ) ;

/* function: geometry_glxwindow
 * This is the geometry of window without the window manager frame in screen coordinates.
 * The value (screen_x == 0, screeny_ == 0) denotes the top left corner of the screen. */
extern int geometry_glxwindow( glxwindow_t * glxwin,
                        /*out*/int32_t * screen_x, /*out*/int32_t * screen_y,
                        /*out*/uint32_t * width, /*out*/uint32_t * height ) ;

/* function: frame_glxwindow
 * This is the geometry of window including the window manager frame in screen coordinates.
 * The value (screen_x == 0, screeny_ == 0) denotes the top left corner of the screen. */
extern int frame_glxwindow( glxwindow_t * glxwin,
                        /*out*/int32_t * screen_x, /*out*/int32_t * screen_y,
                        /*out*/uint32_t * width, /*out*/uint32_t * height ) ;

// group: change

/* function: settransparency_glxwindow
 * Sets the transparency of the toplevel window.
 * An alpha value of 1 draws the window opaque a value of 0 makes
 * it totally translucent.
 * If alpha is set to a value outside of [0..1] EINVAL is returned.
 *
 * Precondition:
 * For the function to work the window manager of the X11 Screen
 * must support the X11 composite extension. */
extern int settransparency_glxwindow( glxwindow_t * glxwin, double alpha ) ;

/* function: show_glxwindow
 * In X11 terms maps a window: makes it visible to the user.
 * After receiveing the notification from the X server
 * the window state is set to <glxwindowVisible>. */
extern int show_glxwindow( glxwindow_t * glxwin ) ;

/* function: hide_glxwindow
 * In X11 terms unmaps a window: makes it invisible to the user.
 * After receiveing the notification from the X server
 * the window state is set to <glxwindowHidden>. */
extern int hide_glxwindow( glxwindow_t * glxwin ) ;

/* function: setpos_glxwindow
 * Changes the position of the window on the screen.
 * Coordinates are in screen coordinates.
 * The coordinate (0,0) is top left. */
extern int setpos_glxwindow( glxwindow_t * glxwin, /*out*/int32_t screen_x, /*out*/int32_t screen_y ) ;

/* function: clear_glxwindow
 * Clears the content of the window.
 * A redraw event (X11: Expose event) is sent. */
extern int clear_glxwindow( glxwindow_t * glxwin ) ;

#endif
