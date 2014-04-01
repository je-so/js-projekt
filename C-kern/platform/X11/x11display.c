/* title: X11-Display impl
   Implements <X11-Display>.

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

#include "C-kern/konfig.h"
#include "C-kern/api/platform/X11/x11display.h"
#include "C-kern/api/err.h"
#include "C-kern/api/math/int/sign.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/mm/mm_macros.h"
#include "C-kern/api/platform/X11/x11.h"
#include "C-kern/api/platform/X11/x11screen.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/platform/task/process.h"
#include "C-kern/api/test/resourceusage.h"
#include "C-kern/api/test/unittest.h"
#endif
#include STR(C-kern/api/platform/KONFIG_OS/graphic/sysx11.h)
#if defined(KONFIG_USERINTERFACE_GLX)
#include STR(C-kern/api/platform/KONFIG_OS/graphic/sysglx.h)
#endif


typedef struct x11windowmap_entry_t x11windowmap_entry_t  ;

typedef struct x11windowmap_t       x11windowmap_t;


/* struct: x11windowmap_entry_t
 * Associates Xlib object id (XID) with object pointer.
 *
 * An <oostore_memindex_t> is included to allow this
 * type of structure to be stored in an index of type tree
 * for faster searching.
 * */
struct x11windowmap_entry_t {
   /* variable: id
    * The identification of an object. */
   uint32_t             id ;
   /* variable: object
    * The pointer to the identified object. */
   struct x11window_t * object ;
} ;


/* struct: x11windowmap_t
 * Stores a set of <x11windowmap_entry_t>.
 * TODO: This simplified implementation will be replaced by a dynamic index structure. */
struct x11windowmap_t {
   x11windowmap_entry_t   entries[16];
};

static int find_x11windowmap(x11windowmap_t * map, uint32_t objectid, /*out*/x11windowmap_entry_t ** object)
{
   if (objectid == 0) {
      return EINVAL;
   }

   for (unsigned i = 0; i < lengthof(map->entries); ++i) {
      if (map->entries[i].id == objectid) {
         *object = &map->entries[i];
         return 0;
      }
   }

   return ESRCH;
}

static int insert_x11windowmap(x11windowmap_t * map, uint32_t objectid, struct x11window_t * object)
{
   unsigned ii = lengthof(map->entries);

   for (unsigned i = 0; i < lengthof(map->entries); ++i) {
      if (map->entries[i].id == objectid) {
         return EEXIST;
      } else if (i < ii && ! map->entries[i].id) {
         ii = i;
      }
   }

   if (ii == lengthof(map->entries)) {
      return ENOMEM;
   }

   map->entries[ii].id     = objectid;
   map->entries[ii].object = object;
   return 0;
}

static int remove_x11windowmap(x11windowmap_t * map, uint32_t objectid)
{
   if (objectid == 0) {
      return EINVAL;
   }

   for (unsigned i = 0; i < lengthof(map->entries); ++i) {
      if (map->entries[i].id == objectid) {
         map->entries[i].id     = 0;
         map->entries[i].object = 0;
         return 0;
      }
   }

   return ESRCH;
}


// section: x11display_t

// group: extension support

/* function: queryextensions_x11display
 * Initializes extension variables of <x11display_t>.
 * It is expected that memory of all extension variables is set to zero
 * before you call this function. */
static int queryextensions_x11display(x11display_t * x11disp)
{
   int   major ;
   int   minor ;
   int   dummy ;
   Bool  isSupported ;

#if defined(KONFIG_USERINTERFACE_GLX)
   isSupported = XQueryExtension(x11disp->sys_display, "GLX", &dummy, &x11disp->glx.eventbase, &x11disp->glx.errorbase) ;
   if (isSupported) {
      // glX implementation uses functionality supported only by version 1.3 or higher
      isSupported = glXQueryVersion(x11disp->sys_display, &major, &minor) ;
      if (  isSupported
            && major == 1
            && minor >= 3) {
         x11disp->glx.isSupported   = true;
         x11disp->glx.version_major = (uint16_t) major;
         x11disp->glx.version_minor = (uint16_t) minor;
      }
   }
#endif

   isSupported = XQueryExtension(x11disp->sys_display, "DOUBLE-BUFFER", &dummy, &x11disp->xdbe.eventbase, &x11disp->xdbe.errorbase) ;
   if (isSupported) {
      isSupported = XdbeQueryExtension(x11disp->sys_display, &major, &minor) ;
      if (isSupported) {
         x11disp->xdbe.isSupported   = true ;
         x11disp->xdbe.version_major = (uint16_t) major ;
         x11disp->xdbe.version_minor = (uint16_t) minor ;
      }
   }

   isSupported = XQueryExtension(x11disp->sys_display, "RANDR", &dummy, &x11disp->xrandr.eventbase, &x11disp->xrandr.errorbase) ;
   if (isSupported) {
      isSupported = XRRQueryVersion(x11disp->sys_display, &major, &minor) ;
      if (isSupported) {
         x11disp->xrandr.isSupported   = true ;
         x11disp->xrandr.version_major = (uint16_t) major ;
         x11disp->xrandr.version_minor = (uint16_t) minor ;

         // prepare receiving events
         for (int i = ScreenCount(x11disp->sys_display); (--i) >= 0 ;) {
            XRRSelectInput(x11disp->sys_display, RootWindow(x11disp->sys_display,i), RRScreenChangeNotifyMask) ;
         }
      }
   }

   isSupported = XQueryExtension(x11disp->sys_display, "RENDER", &dummy, &x11disp->xrender.eventbase, &x11disp->xrender.errorbase) ;
   if (isSupported) {
      isSupported = XRenderQueryVersion(x11disp->sys_display, &major, &minor) ;
      if (isSupported) {
         x11disp->xrender.isSupported   = true ;
         x11disp->xrender.version_major = (uint16_t) major ;
         x11disp->xrender.version_minor = (uint16_t) minor ;
      }
   }

   return 0 ;
}

// group: lifetime

int free_x11display(x11display_t * x11disp)
{
   int err;

   if (x11disp->idmap) {
      memblock_t mblock = memblock_INIT(sizeof(x11windowmap_t), (uint8_t*)x11disp->idmap);
      err = FREE_MM(&mblock);

      if (  x11disp->sys_display
            && XCloseDisplay(x11disp->sys_display)) {
         err = ECOMM;
         TRACESYSCALL_ERRLOG("XCloseDisplay", err);
      }

      x11disp->idmap = 0;
      x11disp->sys_display = 0;

      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORTFREE_ERRLOG(err) ;
   return err ;
}

static int initprivate_x11display(/*out*/x11display_t * x11disp, const char * display_server_name, bool isInitExtension)
{
   int  err ;
   x11display_t   newdisp = x11display_INIT_FREEABLE;
   memblock_t     mblock;

   if (!display_server_name) {
      display_server_name = getenv("DISPLAY") ;
      if (!display_server_name) {
         err = EINVAL ;
         TRACE_NOARG_ERRLOG(log_flags_NONE, X11_DISPLAY_NOT_SET, err) ;
         goto ONABORT ;
      }
   }

   // create new x11_display

   err = ALLOC_MM(sizeof(x11windowmap_t), &mblock);
   if (err) goto ONABORT;

   newdisp.idmap       = (x11windowmap_t*) mblock.addr;
   memset(newdisp.idmap->entries, 0, sizeof(newdisp.idmap->entries));
   newdisp.sys_display = XOpenDisplay(display_server_name);
   if (!newdisp.sys_display) {
      err = ECONNREFUSED ;
      TRACE_ERRLOG(log_flags_NONE, X11_NO_CONNECTION, err, display_server_name) ;
      goto ONABORT ;
   }

#define SETATOM(NAME)   newdisp.atoms.NAME = XInternAtom(newdisp.sys_display, #NAME, False) ; \
                        static_assert(sizeof(newdisp.atoms.NAME) == sizeof(uint32_t), "same type")
   SETATOM(WM_PROTOCOLS) ;
   SETATOM(WM_DELETE_WINDOW) ;
   SETATOM(_NET_FRAME_EXTENTS) ;
   SETATOM(_NET_WM_WINDOW_OPACITY) ;
#undef  SETATOM

   if (isInitExtension) {
      err = queryextensions_x11display(&newdisp) ;
      if (err) goto ONABORT ;
   }

   *x11disp = newdisp ;

   return 0 ;
ONABORT:
   free_x11display(&newdisp) ;
   return err ;
}

int init_x11display(/*out*/x11display_t * x11disp, const char * display_server_name)
{
   int  err ;

   err = initprivate_x11display(x11disp, display_server_name, true);
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

int init2_x11display(/*out*/x11display_t * x11disp, const char * display_server_name, bool isInitExtension)
{
   int  err ;

   err = initprivate_x11display(x11disp, display_server_name, isInitExtension);
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

// group: query

sys_iochannel_t io_x11display(const x11display_t * x11disp)
{
   return ConnectionNumber(x11disp->sys_display) ;
}

void errorstring_x11display(const x11display_t * x11disp, int x11_errcode, char * buffer, uint8_t buffer_size)
{
   int err ;
   int x11_err ;

   if (!buffer_size) {
      err = EINVAL ;
      PRINTUINT8_ERRLOG(buffer_size) ;
      goto ONABORT ;
   }

   x11_err = XGetErrorText(x11disp->sys_display, x11_errcode, buffer, buffer_size) ;
   if (x11_err) {
      err = EINVAL ;
      TRACESYSCALL_ERRLOG("XGetErrorText", err) ;
      PRINTINT_ERRLOG(x11_err) ;
      goto ONABORT ;
   }

   buffer[buffer_size-1] = 0 ;
   return ;
ONABORT:
   if (buffer_size) {
      snprintf(buffer, buffer_size, "%d", x11_errcode) ;
      buffer[buffer_size-1] = 0 ;
   }
   TRACEABORT_ERRLOG(err) ;
   return ;
}

// group: screen

x11screen_t defaultscreen_x11display(x11display_t * x11disp)
{
   return (x11screen_t) x11screen_INIT(x11disp, (uint32_t) DefaultScreen(x11disp->sys_display));
}

uint32_t defaultscreennr_x11display(const x11display_t * x11disp)
{
   return (uint32_t) DefaultScreen(x11disp->sys_display);
}

uint32_t nrofscreens_x11display(const x11display_t * x11disp)
{
   return (uint32_t) ScreenCount(x11disp->sys_display);
}

// ID-manager

int tryfindobject_x11display(x11display_t * x11disp, /*out*/struct x11window_t ** object, uint32_t objectid)
{
   int err ;
   x11windowmap_entry_t * found_node ;

   err = find_x11windowmap(x11disp->idmap, objectid, &found_node) ;
   if (err) goto ONABORT ;

   if (object) {
      *object = found_node->object ;
   }

   return 0 ;
ONABORT:
   if (err != ESRCH) {
      TRACEABORT_ERRLOG(err);
   }
   return err ;
}

int insertobject_x11display(x11display_t * x11disp, struct x11window_t * object, uint32_t objectid)
{
   int err ;

   VALIDATE_INPARAM_TEST(object != 0, ONABORT, );

   err = insert_x11windowmap(x11disp->idmap, objectid, object);
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

int removeobject_x11display(x11display_t * x11disp, uint32_t objectid)
{
   int err ;

   err = remove_x11windowmap(x11disp->idmap, objectid) ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

int replaceobject_x11display(x11display_t * x11disp, struct x11window_t * object, uint32_t objectid)
{
   int err ;
   x11windowmap_entry_t * found_node;

   err = find_x11windowmap(x11disp->idmap, objectid, &found_node) ;
   if (err) goto ONABORT ;

   found_node->object = object;

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   x11display_t  x11disp  = x11display_INIT_FREEABLE ;
   x11display_t  x11disp4 = x11display_INIT_FREEABLE ;
   pid_t         child    = 1 ;

   // TEST x11display_INIT_FREEABLE
   TEST(x11disp.idmap       == 0) ;
   TEST(x11disp.sys_display == 0) ;
   TEST(x11disp.atoms.WM_PROTOCOLS       == 0) ;
   TEST(x11disp.atoms.WM_DELETE_WINDOW   == 0) ;
   TEST(x11disp.atoms._NET_FRAME_EXTENTS == 0) ;
   TEST(x11disp.atoms._NET_WM_WINDOW_OPACITY == 0) ;

   // TEST init2_x11display, free_x11display
   for (int isExtension = 0; isExtension <= 1; ++isExtension) {
      TEST(0 == init2_x11display(&x11disp, ":0.0", (isExtension != 0))) ;
      TEST(x11disp.idmap       != 0) ;
      TEST(x11disp.sys_display != 0) ;
      TEST(x11disp.atoms.WM_PROTOCOLS       == XInternAtom(x11disp.sys_display, "WM_PROTOCOLS", False)) ;
      TEST(x11disp.atoms.WM_DELETE_WINDOW   == XInternAtom(x11disp.sys_display, "WM_DELETE_WINDOW", False)) ;
      TEST(x11disp.atoms._NET_FRAME_EXTENTS == XInternAtom(x11disp.sys_display, "_NET_FRAME_EXTENTS", False)) ;
      TEST(x11disp.atoms._NET_WM_WINDOW_OPACITY == XInternAtom(x11disp.sys_display, "_NET_WM_WINDOW_OPACITY", False)) ;
      TEST(0  < io_x11display(&x11disp)) ;
      TEST(0 == free_x11display(&x11disp)) ;
      TEST(0 == x11disp.idmap) ;
      TEST(0 == x11disp.sys_display) ;
      TEST(0 == free_x11display(&x11disp)) ;
      TEST(0 == x11disp.idmap) ;
      TEST(0 == x11disp.sys_display) ;
   }

   // TEST init_x11display, free_x11display
   TEST(0 == init_x11display(&x11disp, ":0.0")) ;
   TEST(x11disp.idmap       != 0) ;
   TEST(x11disp.sys_display != 0) ;
   TEST(x11disp.atoms.WM_PROTOCOLS       == XInternAtom(x11disp.sys_display, "WM_PROTOCOLS", False)) ;
   TEST(x11disp.atoms.WM_DELETE_WINDOW   == XInternAtom(x11disp.sys_display, "WM_DELETE_WINDOW", False)) ;
   TEST(x11disp.atoms._NET_FRAME_EXTENTS == XInternAtom(x11disp.sys_display, "_NET_FRAME_EXTENTS", False)) ;
   TEST(x11disp.atoms._NET_WM_WINDOW_OPACITY == XInternAtom(x11disp.sys_display, "_NET_WM_WINDOW_OPACITY", False)) ;
   TEST(0  < io_x11display(&x11disp)) ;
   TEST(0 == free_x11display(&x11disp)) ;
   TEST(0 == x11disp.idmap) ;
   TEST(0 == x11disp.sys_display) ;
   TEST(0 == free_x11display(&x11disp)) ;
   TEST(0 == x11disp.idmap) ;
   TEST(0 == x11disp.sys_display) ;

   // TEST init_x11display: 2 different connections
   TEST(0 == init_x11display(&x11disp, ":0.0")) ;
   TEST(x11disp.idmap       != 0) ;
   TEST(x11disp.sys_display != 0) ;
   TEST(x11disp.atoms.WM_PROTOCOLS       == XInternAtom(x11disp.sys_display, "WM_PROTOCOLS", False)) ;
   TEST(x11disp.atoms.WM_DELETE_WINDOW   == XInternAtom(x11disp.sys_display, "WM_DELETE_WINDOW", False)) ;
   TEST(x11disp.atoms._NET_FRAME_EXTENTS == XInternAtom(x11disp.sys_display, "_NET_FRAME_EXTENTS", False)) ;
   TEST(x11disp.atoms._NET_WM_WINDOW_OPACITY == XInternAtom(x11disp.sys_display, "_NET_WM_WINDOW_OPACITY", False)) ;
   TEST(0  < io_x11display(&x11disp)) ;
   TEST(0 == init_x11display(&x11disp4, ":0.0")) ;  // creates new connection
   TEST(x11disp4.idmap       != 0) ;
   TEST(x11disp4.sys_display != 0) ;
   TEST(x11disp4.sys_display != x11disp.sys_display) ;
   TEST(x11disp4.atoms.WM_PROTOCOLS       == XInternAtom(x11disp.sys_display, "WM_PROTOCOLS", False)) ;
   TEST(x11disp4.atoms.WM_DELETE_WINDOW   == XInternAtom(x11disp.sys_display, "WM_DELETE_WINDOW", False)) ;
   TEST(x11disp4.atoms._NET_FRAME_EXTENTS == XInternAtom(x11disp.sys_display, "_NET_FRAME_EXTENTS", False)) ;
   TEST(x11disp4.atoms._NET_WM_WINDOW_OPACITY == XInternAtom(x11disp.sys_display, "_NET_WM_WINDOW_OPACITY", False)) ;
   TEST(io_x11display(&x11disp4) >  0) ;
   TEST(io_x11display(&x11disp4) != io_x11display(&x11disp)) ;
   TEST(0 == free_x11display(&x11disp4)) ;
   TEST(0 == x11disp4.idmap) ;
   TEST(0 == x11disp4.sys_display) ;
   TEST(0 == free_x11display(&x11disp)) ;
   TEST(0 == x11disp.idmap) ;
   TEST(0 == x11disp.sys_display) ;

   return 0 ;
ONABORT:
   if (!child) exit(1) ;
   (void) free_x11display(&x11disp) ;
   (void) free_x11display(&x11disp4) ;
   return EINVAL ;
}

/* function: childprocess_environment
 * Execute this test in child to make changing environment safe.
 * XOpenDisplay has memory leak in case server does not exist. */
static int childprocess_environment(void * dummy)
{
   (void) dummy;
   x11display_t   x11disp = x11display_INIT_FREEABLE;

   // TEST init_x11display: ECONNREFUSED
   TEST(ECONNREFUSED == init_x11display(&x11disp, ":9999.0")) ;
   TEST(0 == x11disp.sys_display) ;

   // TEST init_x11display: getenv("DISPLAY")
   if (!getenv("DISPLAY")) {
      TEST(0 == setenv("DISPLAY", ":0", 1)) ;
   }
   TEST(0 == init_x11display(&x11disp, 0 /*use value of getenv("DISPLAY")*/)) ;
   TEST(0 == free_x11display(&x11disp)) ;

   // TEST init_x11display: EINVAL / DISPLAY not set
   TEST(0 == unsetenv("DISPLAY")) ;
   TEST(0 == getenv("DISPLAY")) ;
   TEST(EINVAL == init_x11display(&x11disp, 0/*use value of getenv("DISPLAY") => EINVAL*/)) ;
   TEST(0 == x11disp.sys_display) ;

   return 0 ;
ONABORT:
   TEST(0 == free_x11display(&x11disp)) ;
   return EINVAL ;
}

static int test_initfree_env(void)
{
   process_t         child = process_INIT_FREEABLE;
   process_result_t  result;

   TEST(0 == init_process(&child, &childprocess_environment, 0, &(process_stdio_t)process_stdio_INIT_INHERIT));
   TEST(0 == wait_process(&child, &result));
   TEST(0 == free_process(&child));
   TEST(process_state_TERMINATED == result.state);
   TEST(0 == result.returncode);

   return 0 ;
ONABORT:
   TEST(0 == free_process(&child));
   return EINVAL ;
}

static int test_query(void)
{
   x11display_t x11disp;

   // TEST isextxrandr_x11display
   x11disp.xrandr.isSupported = true ;
   TEST(true  == isextxrandr_x11display(&x11disp)) ;
   x11disp.xrandr.isSupported = false ;
   TEST(false == isextxrandr_x11display(&x11disp)) ;

   // TEST isfree_x11display
   memset(&x11disp, 255, sizeof(x11disp));
   TEST(0 == isfree_x11display(&x11disp));
   x11disp.idmap = 0;
   TEST(0 == isfree_x11display(&x11disp));
   x11disp.sys_display = 0;
   TEST(1 == isfree_x11display(&x11disp));

   // TEST io_x11display
   TEST(0 == init_x11display(&x11disp, ":0.0"));
   TEST(0 == isfree_x11display(&x11disp));
   TEST(0  < io_x11display(&x11disp));
   int fd = io_x11display(&x11disp);
   TEST(0 == free_x11display(&x11disp));
   TEST(1 == isfree_x11display(&x11disp));

   // TEST io_x11display: same fd
   TEST(0 == init_x11display(&x11disp, ":0.0"));
   TEST(fd == io_x11display(&x11disp));
   TEST(0 == free_x11display(&x11disp));

   // TEST errorstring_x11display: last char is set to 0
   char errstr[100] ;
   char errstr2[100] ;
   memset(errstr, 1, sizeof(errstr)) ;
   memset(errstr2, 1, sizeof(errstr2)) ;
   TEST(0 == init_x11display(&x11disp, ":0.0")) ;
   TEST(sizeof(errstr) == strnlen(errstr, sizeof(errstr))) ;
   errorstring_x11display(&x11disp, 1, errstr, sizeof(errstr)) ;
   uint8_t errlen = (uint8_t) strnlen(errstr, sizeof(errstr)) ;
   TEST(2              < errlen) ;
   TEST(sizeof(errstr) > errlen) ;
   errorstring_x11display(&x11disp, 1, errstr2, errlen) ;
   TEST(0 == strncmp(errstr, errstr2, (uint8_t)(errlen-2))) ;
   TEST(0 != errstr[errlen-1]) ;
   TEST(0 == errstr2[errlen-1]) ;
   memset(errstr, 1, sizeof(errstr)) ;
   TEST(0 == memcmp(errstr, errstr2 + errlen, sizeof(errstr2)-errlen)) ;
   TEST(0 == free_x11display(&x11disp)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_screen(x11display_t * x11disp, x11display_t * x11disp2)
{
   x11display_t * disp[2] = { x11disp, x11disp2 };

   for (unsigned i = 0; i < lengthof(disp); ++i) {
      const uint32_t N = nrofscreens_x11display(disp[i]);
      const uint32_t D = defaultscreennr_x11display(disp[i]);

      // TEST defaultscreen_x11display
      x11screen_t x11screen = defaultscreen_x11display(disp[i]);
      TEST(x11screen.display  == disp[i]);
      TEST(x11screen.nrscreen == D);

      // TEST defaultscreennr_x11display
      TEST(N >  defaultscreennr_x11display(disp[i]));
      TEST(D == defaultscreennr_x11display(disp[i]));

      // TEST nrofscreens_x11display
      TEST(N == nrofscreens_x11display(disp[i]));
      TEST(1 <= nrofscreens_x11display(disp[i]));
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_extensions(x11display_t * x11disp)
{
   x11display_t x11disp_noext = x11display_INIT_FREEABLE;

   // prepare
   TEST(0 == init2_x11display(&x11disp_noext, ":0", false));

   // TEST glx available / version
#if defined(KONFIG_USERINTERFACE_GLX)
   TEST(1 == x11disp->glx.isSupported) ;
   TEST(1 == x11disp->glx.version_major) ;
   TEST(3 <= x11disp->glx.version_minor) ;
#else
   TEST(0 == x11disp->glx.isSupported) ;
   TEST(0 == x11disp->glx.version_major) ;
   TEST(0 == x11disp->glx.version_minor) ;
#endif
   TEST(0 == x11disp_noext.glx.isSupported) ;
   TEST(0 == x11disp_noext.glx.version_major) ;
   TEST(0 == x11disp_noext.glx.version_minor) ;

   // TEST xrandr available / version
   TEST(1 == x11disp->xdbe.isSupported) ;
   TEST(1 <= x11disp->xdbe.version_major) ;
   TEST(0 == x11disp_noext.xdbe.isSupported) ;

   // TEST xrandr available / version
   TEST(1 == x11disp->xrandr.isSupported) ;
   TEST(1 <= x11disp->xrandr.version_major) ;
   TEST(0 == x11disp_noext.xrandr.isSupported) ;

   // TEST xcomposite available / version
   TEST(1 == x11disp->xrender.isSupported) ;
   TEST(1 <= x11disp->xrender.version_major || x11disp->xrender.version_minor > 2) ;
   TEST(0 == x11disp_noext.xrender.isSupported) ;

   // unprepare
   TEST(0 == free_x11display(&x11disp_noext));

   return 0 ;
ONABORT:
   free_x11display(&x11disp_noext);
   return EINVAL ;
}

static int test_id_manager(x11display_t * x11disp1, x11display_t * x11disp2)
{
   struct x11window_t * object1 = 0;
   struct x11window_t * object2 = 0;

   // TEST insertobject_x11display
   for (uint32_t i = 100; i < 100+lengthof(x11disp1->idmap->entries); ++i) {
      TEST(0 == insertobject_x11display(x11disp1, (struct x11window_t*) (1000 + i), i)) ;
      TEST(0 == insertobject_x11display(x11disp2, (struct x11window_t*) (2000 + i), i)) ;
   }

   // TEST tryfindobject_x11display
   for (uint32_t i = 100; i < 100+lengthof(x11disp1->idmap->entries); ++i) {
      TEST(0 == tryfindobject_x11display(x11disp1, &object1, i)) ;
      TEST(0 == tryfindobject_x11display(x11disp2, &object2, i)) ;
      TEST(object1 == (struct x11window_t*) (1000 + i)) ;
      TEST(object2 == (struct x11window_t*) (2000 + i)) ;
   }

   // TEST replaceobject_x11display
   for (uint32_t i = 100; i < 100+lengthof(x11disp1->idmap->entries); ++i) {
      TEST(0 == replaceobject_x11display(x11disp1, (struct x11window_t*) (1001 + i), i)) ;
      TEST(0 == replaceobject_x11display(x11disp2, (struct x11window_t*) (2001 + i), i)) ;
      TEST(0 == tryfindobject_x11display(x11disp1, &object1, i)) ;
      TEST(0 == tryfindobject_x11display(x11disp2, &object2, i)) ;
      TEST(object1 == (struct x11window_t*) (1001 + i)) ;
      TEST(object2 == (struct x11window_t*) (2001 + i)) ;
   }

   // TEST removeobject_x11display
   for (uint32_t i = 100; i < 100+lengthof(x11disp1->idmap->entries); ++i) {
      TEST(0 == removeobject_x11display(x11disp1, i)) ;
      TEST(0 == removeobject_x11display(x11disp2, i)) ;
      TEST(ESRCH == tryfindobject_x11display(x11disp1, &object1, i)) ;
      TEST(ESRCH == tryfindobject_x11display(x11disp2, &object2, i)) ;
   }

   // TEST ESRCH
   TEST(ESRCH == tryfindobject_x11display(x11disp1, &object1, 98)) ;
   TEST(ESRCH == removeobject_x11display(x11disp1, 98)) ;
   TEST(ESRCH == replaceobject_x11display(x11disp1, 0, 98)) ;

   // TEST EINVAL
   TEST(EINVAL == insertobject_x11display(x11disp1, (struct x11window_t*) 0, 99));

   // TEST EEXIST
   TEST(0 == insertobject_x11display(x11disp1, (struct x11window_t*) 1, 99));
   TEST(EEXIST == insertobject_x11display(x11disp1, (struct x11window_t*) 3, 99));
   TEST(0 == removeobject_x11display(x11disp1, 99)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int childprocess_unittest(void)
{
   x11display_t      x11disp1 = x11display_INIT_FREEABLE ;
   x11display_t      x11disp2 = x11display_INIT_FREEABLE ;
   resourceusage_t   usage    = resourceusage_INIT_FREEABLE ;

   // XQueryExtension keeps file descriptors and memory
   // without it no resource leak !
   TEST(0 == init_resourceusage(&usage)) ;
   TEST(0 == init2_x11display(&x11disp1, ":0", false)) ;
   TEST(0 == free_x11display(&x11disp1)) ;
   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   TEST(0 == init_resourceusage(&usage)) ;
   if (test_initfree_env())     goto ONABORT ;
   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   if (test_initfree())    goto ONABORT ;

   TEST(0 == init_x11display(&x11disp1, ":0")) ;
   TEST(0 == init_x11display(&x11disp2, ":0")) ;
   char errstr[100];
   errorstring_x11display(&x11disp1, 1, errstr, sizeof(errstr));

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_query())                            goto ONABORT;
   if (test_screen(&x11disp1, &x11disp2))       goto ONABORT;
   if (test_extensions(&x11disp1))              goto ONABORT;
   if (test_id_manager(&x11disp1, &x11disp2))   goto ONABORT;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   TEST(0 == free_x11display(&x11disp1)) ;
   TEST(0 == free_x11display(&x11disp2)) ;

   return 0 ;
ONABORT:
   (void) free_x11display(&x11disp1);
   (void) free_x11display(&x11disp2);
   (void) free_resourceusage(&usage);
   return EINVAL ;
}

int unittest_platform_X11_x11display()
{
   int err;

   TEST(0 == execasprocess_unittest(&childprocess_unittest, &err));

   return err;
ONABORT:
   return EINVAL;
}

#endif
