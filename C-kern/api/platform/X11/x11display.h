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

   file: C-kern/platform/shared/X11/x11display.c
    Implementation file of <X11-Display impl>.
*/
#ifndef CKERN_PLATFORM_X11_X11DISPLAY_HEADER
#define CKERN_PLATFORM_X11_X11DISPLAY_HEADER

// forward
struct x11display_objectid_t ;

/* typedef: struct x11display_t
 * Export <x11display_t> into global namespace.
 * Describes connection to X11 display server. */
typedef struct x11display_t               x11display_t ;

/* typedef: struct x11display_extension_t
 * Export <x11display_extension_t> into global namespace.
 * Describes an X11 server extension. */
typedef struct x11display_extension_t     x11display_extension_t ;

/* typedef: struct x11display_objectid_t
 * Export <x11display_objectid_t> into global namespace.
 * Associates an id with an object pointer. */
typedef struct x11display_objectid_t      x11display_objectid_t  ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_platform_X11_x11display
 * Tests connection to local X11 display. */
int unittest_platform_X11_x11display(void) ;
#endif


/* struct: x11display_extension_t
 * Stores the version number and the event offsets numbers of an X11 extension.
 * It stores also if the extension is supported. */
struct x11display_extension_t {
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
   struct x11display_objectid_t  * idmap ;
   /* variable: sys_display
    * The X11 display handle of type »Display*«. The generic »void*« type is used to not pollute the
    * global namespace with X11 type names. */
   void                          * sys_display ;
   struct {
         uint32_t                WM_PROTOCOLS ;
         uint32_t                WM_DELETE_WINDOW ;
         uint32_t                _NET_FRAME_EXTENTS ;
         uint32_t                _NET_WM_WINDOW_OPACITY ;
   }                             atoms ;
   /* variable: opengl
    * Check isSupported if OpenGL is supported.
    * The name of the X11 extension which offers an OpenGL binding is "GLX". */
   x11display_extension_t        opengl ;
   /* variable: xdbe
    * Check isSupported if »Double Buffer extension« is supported.
    * The attribute <x11attribute_DOUBLEBUFFER> and function <backbuffer_x11window>
    * work only if this extension is implemented by the X11 server. */
   x11display_extension_t        xdbe ;
   /* variable: xrandr
    * Check isSupported if »X Resize, Rotate and Reflection extension« is supported.
    * The types <x11videomode_iterator_t> and <x11videomode_t>
    * work only if this extension is implemented by the X11 server. */
   x11display_extension_t        xrandr ;
   /* variable: xrender
    * Check isSupported if »X Rendering Extension « is supported.
    * Transparent toplevel windows (as a whole) and alpha blending
    * of single pixels drawn into the window with the underlying
    * window background work only if this extension is implemented
    * by the X11 server.
    *
    * See:
    * <settransparency_glxwindow> and <GLX_ATTRIB_TRANSPARENT>
    * */
   x11display_extension_t        xrender ;
} ;

// group: lifetime

/* define: x11display_INIT_FREEABLE
 * Static initializer. */
#define x11display_INIT_FREEABLE    { .idmap = 0, .sys_display = 0 }

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
 * Returns true if xrandr extension is supported. */
bool isextxrandr_x11display(const x11display_t * x11disp) ;

// group: ID-manager

/* function: findobject_x11display
 * Maps an object id to its associated object pointer.
 * Returns ESRCH if no object exists.
 * On success the returned object contains either a pointer to a valid object or the special
 * value 0 (*NULL*).
 *
 * Event handlers must take care of 0 cause after destroying an X11 object
 * the Display server generates events which indicates this fact or has already generated
 * other events which are not processed yet.
 *
 * An X11 object is only destroyed after the corresponding free_XXX function,
 * e.g. <free_glxwindow>, has been called. Calling free_XXX registers the value 0
 * for the corresponding objectid and a special DestroyNotify_eventhandler then
 * removes the registration with <removeobject_x11display>. */
int findobject_x11display(x11display_t * x11disp, /*out*/void ** object, uintptr_t objectid) ;

/* function: tryfindobject_x11display
 * Checks if an object id is registered.
 * Returns ESRCH if this object id does not exists.
 * In case of success and if object is set to a value != NULL a pointer is returned in object.
 * The function is equal to <findobject_x11display> except that no error logging is done. */
int tryfindobject_x11display(x11display_t * x11disp, /*out*/void ** object/*could be NULL*/, uintptr_t objectid) ;

/* function: insertobject_x11display
 * Registers an object under its corresponding id. */
int insertobject_x11display(x11display_t * x11disp, void * object, uintptr_t objectid) ;

/* function: removeobject_x11display
 * Removes the id and its associated pointer from the registration. */
int removeobject_x11display(x11display_t * x11disp, uintptr_t objectid) ;


// section: inline implementation

// group: x11display_t

/* define: isextxrandr_x11display
 * Implements <x11display_t.isextxrandr_x11display>. */
#define isextxrandr_x11display(x11disp)      \
         ((x11disp)->xrandr.isSupported)

#endif
