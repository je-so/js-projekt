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

   file: C-kern/platform/shared/X11/x11display.c
    Implementation file of <X11-Display impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/platform/X11/x11display.h"
#include "C-kern/api/err.h"
#include "C-kern/api/ds/typeadapt.h"
#include "C-kern/api/ds/inmem/splaytree.h"
#include "C-kern/api/math/int/sign.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/mm/mm_macros.h"
#include "C-kern/api/platform/X11/x11.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif
#include "C-kern/api/platform/X11/x11syskonfig.h"


typedef struct x11display_objectid_adapt_t   x11display_objectid_adapt_t ;


/* struct: x11display_objectid_t
 * Associates Xlib object id (XID) with object pointer.
 *
 * An <oostore_memindex_t> is included to allow this
 * type of structure to be stored in an index of type tree
 * for faster searching.
 * */
struct x11display_objectid_t {
   /* variable: index
    * Index defined on <id>. */
   splaytree_node_t  index ;
   /* variable: id
    * The identification of an object. */
   uintptr_t         id ;
   /* variable: object
    * The pointer to the identified object. */
   void              * object ;
} ;


/* struct: x11display_objectid_adapt_t
 * Memory adapter to allow <splaytree_t> to store and handle objects of type <x11display_objectid_t>. */
struct x11display_objectid_adapt_t {
   typeadapt_EMBED(x11display_objectid_adapt_t, x11display_objectid_t, uintptr_t) ;
} ;

static int impl_delete_objectidadapt(x11display_objectid_adapt_t * typeadp, x11display_objectid_t ** tobject)
{
   int err = 0 ;
   (void) typeadp ;

   if (*tobject) {
      memblock_t memblock = memblock_INIT(sizeof(x11display_objectid_t), (uint8_t*)*tobject) ;
      err = FREE_MM(&memblock) ;

      *tobject = 0 ;
   }

   return err ;
}

static int impl_cmpkeyobj_objectidadapt(x11display_objectid_adapt_t * typeadp, uintptr_t lkey, const x11display_objectid_t * robject)
{
   (void) typeadp ;
   uintptr_t rkey = robject->id ;
   return (lkey < rkey ? -1 : (lkey > rkey ? +1 : 0)) ;
}

static int impl_cmpobj_objectidadapt(x11display_objectid_adapt_t * typeadp, const x11display_objectid_t * lobject, const x11display_objectid_t * robject)
{
   (void) typeadp ;
   uintptr_t lkey = lobject->id ;
   uintptr_t rkey = robject->id ;
   return (lkey < rkey ? -1 : (lkey > rkey ? +1 : 0)) ;
}


// section: x11display_objectid_t

/* Adapts splaytree_t interface to handle <x11display_objectid_t>.
 * They are compared with help of their id.
 * */
splaytree_IMPLEMENT(_objidtree, x11display_objectid_t, uintptr_t/*must be of size (void*)*/, index)

static int find_x11displayobjectid(x11display_objectid_t ** root, uintptr_t objectid, /*out*/x11display_objectid_t ** object)
{
   int err ;
   x11display_objectid_adapt_t typeadp = typeadapt_INIT_CMP(&impl_cmpkeyobj_objectidadapt, &impl_cmpobj_objectidadapt) ;
   splaytree_t                 tree    = splaytree_INIT(&(*root)->index) ;

   err = find_objidtree(&tree, objectid, object, genericcast_typeadapt(&typeadp, x11display_objectid_adapt_t, x11display_objectid_t, uintptr_t)) ;
   getinistate_objidtree(&tree, root) ;

   return err ;
}

static int new_x11displayobjectid(x11display_objectid_t ** root, uintptr_t objectid, void * value_object)
{
   int err ;
   x11display_objectid_adapt_t typeadp = typeadapt_INIT_CMP(&impl_cmpkeyobj_objectidadapt, &impl_cmpobj_objectidadapt) ;
   splaytree_t          tree     = splaytree_INIT(&(*root)->index) ;
   memblock_t           memblock = memblock_INIT_FREEABLE ;

   err = RESIZE_MM(sizeof(x11display_objectid_t), &memblock) ;
   if (err) return err ;

   x11display_objectid_t * new_object ;
   new_object         = (x11display_objectid_t*) memblock.addr ;
   new_object->id     = objectid ;
   new_object->object = value_object ;

   err = insert_objidtree(&tree, new_object, genericcast_typeadapt(&typeadp, x11display_objectid_adapt_t, x11display_objectid_t, uintptr_t)) ;
   getinistate_objidtree(&tree, root) ;

   if (err) {
      (void) FREE_MM(&memblock) ;
   }

   return err ;
}

static int delete_x11displayobjectid(x11display_objectid_t ** root, uintptr_t objectid)
{
   int err ;
   x11display_objectid_adapt_t typeadp = typeadapt_INIT_CMP(&impl_cmpkeyobj_objectidadapt, &impl_cmpobj_objectidadapt) ;
   splaytree_t                 tree    = splaytree_INIT(&(*root)->index) ;
   x11display_objectid_t       * removed_obj ;

   err = find_objidtree(&tree, objectid, &removed_obj, genericcast_typeadapt(&typeadp, x11display_objectid_adapt_t, x11display_objectid_t, uintptr_t)) ;
   if (!err) {
      err = remove_objidtree(&tree, removed_obj, genericcast_typeadapt(&typeadp, x11display_objectid_adapt_t, x11display_objectid_t, uintptr_t)) ;
   }
   getinistate_objidtree(&tree, root) ;

   if (!err) {
      err = impl_delete_objectidadapt(0, &removed_obj) ;
   }

   return err ;
}

static int deleteall_x11displayobjectid(x11display_objectid_t ** root)
{
   int err ;
   x11display_objectid_adapt_t typeadp = typeadapt_INIT_LIFETIME(0, &impl_delete_objectidadapt) ;
   splaytree_t                 tree    = splaytree_INIT(&(*root)->index) ;

   err = free_objidtree(&tree, genericcast_typeadapt(&typeadp, x11display_objectid_adapt_t, x11display_objectid_t, uintptr_t)) ;
   getinistate_objidtree(&tree, root) ;

   return err ;
}


// section: x11display_t

// group: extension support

/* function: initextensions_x11display
 * Initializes extension variables of <x11display_t>.
 * It is expected that memory of all extension variables is set to zero
 * before you call this function. */
static int initextensions_x11display(x11display_t * x11disp)
{
   int   major ;
   int   minor ;
   int   dummy ;
   Bool  isSupported ;

   isSupported = XQueryExtension(x11disp->sys_display, "GLX", &dummy, &x11disp->opengl.eventbase, &x11disp->opengl.errorbase) ;
   if (isSupported) {
      // glX implementation uses functionality supported only by version 1.3 or higher
      isSupported = glXQueryVersion(x11disp->sys_display, &major, &minor) ;
      if (  isSupported
            && major == 1
            && minor >= 3) {
         x11disp->opengl.isSupported   = true ;
         x11disp->opengl.version_major = (uint16_t) major ;
         x11disp->opengl.version_minor = (uint16_t) minor ;
      }
   }

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
   int  err = 0 ;
   int  err2 ;

   if (x11disp->idmap) {
      err2 = deleteall_x11displayobjectid(&x11disp->idmap) ;
      if (err2) err = err2 ;
   }

   if (x11disp->sys_display) {
      err2 = XCloseDisplay(x11disp->sys_display) ;
      x11disp->sys_display = 0 ;
      if (err2) {
         err = ECOMM ;
         TRACESYSERR_LOG("XCloseDisplay", err) ;
      }
   }

   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}

int init_x11display(/*out*/x11display_t * x11disp, const char * display_server_name)
{
   int  err ;
   x11display_t newdisp = x11display_INIT_FREEABLE ;

   if (!display_server_name) {
      display_server_name = getenv("DISPLAY") ;
      if (!display_server_name) {
         TRACEERR_NOARG_LOG(X11_DISPLAY_NOT_SET) ;
         err = EINVAL ;
         goto ONABORT ;
      }
   }

   // create new x11_display

   newdisp.idmap       = 0 ;
   newdisp.sys_display = XOpenDisplay(display_server_name) ;
   if (!newdisp.sys_display) {
      err = ECONNREFUSED ;
      TRACEERR_LOG(X11_NO_CONNECTION, display_server_name) ;
      goto ONABORT ;
   }

#define SETATOM(NAME)   newdisp.atoms.NAME = XInternAtom(newdisp.sys_display, #NAME, False) ; \
                        static_assert(sizeof(newdisp.atoms.NAME) == sizeof(uint32_t), "same type")
   SETATOM(WM_PROTOCOLS) ;
   SETATOM(WM_DELETE_WINDOW) ;
   SETATOM(_NET_FRAME_EXTENTS) ;
   SETATOM(_NET_WM_WINDOW_OPACITY) ;
#undef  SETATOM

   err = initextensions_x11display(&newdisp) ;
   if (err) goto ONABORT ;

   *x11disp = newdisp ;

   return 0 ;
ONABORT:
   free_x11display(&newdisp) ;
   TRACEABORT_LOG(err) ;
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
      PRINTUINT8_LOG(buffer_size) ;
      goto ONABORT ;
   }

   x11_err = XGetErrorText(x11disp->sys_display, x11_errcode, buffer, buffer_size) ;
   if (x11_err) {
      err = EINVAL ;
      TRACESYSERR_LOG("XGetErrorText", err) ;
      PRINTINT_LOG(x11_err) ;
      goto ONABORT ;
   }

   buffer[buffer_size-1] = 0 ;
   return ;
ONABORT:
   if (buffer_size) {
      snprintf(buffer, buffer_size, "%d", x11_errcode) ;
      buffer[buffer_size-1] = 0 ;
   }
   TRACEABORT_LOG(err) ;
   return ;
}

// ID-manager

int findobject_x11display(x11display_t * x11disp, /*out*/void ** object, uintptr_t objectid)
{
   int err ;
   x11display_objectid_t * found_node ;

   err = find_x11displayobjectid(&x11disp->idmap, objectid, &found_node) ;
   if (err) goto ONABORT ;

   *object = found_node->object ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int tryfindobject_x11display(x11display_t * x11disp, /*out*/void ** object, uintptr_t objectid)
{
   int err ;
   x11display_objectid_t * found_node ;

   err = find_x11displayobjectid(&x11disp->idmap, objectid, &found_node) ;

   if (  !err
         && object) {
      *object = found_node->object ;
   }

   return err ;
}

int insertobject_x11display(x11display_t * x11disp, void * object, uintptr_t objectid)
{
   int err ;

   err = new_x11displayobjectid(&x11disp->idmap, objectid, object) ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int removeobject_x11display(x11display_t * x11disp, uintptr_t objectid)
{
   int err ;

   err = delete_x11displayobjectid(&x11disp->idmap, objectid) ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   x11display_t  x11disp  = x11display_INIT_FREEABLE ;
   x11display_t  x11disp2 = x11display_INIT_FREEABLE ;
   pid_t         child    = 1 ;

   // TEST x11display_INIT_FREEABLE
   TEST(x11disp.idmap       == 0) ;
   TEST(x11disp.sys_display == 0) ;
   TEST(x11disp.atoms.WM_PROTOCOLS       == 0) ;
   TEST(x11disp.atoms.WM_DELETE_WINDOW   == 0) ;
   TEST(x11disp.atoms._NET_FRAME_EXTENTS == 0) ;
   TEST(x11disp.atoms._NET_WM_WINDOW_OPACITY == 0) ;

   // TEST init_x11display, free_x11display
   TEST(0 == init_x11display(&x11disp, ":0.0")) ;
   TEST(x11disp.idmap       == 0) ;
   TEST(x11disp.sys_display != 0) ;
   TEST(x11disp.atoms.WM_PROTOCOLS       == XInternAtom(x11disp.sys_display, "WM_PROTOCOLS", False)) ;
   TEST(x11disp.atoms.WM_DELETE_WINDOW   == XInternAtom(x11disp.sys_display, "WM_DELETE_WINDOW", False)) ;
   TEST(x11disp.atoms._NET_FRAME_EXTENTS == XInternAtom(x11disp.sys_display, "_NET_FRAME_EXTENTS", False)) ;
   TEST(x11disp.atoms._NET_WM_WINDOW_OPACITY == XInternAtom(x11disp.sys_display, "_NET_WM_WINDOW_OPACITY", False)) ;
   TEST(0  < io_x11display(&x11disp)) ;
   TEST(0 == free_x11display(&x11disp)) ;
   TEST(0 == x11disp.sys_display) ;
   TEST(0 == free_x11display(&x11disp)) ;
   TEST(0 == x11disp.sys_display) ;

   // TEST init_x11display: 2 different connections
   TEST(0 == init_x11display(&x11disp, ":0.0")) ;
   TEST(x11disp.idmap       == 0) ;
   TEST(x11disp.sys_display != 0) ;
   TEST(x11disp.atoms.WM_PROTOCOLS       == XInternAtom(x11disp.sys_display, "WM_PROTOCOLS", False)) ;
   TEST(x11disp.atoms.WM_DELETE_WINDOW   == XInternAtom(x11disp.sys_display, "WM_DELETE_WINDOW", False)) ;
   TEST(x11disp.atoms._NET_FRAME_EXTENTS == XInternAtom(x11disp.sys_display, "_NET_FRAME_EXTENTS", False)) ;
   TEST(x11disp.atoms._NET_WM_WINDOW_OPACITY == XInternAtom(x11disp.sys_display, "_NET_WM_WINDOW_OPACITY", False)) ;
   TEST(0  < io_x11display(&x11disp)) ;
   TEST(0 == init_x11display(&x11disp2, ":0.0")) ;  // creates new connection
   TEST(x11disp2.idmap       == 0) ;
   TEST(x11disp2.sys_display != 0) ;
   TEST(x11disp2.sys_display != x11disp.sys_display) ;
   TEST(x11disp2.atoms.WM_PROTOCOLS       == XInternAtom(x11disp.sys_display, "WM_PROTOCOLS", False)) ;
   TEST(x11disp2.atoms.WM_DELETE_WINDOW   == XInternAtom(x11disp.sys_display, "WM_DELETE_WINDOW", False)) ;
   TEST(x11disp2.atoms._NET_FRAME_EXTENTS == XInternAtom(x11disp.sys_display, "_NET_FRAME_EXTENTS", False)) ;
   TEST(x11disp2.atoms._NET_WM_WINDOW_OPACITY == XInternAtom(x11disp.sys_display, "_NET_WM_WINDOW_OPACITY", False)) ;
   TEST(io_x11display(&x11disp2) >  0) ;
   TEST(io_x11display(&x11disp2) != io_x11display(&x11disp)) ;
   TEST(0 == free_x11display(&x11disp2)) ;
   TEST(0 == x11disp2.idmap) ;
   TEST(0 == x11disp2.sys_display) ;
   TEST(0 == free_x11display(&x11disp)) ;
   TEST(0 == x11disp.idmap) ;
   TEST(0 == x11disp.sys_display) ;

   child = fork() ;
   if (0 == child) {
      // execute this test in child to make changing environment safe
      // XOpenDisplay has memory leak in case server does not exist

      // TEST init_x11display: ECONNREFUSED
      TEST(ECONNREFUSED == init_x11display(&x11disp, ":9999.0")) ;
      TEST(0 == x11disp.sys_display) ;

      // TEST init_x11display: getenv("DISPLAY")
      if (!getenv("DISPLAY")) {
         TEST(0 == setenv("DISPLAY", ":0", 1)) ;
      }
      TEST(0 == init_x11display(&x11disp, 0 /*use value of getenv("DISPLAY")*/)) ;
      TEST(0 == free_x11display(&x11disp)) ;

      // TEST init_x11display: EINVAL DISPLAY not set
      TEST(0 == unsetenv("DISPLAY")) ;
      TEST(0 == getenv("DISPLAY")) ;
      TEST(EINVAL == init_x11display(&x11disp, 0/*use value of getenv("DISPLAY") => EINVAL*/)) ;
      TEST(0 == x11disp.sys_display) ;

      exit(0) ;
   }

   int childstatus ;
   TEST(child == wait(&childstatus)) ;
   TEST(WIFEXITED(childstatus)) ;
   TEST(0 == WEXITSTATUS(childstatus)) ;

   // TEST io_x11display
   TEST(0 == init_x11display(&x11disp, ":0.0")) ;
   TEST(0  < io_x11display(&x11disp)) ;
   int fd = io_x11display(&x11disp) ;
   TEST(0 == free_x11display(&x11disp)) ;
   TEST(0 == init_x11display(&x11disp, ":0.0")) ;
   TEST(fd == io_x11display(&x11disp)) ;
   TEST(0 == free_x11display(&x11disp)) ;

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

   // TEST isextxrandr_x11display
   x11disp.xrandr.isSupported = true ;
   TEST(true  == isextxrandr_x11display(&x11disp)) ;
   x11disp.xrandr.isSupported = false ;
   TEST(false == isextxrandr_x11display(&x11disp)) ;

   return 0 ;
ONABORT:
   if (!child) exit(1) ;
   (void) free_x11display(&x11disp) ;
   (void) free_x11display(&x11disp2) ;
   return EINVAL ;
}

static int test_extensions(x11display_t * x11disp)
{
   // TEST opengl available / version
   TEST(x11disp->opengl.isSupported) ;
   TEST(1 == x11disp->opengl.version_major) ;
   TEST(3 <= x11disp->opengl.version_minor) ;

   // TEST xrandr available / version
   TEST(x11disp->xdbe.isSupported) ;
   TEST(1 <= x11disp->xdbe.version_major) ;

   // TEST xrandr available / version
   TEST(x11disp->xrandr.isSupported) ;
   TEST(1 <= x11disp->xrandr.version_major) ;

   // TEST xcomposite available / version
   TEST(x11disp->xrender.isSupported) ;
   TEST(1 <= x11disp->xrender.version_major || x11disp->xrender.version_minor > 2) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_id_manager(x11display_t * x11disp1, x11display_t * x11disp2)
{
   x11display_t   copy    = x11display_INIT_FREEABLE ;
   void *         object1 = 0 ;
   void *         object2 = 0 ;

   // TEST insertobject_x11display
   TEST(0 == x11disp1->idmap) ;
   TEST(0 == x11disp2->idmap) ;
   for (uint32_t i = 100; i < 200; ++i) {
      TEST(0 == insertobject_x11display(x11disp1, (void*) (1000 + i), i)) ;
      TEST(0 == insertobject_x11display(x11disp2, (void*) (2000 + i), i)) ;
   }

   // TEST findobject_x11display
   for (uint32_t i = 100; i < 200; ++i) {
      TEST(0 == findobject_x11display(x11disp1, &object1, i)) ;
      TEST(0 == findobject_x11display(x11disp2, &object2, i)) ;
      TEST(object1 == (void*) (1000 + i)) ;
      TEST(object2 == (void*) (2000 + i)) ;
   }

   // TEST removeobject_x11display
   for (uint32_t i = 100; i < 200; ++i) {
      TEST(0 != x11disp1->idmap) ;
      TEST(0 != x11disp2->idmap) ;
      TEST(0 == removeobject_x11display(x11disp1, i)) ;
      TEST(0 == removeobject_x11display(x11disp2, i)) ;
   }
   TEST(0 == x11disp1->idmap) ;
   TEST(0 == x11disp2->idmap) ;

   // TEST ESRCH
   TEST(0 == insertobject_x11display(x11disp1, (void*) 1000, 99)) ;
   TEST(0 == insertobject_x11display(x11disp2, (void*) 2000, 98)) ;
   TEST(ESRCH == removeobject_x11display(x11disp1, 1000)) ;
   TEST(ESRCH == removeobject_x11display(x11disp2, 2000)) ;
   TEST(ESRCH == findobject_x11display(x11disp1, &object1, 98)) ;
   TEST(ESRCH == findobject_x11display(x11disp2, &object2, 99)) ;
   TEST(ESRCH == removeobject_x11display(x11disp1, 98)) ;
   TEST(ESRCH == removeobject_x11display(x11disp2, 99)) ;

   // TEST EEXIST
   TEST(EEXIST == insertobject_x11display(x11disp1, (void*) 1000, 99)) ;
   TEST(EEXIST == insertobject_x11display(x11disp2, (void*) 2000, 98)) ;

   // TEST free_x11display: frees x11disp1->idmap
   for (uint32_t i = 10; i < 20; ++i) {
      TEST(0 == insertobject_x11display(x11disp1, (void*) (100 + i), i)) ;
      TEST(0 == insertobject_x11display(x11disp2, (void*) (200 + i), i)) ;
   }
   TEST(0 != x11disp1->idmap) ;
   TEST(0 != x11disp2->idmap) ;
   copy.idmap = x11disp1->idmap ;
   x11disp1->idmap = 0 ;
   TEST(0 == free_x11display(&copy)) ;
   TEST(0 == copy.idmap) ;
   copy.idmap = x11disp2->idmap ;
   x11disp2->idmap = 0 ;
   TEST(0 == free_x11display(&copy)) ;
   TEST(0 == copy.idmap) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

int unittest_platform_X11_x11display()
{
   x11display_t      x11disp1 = x11display_INIT_FREEABLE ;
   x11display_t      x11disp2 = x11display_INIT_FREEABLE ;
   resourceusage_t   usage    = resourceusage_INIT_FREEABLE ;

   // in GLX extension there is resource leak of approx 24 bytes per init_x11display ? => skip malloc comparison
   // remove glXQueryVersion and no resource leak
   if (test_initfree()) goto ONABORT ;

   TEST(0 == init_x11display(&x11disp1, ":0")) ;
   TEST(0 == init_x11display(&x11disp2, ":0")) ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_extensions(&x11disp1))              goto ONABORT ;
   if (test_id_manager(&x11disp1, &x11disp2))   goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   TEST(0 == free_x11display(&x11disp1)) ;
   TEST(0 == free_x11display(&x11disp2)) ;

   return 0 ;
ONABORT:
   (void) free_x11display(&x11disp1) ;
   (void) free_x11display(&x11disp2) ;
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
