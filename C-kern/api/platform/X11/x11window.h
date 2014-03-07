/* title: X11-Window

   Offers support for displaying 2D output
   in a rectangular area called window on a screen
   served by an X11 display server.

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

   file: C-kern/api/platform/X11/x11window.h
    Header file <X11-Window>.

   file: C-kern/platform/X11/x11window.c
    Implementation file <X11-Window impl>.
*/
#ifndef CKERN_PLATFORM_X11_X11WINDOW_HEADER
#define CKERN_PLATFORM_X11_X11WINDOW_HEADER

// forward
struct cstring_t ;
struct x11attribute_t ;
struct x11display_t ;
struct x11drawable_t ;
struct x11screen_t ;

/* typedef: struct x11window_t
 * Export <x11window_t> into global namespace. */
typedef struct x11window_t             x11window_t ;

/* typedef: struct x11window_it
 * Export <x11window_it> into global namespace. */
typedef struct x11window_it            x11window_it ;

/* enums: x11window_state_e
 * State of a <x11window_t> object.
 *
 * x11window_Destroyed  - The window is destroyed. This state is set if you call <free_x11window> or after
 *                       some other process destroyed the window (for example xkill) and the destroy event is
 *                       handled in the internal dispatch loop.
 * x11window_Hidden     - The window is created but not shown to the user (iconic or minimized state).
 * x11window_Shown      - The window is created and shown to the user but may be only partially visible
 *                       or obscured by another window. */
enum x11window_state_e {
   x11window_Destroyed = 0,
   x11window_Hidden,
   x11window_Shown,
} ;

typedef enum x11window_state_e         x11window_state_e ;

/* enums: x11window_flags_e
 * Additional flags of an object of type <x11window_t> or its subtype.
 *
 * x11window_OwnWindow     - The window is owned by this object. Freeing this object also frees the system window.
 * x11window_OwnColormap   - The colormap is owned by this object. Freeing this object also frees the system colormap.
 * */
enum x11window_flags_e {
   x11window_OwnWindow     = 1,
   x11window_OwnColormap   = 2,
} ;

typedef enum x11window_flags_e         x11window_flags_e ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_platform_X11_x11window
 * Test <x11window_t> functionality. */
int unittest_platform_X11_x11window(void) ;
#endif


/* struct: x11window_it
 * Callback interface for X11 events. */
struct x11window_it {
   /* variable: onclose
    * The event handler is called if the user requested to close the window. You can save state information before calling <free_x11window>. */
   void (* onclose)        (x11window_t * x11win);
   /* variable: ondestroy
    * The event handler is called if the window was destroyed by another process.
    * The <x11window_t.state> is set to <x11window_Destroyed> before this callback is called.
    * You must call <free_x11window> - any other function which uses <x11window_t.sys_drawable> as parameter
    * would cause an Xlib error which would cause the process to abort. Not calling <free_x11window> results in
    * a memory leak. Calling <free_x11window> in response to an <onclose> event does not trigger a <ondestroy> callback. */
   void (* ondestroy)      (x11window_t * x11win);
   /* variable: onredraw
    * The event handler is called if the window was (partially) obscured and the obscured content has to be onredrawn. */
   void (* onredraw)       (x11window_t * x11win);
   /* variable: onreshape
    * The event handler is called whenever the geometry of the window changes.
    * The x and y coordinates can be queried for with a call to <pos_x11window>. */
   void (* onreshape)      (x11window_t * x11win, uint32_t width, uint32_t height);
   /* variable: onvisible
    * The event handler is called if the window changes state from <x11window_Shown> to <x11window_Hidden> or vice versa.
    * Use <x11window_t.state_x11window> to get the current state. */
   void (* onvisible)      (x11window_t * x11win);
} ;

// group: generic

/* define: x11window_it_INIT
 * Static initializer. */
#define x11window_it_INIT(subwindow_fct_suffix) {  \
            & onclose ## subwindow_fct_suffix,     \
            & ondestroy ## subwindow_fct_suffix,   \
            & onredraw ## subwindow_fct_suffix,    \
            & onreshape ## subwindow_fct_suffix,   \
            & onvisible ## subwindow_fct_suffix    \
         }

/* function: genericcast_x11windowit
 * Casts parameter iimpl into pointer to interface <x11window_it>.
 * The parameter iimpl has to be of type pointer to type declared with <x11window_it_DECLARE>.
 * The other parameters must be the same as in <x11window_it_DECLARE> without the first. */
const x11window_it * genericcast_x11windowit(const void * iimpl, TYPENAME subwindow_t) ;

/* function: x11window_it_DECLARE
 * Declares an interface function table for getting notified if window an event has occurred.
 * The declared interface is structural compatible with <x11window_it>.
 * See <x11window_it> for a list of declared functions.
 *
 * Parameter:
 * declared_it - The name of the structure which is declared as the interface.
 *               The name should have the suffix "_it".
 * subwindow_t - The window subtype for which implements the callback functions. */
void x11window_it_DECLARE(TYPENAME declared_it, TYPENAME subwindow_t) ;


/* struct: x11window_t
 * Displays a window (rectangular area) on a screen.
 * The window can have a frame and title bar which is draw and managed by the window manager.
 * Add <x11attribute_INIT_WINFRAME> to your configuration list before calling <init_x11window>
 * to add a frame and title bar. Use <x11attribute_INIT_WINTITLE> to name the title bar. */
struct x11window_t {
   /* variable: display
    * Reference to <x11display_t>. Every call to X library needs this parameter. */
   struct x11display_t* display ;
   /* variable: sys_drawable
    * X window ID. The ID describes a drawable of type window. */
   uint32_t             sys_drawable ;
   /* variable: sys_colormap
    * X window ID. The ID describes a colormap which is associated with the window.
    * A colormap is used to map the window pixel depth to the screen pixel depth. */
   uint32_t             sys_colormap ;
   /* variable: iimpl
    * Reference to <x11window_it> which handles events. */
   const x11window_it * iimpl ;
   /* variable: state
    * Current state of window (shown, hidden, destroyed). See <x11window_state_e>. */
   uint8_t              state ;
   /* variable: flags
    * Additional flags inidicating ownership of system objects. See <x11window_flags_e>. */
   uint8_t              flags ;
} ;

// group: lifetime

/* define: x11window_INIT_FREEABLE
 * Static initializer. */
#define x11window_INIT_FREEABLE        { 0, 0, 0, 0, 0, 0 }

/* function: init_x11window
 * Create a new window on x11screen and assign it to x11win.
 * After successful return <state_x11window> returns <x11window_Hidden>.
 * Call <show_x11window> to show the window to the user. */
int init_x11window(/*out*/x11window_t * x11win, struct x11screen_t * x11screen, const struct x11window_it * eventhandler, uint8_t nrofattributes, const struct x11attribute_t * configuration/*[nrofattributes]*/) ;

/* function: initsys_x11window
 * Use this call from a subtype.
 * This parameter visual points to Visual (X11 type) and depth specifies the windows depth.
 * Any unknown <x11attribute_t.name> is ignored. Uses XCreateWindow for its implementation. */
int initsys_x11window(/*out*/x11window_t * x11win, const struct x11window_it * eventhandler, struct x11display_t * x11disp, uint32_t parent_sys_drawable,
                           /*Visual*/void * visual, int depth, uint8_t nrofattributes, const struct x11attribute_t * configuration/*[nrofattributes]*/);

/* function: initmove_glxwindow
 * Must be called if address of <x11window_t> changes.
 * A simple memcpy from source to destination does not work.
 * *Not implemented*. */
int initmove_glxwindow(/*out*/x11window_t * dest_x11win, x11window_t * src_x11win) ;

/* function: free_x11window
 * Frees all associated resources in case <w11window_t.flags>  inidcates ownership.
 * If the object does not own the system resources they are not freed. You can use this
 * to wrap system windows (desktop window for example) into an <x11window_t> object. */
int free_x11window(x11window_t * x11win) ;

// group: query

/* function: screen_x11window
 * Returns the screen the window is located on. */
struct x11screen_t screen_x11window(const x11window_t * x11win) ;

/* function: flags_x11window
 * Returns flags which indicate ownership state of the window as seen by the user. See <x11window_state_e>.
 * After calling <init_x11window> the state is set to <x11window_Hidden>. Call * */
x11window_flags_e flags_x11window(const x11window_t * x11win) ;

/* function: state_x11window
 * Returns the state of the window as seen by the user. See <x11window_state_e>.
 * After calling <init_x11window> the state is set to <x11window_Hidden>. */
x11window_state_e state_x11window(const x11window_t * x11win) ;

/* function: title_x11window
 * Returns the window title string in title encoded in UTF-8. */
int title_x11window(const x11window_t * x11win, /*ret*/struct cstring_t * title) ;

/* function: pos_x11window
 * Returns the position of the window in screen coordinates.
 * The value (screen_x == 0, screen_y == 0) denotes the top left corner of the screen. */
int pos_x11window(const x11window_t * x11win, /*out*/int32_t * screen_x, /*out*/int32_t * screen_y) ;

/* function: size_x11window
 * Returns the width and height of the window in pixels. */
int size_x11window(const x11window_t * x11win, /*out*/uint32_t * width, /*out*/uint32_t * height) ;

/* function: geometry_x11window
 * This is the geometry of window without the window manager frame in screen coordinates.
 * The value (screen_x == 0, screen_y == 0) denotes the top left corner of the screen. */
int geometry_x11window(const x11window_t * x11win, /*out*/int32_t * screen_x, /*out*/int32_t * screen_y, /*out*/uint32_t * width, /*out*/uint32_t * height) ;

/* function: frame_x11window
 * This is the geometry of window including the window manager frame in screen coordinates.
 * The value (screen_x == 0, screen_y == 0) denotes the top left corner of the screen. */
int frame_x11window(const x11window_t * x11win, /*out*/int32_t * screen_x, /*out*/int32_t * screen_y, /*out*/uint32_t * width, /*out*/uint32_t * height) ;

// group: update

/* function: show_x11window
 * Makes window visible to the user. In X11 speak it is mapped.
 * After receiveing the notification from the X server the window state is set to <x11window_Shown>. */
int show_x11window(x11window_t * x11win) ;

/* function: hide_x11window
 * In X11 terms unmaps a window: makes it invisible to the user.
 * After receiveing the notification from the X server the window state is set to <x11window_Hidden>. */
int hide_x11window(x11window_t * x11win) ;

/* function: setpos_x11window
 * Changes the position of the window on the screen.
 * Coordinates are in screen coordinates.
 * The coordinate (0,0) is top left. */
int setpos_x11window(x11window_t * x11win, int32_t screen_x, int32_t screen_y) ;

/* function: resize_x11window
 * Changes the size of the window.
 * Parameter width and height must be > 0 else result is undefined. */
int resize_x11window(x11window_t * x11win, uint32_t width, uint32_t height) ;

/* function: sendclose_x11window
 * Sends a close request to a window. If the window has installed an event handler it should
 * free (destroy) the window after having received the request. */
int sendclose_x11window(x11window_t * x11win) ;

/* function: sendredraw_x11window
 * Sends a redraw event to a window. If no event handler is installed nothing is received. */
int sendredraw_x11window(x11window_t * x11win) ;

/* function: settitle_x11window
 * Sets the text of the window titile bar. This function assumes title to be encoded in UTF-8. */
int settitle_x11window(const x11window_t * x11win, const char * title) ;

/* function: setopacity_x11window
 * Sets the opacity of the window and its window manager frame.
 * An opacity value of 1 draws the window opaque.
 * A value of 0 makes it totally translucent.
 * If opacity is set to a value outside of [0..1] EINVAL is returned.
 * The opacity is different from <x11attribute_ALPHAOPACITY>.
 * A value of 0 makes the window invisible.
 *
 * Wheras an alpha value of 0 in case of <x11attribute_ALPHAOPACITY>
 * does contribute by adding it to the background value.
 *
 * Blending Function:
 * > Screen-Pixel-RGB = (Window-Pixel-Alpha) * Window-Pixel-RGB + (1 - Window-Pixel-Alpha) * Background-Pixel-RGB
 *
 * Precondition:
 * For the function to work the X11 server and window manager must
 * support the X11 composite extension. */
int setopacity_x11window(x11window_t * x11win, double opacity) ;

/* function: swapbuffer_x11window
 * Changes the content of the double buffer with the window.
 * Do not call this function if no <x11dblbuffer_t> has been allocated. */
int swapbuffer_x11window(x11window_t * x11win);

// group: generic

/* function: genericcast_x11window
 * Casts a pointer to object into a pointer to <x11window_t>.
 * All fields of object must be compatible with x11window_t and at the same offset. */
x11window_t * genericcast_x11window(void * object) ;


// section: inline implementation

// group: x11window_it

/* define: genericcast_x11windowit
 * Implements <x11window_it.genericcast_x11windowit>. */
#define genericcast_x11windowit(iimpl, subwindow_t)   \
      ( __extension__ ({                                                                     \
         static_assert(                                                                      \
            offsetof(typeof(*(iimpl)), onclose)      == offsetof(x11window_it, onclose)      \
            && offsetof(typeof(*(iimpl)), ondestroy) == offsetof(x11window_it, ondestroy)    \
            && offsetof(typeof(*(iimpl)), onredraw)  == offsetof(x11window_it, onredraw)     \
            && offsetof(typeof(*(iimpl)), onreshape) == offsetof(x11window_it, onreshape)    \
            && offsetof(typeof(*(iimpl)), onvisible)  == offsetof(x11window_it, onvisible),  \
            "ensure same structure");                                                        \
         if (0) {                                                                            \
            (iimpl)->onclose((subwindow_t*)0);                                               \
            (iimpl)->ondestroy((subwindow_t*)0);                                             \
            (iimpl)->onredraw((subwindow_t*)0);                                              \
            (iimpl)->onreshape((subwindow_t*)0, (uint32_t)0, (uint32_t)0);                   \
            (iimpl)->onvisible((subwindow_t*)0);                                             \
         }                                                                                   \
         (x11window_it*) (iimpl);                                                            \
      }))

/* define: x11window_it_DECLARE
 * Implements <x11window_it.x11window_it_DECLARE>. */
#define x11window_it_DECLARE(declared_it, subwindow_t) \
         typedef struct declared_it    declared_it ;        \
         struct declared_it {                               \
            void (* onclose)     (subwindow_t * x11win);    \
            void (* ondestroy)   (subwindow_t * x11win);    \
            void (* onredraw)    (subwindow_t * x11win);    \
            void (* onreshape)   (subwindow_t * x11win, uint32_t width, uint32_t height); \
            void (* onvisible)   (subwindow_t * x11win);    \
         }

// group: x11window_t

/* define: flags_x11window
 * Implements <x11window_t.flags_x11window>. */
#define flags_x11window(x11win) \
         ((x11window_flags_e)(x11win)->flags)

/* define: pos_x11window
 * Implements <x11window_t.pos_x11window>. */
#define pos_x11window(x11win, screen_x, screen_y) \
         (geometry_x11window(x11win, screen_x, screen_y, 0, 0))

/* define: size_x11window
 * Implements <x11window_t.size_x11window>. */
#define size_x11window(x11win, width, height) \
         (geometry_x11window(x11win, 0, 0, width, height))

/* define: state_x11window
 * Implements <x11window_t.state_x11window>. */
#define state_x11window(x11win) \
         ((x11window_state_e)(x11win)->state)

/* define: genericcast_x11window
 * Implements <x11window_t.genericcast_x11window>. */
#define genericcast_x11window(object) \
         ( __extension__ ({                                                                     \
            static_assert(                                                                      \
               offsetof(typeof(*(object)), display) == offsetof(x11window_t, display)           \
               && offsetof(typeof(*(object)), sys_drawable) == offsetof(x11window_t, sys_drawable)  \
               && offsetof(typeof(*(object)), sys_colormap) == offsetof(x11window_t, sys_colormap) \
               && offsetof(typeof(*(object)), iimpl) == offsetof(x11window_t, iimpl)            \
               && offsetof(typeof(*(object)), state) == offsetof(x11window_t, state)            \
               && offsetof(typeof(*(object)), flags) == offsetof(x11window_t, flags)            \
               && sizeof((object)->flags) == sizeof(((x11window_t*)0)->flags),                  \
               "ensure same structure") ;                                                       \
            (x11window_t *) (object) ;                                                          \
         }))

#endif
