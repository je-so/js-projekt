/* title: X11-Display
   Handles access to a X11 display server.
   To create X11 windows and to do some graphics operations
   a connection to a X11 display server is needed.
   Before any other function in the X11 subsystem can be used
   call <init_x11display> to establish a connection to an X11
   display server.

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

   file: C-kern/api/platform/X11/x11display.h
    Header file of <X11-Display>.

   file: C-kern/platform/X11/x11display.c
    Implementation file of <X11-Display impl>.
*/
#ifndef CKERN_PLATFORM_X11_X11DISPLAY_HEADER
#define CKERN_PLATFORM_X11_X11DISPLAY_HEADER

// forward
struct x11screen_t;
struct x11window_t;
struct x11windowmap_t;

/* typedef: struct x11display_t
 * Export <x11display_t> into global namespace.
 * Describes connection to X11 display server. */
typedef struct x11display_t   x11display_t ;

/* typedef: struct x11extension_t
 * Export <x11extension_t> into global namespace.
 * Describes an X11 server extension. */
typedef struct x11extension_t x11extension_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_platform_X11_x11display
 * Tests connection to local X11 display. */
int unittest_platform_X11_x11display(void) ;
#endif


/* struct: x11extension_t
 * Stores the version number and the event offsets numbers of an X11 extension.
 * It stores also if the extension is supported. */
struct x11extension_t {
   uint16_t    version_major ;
   uint16_t    version_minor ;
   int         errorbase ;
   int         eventbase ;
   bool        isSupported ;
} ;


/* struct: x11display_t
 * Describes connection to an X11 display server.
 * If more then one thread wants to access a display
 * every thread must create its own <x11display_t>.
 *
 * Not thread safe:
 * The functions on a display object are not thread safe.
 * The underlying X11 library is initialized to be thread safe
 * so that accessing core X11 directly via <sys_display> is thread safe.
 *
 * A display normally corresponds to a graphics card. */
struct x11display_t {
   /* variable: idmap
    * Used internally to map an id to an object pointer. */
   struct x11windowmap_t * idmap;
   /* variable: sys_display
    * The X11 display handle of type »Display*«. The generic »void*« type is used to not pollute the
    * global namespace with X11 type names. */
   void               * sys_display;
   struct {
         uint32_t    WM_PROTOCOLS;
         uint32_t    WM_DELETE_WINDOW;
         uint32_t    _NET_FRAME_EXTENTS;
         uint32_t    _NET_WM_WINDOW_OPACITY;
   }                    atoms;
   /* variable: glx
    * Check isSupported whether glx is supported.
    * The name of the X11 extension which offers an OpenGL binding is "GLX". */
   x11extension_t       glx;
   /* variable: xdbe
    * Check isSupported whether »Double Buffer extension« is supported.
    * <x11dblbuffer_t> works only if this extension is supported. */
   x11extension_t       xdbe;
   /* variable: xrandr
    * Check isSupported whether »X Resize, Rotate and Reflection extension« is supported.
    * The types <x11videomode_iterator_t> and <x11videomode_t>
    * work only if this extension is implemented by the X11 server. */
   x11extension_t       xrandr;
   /* variable: xrender
    * Check isSupported whether »X Rendering Extension « is supported.
    * Transparent toplevel windows (as a whole) and alpha blending
    * of single pixels drawn into the window with the underlying
    * window background work only if this extension is implemented
    * by the X11 server.
    *
    * See:
    * <settransparency_glxwindow> and <GLX_ATTRIB_TRANSPARENT>
    * */
   x11extension_t       xrender;
} ;

// group: lifetime

/* define: x11display_FREE
 * Static initializer. */
#define x11display_FREE { .idmap = 0, .sys_display = 0 }

/* function: init_x11display
 * Connects to a X11 display server and returns the newly created connection.
 * The server can be located on the same local node or
 * on a remote node reachable through a tcp/ip network.
 * If the parameter is set to 0 the default connection is used.
 * The default is read from the environment variable "DISPLAY" which is normally set to ":0.0".
 *
 * Syntax of display_server_name:
 * > [ <ip-number> | <dns-server-name> | "" ] ":" <display-number> "." <default-screen-number>
 * An empty server name connects to the local host with the fastest possible type of connection.
 *
 * Do not share connections:
 * Every thread must have its own connection to a X11 graphics display. */
int init_x11display(/*out*/x11display_t * x11disp, const char * display_server_name) ;

/* function: init2_x11display
 * Same as <init_x11display> but allows to not initialize any X11 extension. */
int init2_x11display(/*out*/x11display_t * x11disp, const char * display_server_name, bool isInitExtension);

/* function: free_x11display
 * Closes display connection and frees all resources. */
int free_x11display(x11display_t * x11disp) ;

/* function: initmove_x11display
 * Must be called if address of <x11display_t> changes.
 * A simple memcpy from source to destination does not work.
 *
 * Not Implemented. */
int initmove_x11display(/*out*/x11display_t * destination, x11display_t * source) ;

// group: query

/* function: io_x11display
 * Returns file descriptor of network connection.
 * You can use it to wait for incoming events sent by X11 display server. */
sys_iochannel_t io_x11display(const x11display_t * x11disp) ;

/* function: errorstring_x11display
 * Returns a NULL terminated name of the error with code »x11_errcode« in plain english.
 * In case of an internal error the number of x11_errcode is returned. */
void errorstring_x11display(const x11display_t * x11disp, int x11_errcode, char * buffer, uint8_t buffer_size) ;

/* function: isextxrandr_x11display
 * Returns true if xrandr extension is supported.
 * This extension supports querying and setting different video modes
 * (screen resolutions) - see <x11videomode_t>. */
bool isextxrandr_x11display(const x11display_t * x11disp) ;

/* function: isfree_x11display
 * Returns true if x11disp is set to <x11display_FREE>. */
static inline bool isfree_x11display(const x11display_t * x11disp);

// group: screen

/* function: defaultscreen_x11display
 * Returns the default screen of x11disp. */
struct x11screen_t defaultscreen_x11display(x11display_t * x11disp);

/* function: defaultscreennr_x11display
 * Returns the default screen number of x11disp. */
uint32_t defaultscreennr_x11display(const x11display_t * x11disp);

/* function: nrofscreens_x11display
 * Returns the number of all screens attached to x11disp.
 * The first screen has the number 0 and the last <nrofscreens_x11display>-1. */
uint32_t nrofscreens_x11display(const x11display_t * x11disp);

// group: ID-manager

/* function: tryfindobject_x11display
 * Maps an objectid to its associated object pointer.
 * Returns ESRCH if no object is registered with objectid.
 * On success the returned object contains either a pointer to an object which
 * could be 0. No error logging is done in case of error ESRCH. */
int tryfindobject_x11display(x11display_t * x11disp, /*out*/struct x11window_t ** object/*could be NULL*/, uint32_t objectid) ;

/* function: insertobject_x11display
 * Registers an object under an objectid. */
int insertobject_x11display(x11display_t * x11disp, struct x11window_t * object, uint32_t objectid) ;

/* function: removeobject_x11display
 * Removes objectid and its associated pointer from the registration.
 * This function is called from <free_x11window> or from <dispatchevent_X11>
 * in case a DestroyNotify for a registerd window was received. */
int removeobject_x11display(x11display_t * x11disp, uint32_t objectid);

/* function: replaceobject_x11display
 * Replaces the object for an already registered objectid. */
int replaceobject_x11display(x11display_t * x11disp, struct x11window_t * object, uint32_t objectid) ;


// section: inline implementation

// group: x11display_t

/* define: isextxrandr_x11display
 * Implements <x11display_t.isextxrandr_x11display>. */
#define isextxrandr_x11display(x11disp)      \
         ((x11disp)->xrandr.isSupported)

/* define: isfree_x11display
 * Implements <x11display_t.isfree_x11display>. */
static inline bool isfree_x11display(const x11display_t * x11disp)
{
   return 0 == x11disp->idmap && 0 == x11disp->sys_display;
}

#endif
