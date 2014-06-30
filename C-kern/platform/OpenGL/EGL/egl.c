/* title: EGL-OpenGL-Binding impl

   Implements <EGL-OpenGL-Binding>.

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

   file: C-kern/api/platform/OpenGL/EGL/egl.h
    Header file <EGL-OpenGL-Binding>.

   file: C-kern/platform/OpenGL/EGL/egl.c
    Implementation file <EGL-OpenGL-Binding impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/platform/OpenGL/EGL/egl.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/test/resourceusage.h"
#include "C-kern/api/graphic/gconfig.h"
#include "C-kern/api/platform/OpenGL/EGL/eglconfig.h"
#include "C-kern/api/platform/OpenGL/EGL/eglcontext.h"
#include "C-kern/api/platform/OpenGL/EGL/egldisplay.h"
#include "C-kern/api/platform/OpenGL/EGL/eglpbuffer.h"
#endif
#include STR(C-kern/api/platform/KONFIG_OS/graphic/sysegl.h)



// section: egl_t

// group: query

int nativeerr_egl(void)
{
   return eglGetError();
}

int aserrcode_egl(int eglerr)
{
   switch (eglerr) {
   case EGL_SUCCESS:          return 0;      // The last function succeeded without error.
   case EGL_NOT_INITIALIZED:  return ESTATE; // EGL is not initialized, or could not be initialized, for the specified EGL display connection.
   case EGL_BAD_ACCESS:       return EACCES; // EGL cannot access a requested resource (for example a context is bound in another thread).
   case EGL_BAD_ALLOC:        return EALLOC; // EGL failed to allocate resources for the requested operation.
   case EGL_BAD_ATTRIBUTE:    return EINVAL; // An unrecognized attribute or attribute value was passed in the attribute list.
   case EGL_BAD_CONTEXT:      return EINVAL; // An EGLContext argument does not name a valid EGL rendering context.
   case EGL_BAD_CONFIG:       return EINVAL; // An EGLConfig argument does not name a valid EGL frame buffer configuration
                                             // or does not support the current rendering API.
   case EGL_BAD_CURRENT_SURFACE:return ENODEV; // The current surface of the calling thread is a window, pixel buffer or pixmap that is no longer valid.
   case EGL_BAD_DISPLAY:      return EINVAL; // An EGLDisplay argument does not name a valid EGL display connection.
   case EGL_BAD_SURFACE:      return EINVAL; // An EGLSurface argument does not name a valid surface (window, pixel buffer or pixmap) configured for GL rendering.
   case EGL_BAD_MATCH:        return EINVAL; // Arguments are inconsistent (for example, a valid context requires buffers not supplied by a valid surface).
   case EGL_BAD_PARAMETER:    return EINVAL; // One or more argument values are invalid.
   case EGL_BAD_NATIVE_PIXMAP:return EINVAL; // A NativePixmapType argument does not refer to a valid native pixmap.
   case EGL_BAD_NATIVE_WINDOW:return EINVAL; // A NativeWindowType argument does not refer to a valid native window.
   case EGL_CONTEXT_LOST:     return ERESET; // A power management event has occurred. The application must destroy all contexts and reinitialise OpenGL ES state and objects to continue rendering.
   }

   return EINVAL;    // invalid value in eglerr
}

// group: test

#ifdef KONFIG_UNITTEST

static int test_query(void)
{
   // TEST nativeerr_egl
   TEST(0 == eglTerminate(0));
   TEST(EGL_BAD_DISPLAY == nativeerr_egl());
   // value was reset
   TEST(EGL_SUCCESS == nativeerr_egl());
   TEST(EGL_SUCCESS == nativeerr_egl());

   return 0;
ONERR:
   return EINVAL;
}

static int test_query2(void)
{
   // TEST aserrcode_egl
   TEST(0 == aserrcode_egl(EGL_SUCCESS));
   TEST(ESTATE == aserrcode_egl(EGL_NOT_INITIALIZED));
   TEST(EACCES == aserrcode_egl(EGL_BAD_ACCESS));
   TEST(EALLOC == aserrcode_egl(EGL_BAD_ALLOC));
   TEST(EINVAL == aserrcode_egl(EGL_BAD_ATTRIBUTE));
   TEST(EINVAL == aserrcode_egl(EGL_BAD_CONFIG));
   TEST(EINVAL == aserrcode_egl(EGL_BAD_CONTEXT));
   TEST(ENODEV == aserrcode_egl(EGL_BAD_CURRENT_SURFACE));
   TEST(EINVAL == aserrcode_egl(EGL_BAD_DISPLAY));
   TEST(EINVAL == aserrcode_egl(EGL_BAD_MATCH));
   TEST(EINVAL == aserrcode_egl(EGL_BAD_NATIVE_PIXMAP));
   TEST(EINVAL == aserrcode_egl(EGL_BAD_NATIVE_WINDOW));
   TEST(EINVAL == aserrcode_egl(EGL_BAD_PARAMETER));
   TEST(EINVAL == aserrcode_egl(EGL_BAD_SURFACE));
   TEST(ERESET == aserrcode_egl(EGL_CONTEXT_LOST));

   // TEST aserrcode_egl: parameter out of range
   TEST(EINVAL == aserrcode_egl(0));
   TEST(EINVAL == aserrcode_egl(INT_MAX));
   TEST(EINVAL == aserrcode_egl(INT_MIN));
   TEST(EINVAL == aserrcode_egl(EGL_CONTEXT_LOST+1));
   TEST(EINVAL == aserrcode_egl(EGL_SUCCESS-1));

   return 0;
ONERR:
   return EINVAL;
}

static int childprocess_unittest(void)
{
   resourceusage_t   usage = resourceusage_FREE;

   if (test_query())    goto ONERR;

   TEST(0 == init_resourceusage(&usage));

   if (test_query2())                  goto ONERR;

   TEST(0 == same_resourceusage(&usage));
   TEST(0 == free_resourceusage(&usage));

   return 0;
ONERR:
   (void) free_resourceusage(&usage);
   return EINVAL;
}

int unittest_platform_opengl_egl_egl()
{
   int err;

   TEST(0 == execasprocess_unittest(&childprocess_unittest, &err));

   return err;
ONERR:
   return EINVAL;
}

#endif
