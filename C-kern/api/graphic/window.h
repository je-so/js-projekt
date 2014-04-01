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

/* typedef: struct window_evh_t
 * Export <window_evh_t> into global namespace. */
typedef struct window_evh_t window_evh_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_graphic_window
 * Test <window_t> functionality. */
int unittest_graphic_window(void);
#endif


/* struct: window_evh_t
 * Callback interface for handling generic window events. */
struct window_evh_t {
   /* variable: onclose
    * The event handler is called if the user requested to close the window.
    * You can save state information before calling <free_window>. */
   void (* onclose)        (window_t * win);
   /* variable: ondestroy
    * The event handler is called if the window was destroyed by another process.
    * You must call <free_window> - any other function which uses <window_t> as parameter
    * would cause an error which would cause the process to abort in case of X11.
    * Not calling <free_window> results in a memory leak.
    * Calling <free_window> in response to an <onclose> event does not trigger an <ondestroy> callback. */
   void (* ondestroy)      (window_t * win);
   /* variable: onredraw
    * The event handler is called if the window was (partially) obscured and the obscured content has to be redrawn. */
   void (* onredraw)       (window_t * win);
   /* variable: onreshape
    * The event handler is called whenever the geometry of the window changes.
    * The x and y coordinates can be queried for with a call to <pos_window>. */
   void (* onreshape)      (window_t * win, uint32_t width, uint32_t height);
   /* variable: onvisible
    * The event handler is called whenever the window changes from hidden to shown state
    * or vice versa. If isVisible is set the window is in a shown state
    * else it is in a hidden state and not visible. */
   void (* onvisible)      (window_t * win, bool isVisible);
};

// group: generic

/* define: window_evh_INIT
 * Static initializer. */
#define window_evh_INIT(subwindow_fct_suffix) {  \
            & onclose ## subwindow_fct_suffix,     \
            & ondestroy ## subwindow_fct_suffix,   \
            & onredraw ## subwindow_fct_suffix,    \
            & onreshape ## subwindow_fct_suffix,   \
            & onvisible ## subwindow_fct_suffix    \
         }

#define window_evh_INIT_NULL \
         { 0, 0, 0, 0, 0 }

// group: generic

/* function: genericcast_windowevh
 * Casts parameter evhimpl into pointer to interface <window_evh_t>.
 * The parameter evhimpl has to be of type pointer to type declared with <window_evh_DECLARE>.
 * The other parameters must be the same as in <window_evh_DECLARE> without the first. */
const window_evh_t * genericcast_windowevh(const void * evhimpl, TYPENAME subwindow_t);

/* function: window_evh_DECLARE
 * Declares an interface to handle window events.
 * The interface is structural compatible with <window_evh_t>.
 * See <window_evh_t> for a list of declared functions.
 *
 * Parameter:
 * declared_evh_t - The name of the structure which is declared as the interface.
 *                  The name should have the suffix "_evh_t".
 * subwindow_t    - The window subtype for which declared_evh_t implements the callback functions. */
void window_evh_DECLARE(TYPENAME declared_evh_t, TYPENAME subwindow_t);


/* struct: window_t
 * Wraps a native window and its OpenGL specific wrapper (if needed). */
struct window_t {
#if defined(KONFIG_USERINTERFACE_X11) && defined(KONFIG_USERINTERFACE_EGL)
   x11window_t oswindow;
   eglwindow_t glwindow;
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
 * Creates a new native window and its OpenGL extension/wrapper type. */
int init_window(/*out*/window_t * win, struct display_t * disp, uint32_t screennr, const struct window_evh_t * eventhandler, struct surfaceconfig_t * surfconf, struct windowconfig_t * winattr);

/* function: free_window
 * Frees win and its associated resources like native windows.
 * Before you call this function make sure that every other resource
 * which depends on win is freed. */
int free_window(window_t * win);

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

/* function: display_window
 * Returns a pointer to the <display_t> the window is associated with. */
struct display_t * display_window(const window_t * win);

/* function: isvisible_window
 * Returns true if window is visible on the screen.
 * In case of true it may be obscured by another window nonetheless. */
bool isvisible_window(const window_t * win);

/* function: pos_window
 * Returns the position of the window in screen coordinates.
 * The value (screen_x == 0, screen_y == 0) denotes the top left corner of the screen. */
int pos_window(const window_t * win, /*out*/int32_t * screen_x, /*out*/int32_t * screen_y);

/* function: size_window
 * Returns the width and height of win in pixels. */
int size_window(const window_t * win, /*out*/uint32_t * width, /*out*/uint32_t * height);

// group: update

/* function: show_window
 * Makes window visible to the user.
 * After the event received from the windowing system has been processed
 * the window state is changed to visible - see <isvisible_window>. */
int show_window(window_t * win);

/* function: hide_window
 * Hides win and removes it from the screen.
 * After the event received from the windowing system has been processed
 * the window state is changed to hidden - see <isvisible_window>. */
int hide_window(window_t * win);

/* function: setpos_window
 * Changes the position of the window on the screen.
 * Coordinates are in screen coordinates.
 * The coordinate (0,0) is top left. */
int setpos_window(window_t * win, int32_t screen_x, int32_t screen_y);

/* function: resize_window
 * Changes the size of the window.
 * Parameter width and height must be > 0 else result is undefined. */
int resize_window(window_t * win, uint32_t width, uint32_t height);

/* function: sendclose_window
 * Sends a close request to win. The registered eventhandler receives
 * an <window_evh_t.onclose> event. If no event handler is registered
 * the event is ignored. */
int sendclose_window(window_t * win);

/* function: sendredraw_window
 * Sends a redraw event to win. The registered eventhandler receives
 * an <window_evh_t.onredraw> event. If no event handler is registered
 * the event is ignored. */
int sendredraw_window(window_t * win);



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

#ifdef KONFIG_USERINTERFACE_X11
/* define: display_window
 * Implements <window_t.display_window>. */
#define display_window(win) \
         (castfromos_display(display_x11window(os_window(win))))

/* define: hide_window
 * Implements <window_t.hide_window>. */
#define hide_window(win) \
         hide_x11window(os_window(win))

/* define: isvisible_window
 * Implements <window_t.isvisible_window>. */
#define isvisible_window(win) \
         (x11window_state_SHOWN == state_x11window(os_window(win)))

/* define: pos_window
 * Implements <window_t.pos_window>. */
#define pos_window(win, screen_x, screen_y) \
         pos_x11window(os_window(win), screen_x, screen_y)

/* define: resize_window
 * Implements <window_t.resize_window>. */
#define resize_window(win, width, height) \
         resize_x11window(os_window(win), width, height)

/* define: sendclose_window
 * Implements <window_t.sendclose_window>. */
#define sendclose_window(win) \
         sendclose_x11window(os_window(win))

/* define: sendredraw_window
 * Implements <window_t.sendredraw_window>. */
#define sendredraw_window(win) \
         sendredraw_x11window(os_window(win))

/* define: setpos_window
 * Implements <window_t.setpos_window>. */
#define setpos_window(win, screen_x, screen_y) \
         setpos_x11window(os_window(win), screen_x, screen_y)

/* define: show_window
 * Implements <window_t.show_window>. */
#define show_window(win) \
         show_x11window(os_window(win))

/* define: size_window
 * Implements <window_t.size_window>. */
#define size_window(win, width, height) \
         size_x11window(os_window(win), width, height)

#else
   #error "Not implemented"
#endif

// group: window_evh_t

/* define: genericcast_windowevh
 * Implements <window_evh_t.genericcast_windowevh>. */
#define genericcast_windowevh(evhimpl, subwindow_t)   \
      ( __extension__ ({                              \
         static_assert(                               \
            offsetof(typeof(*(evhimpl)), onclose)     \
               == offsetof(window_evh_t, onclose)     \
            && offsetof(typeof(*(evhimpl)),ondestroy) \
               == offsetof(window_evh_t, ondestroy)   \
            && offsetof(typeof(*(evhimpl)), onredraw) \
               == offsetof(window_evh_t, onredraw)    \
            && offsetof(typeof(*(evhimpl)),onreshape) \
               == offsetof(window_evh_t, onreshape)   \
            && offsetof(typeof(*(evhimpl)),onvisible) \
               == offsetof(window_evh_t, onvisible),  \
            "ensure same structure");                 \
         if (0) {                                     \
            (evhimpl)->onclose((subwindow_t*)0);      \
            (evhimpl)->ondestroy((subwindow_t*)0);    \
            (evhimpl)->onredraw((subwindow_t*)0);     \
            (evhimpl)->onreshape((subwindow_t*)0, (uint32_t)0, (uint32_t)0); \
            (evhimpl)->onvisible((subwindow_t*)0, (bool)0);   \
         }                                            \
         (const window_evh_t*) (evhimpl);             \
      }))

/* define: window_evh_DECLARE
 * Implements <window_evh_t.window_evh_DECLARE>. */
#define window_evh_DECLARE(declared_evh_t, subwindow_t) \
         typedef struct declared_evh_t declared_evh_t;   \
         struct declared_evh_t {                         \
            void (* onclose)     (subwindow_t * win);    \
            void (* ondestroy)   (subwindow_t * win);    \
            void (* onredraw)    (subwindow_t * win);    \
            void (* onreshape)   (subwindow_t * win, uint32_t width, uint32_t height); \
            void (* onvisible)   (subwindow_t * win, bool isVisible);   \
         }


#endif
