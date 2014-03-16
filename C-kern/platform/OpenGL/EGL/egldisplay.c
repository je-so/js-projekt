/* title: EGL-Display impl

   Implements <EGL-Display>.

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

   file: C-kern/api/platform/OpenGL/EGL/egldisplay.h
    Header file <EGL-Display>.

   file: C-kern/platform/OpenGL/EGL/egldisplay.c
    Implementation file <EGL-Display impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/platform/OpenGL/EGL/egldisplay.h"
#include "C-kern/api/err.h"
#include "C-kern/api/platform/X11/x11display.h"
#include "C-kern/api/test/errortimer.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/test/resourceusage.h"
#endif
#include STR(C-kern/api/platform/KONFIG_OS/graphic/sysegl.h)



// section: egldisplay_t

// group: variable
#ifdef KONFIG_UNITTEST
static test_errortimer_t   s_egldisplay_errtimer = test_errortimer_INIT_FREEABLE ;
#endif

// group: lifetime

/* function: initshared_egldisplay
 * Inits egldisp with display.
 * If the argument display is not either EINVAL or EALLOC
 * is returned. */
static inline int initshared_egldisplay(egldisplay_t * egldisp, EGLDisplay display)
{
   if (  PROCESS_testerrortimer(&s_egldisplay_errtimer)
         || display == EGL_NO_DISPLAY) {
      return EINVAL;
   }

   if (  PROCESS_testerrortimer(&s_egldisplay_errtimer)
         || EGL_FALSE == eglInitialize(display, 0, 0)) {
      // there is no eglFreeDisplay
      return EALLOC;
   }

   *egldisp = display;
   return 0;
}

int initdefault_egldisplay(/*out*/egldisplay_t * egldisp)
{
   int err;
   EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

   err = initshared_egldisplay(egldisp, display);
   if (err) goto ONABORT;

   return 0;
ONABORT:
   TRACEABORT_ERRLOG(err);
   return err;
}

#define KONFIG_x11        1
#if ((KONFIG_USERINTERFACE)&KONFIG_x11)
int initx11_egldisplay(/*out*/egldisplay_t * egldisp, struct x11display_t * x11disp)
{
   int err;

   VALIDATE_INPARAM_TEST(x11disp->sys_display != 0, ONABORT,);

   EGLDisplay display = eglGetDisplay(x11disp->sys_display);

   err = initshared_egldisplay(egldisp, display);
   if (err) goto ONABORT;

   return 0;
ONABORT:
   TRACEABORT_ERRLOG(err);
   return err;
}
#endif
#undef KONFIG_x11

int free_egldisplay(egldisplay_t * egldisp)
{
   int err;

   if (*egldisp) {
      EGLBoolean isTerminate = eglTerminate(*egldisp);
      IFERROR_testerrortimer(&s_egldisplay_errtimer, { isTerminate = EGL_FALSE; });

      // there is no eglFreeDisplay
      *egldisp = 0;

      if (EGL_FALSE == isTerminate) {
         err = EINVAL;
         goto ONABORT;
      }
   }

   return 0;
ONABORT:
   TRACEABORTFREE_ERRLOG(err);
   return err;
}


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   egldisplay_t egldisp = egldisplay_INIT_FREEABLE;
   EGLDisplay   olddisplay = 0;

   // TEST egldisplay_INIT_FREEABLE
   TEST(0 == egldisp);

   // TEST initdefault_egldisplay
   TEST(0 == initdefault_egldisplay(&egldisp));
   TEST(0 != egldisp);
   olddisplay = egldisp;

   // TEST free_egldisplay
   TEST(0 == free_egldisplay(&egldisp));
   TEST(0 == egldisp);
   TEST(0 == free_egldisplay(&egldisp));
   TEST(0 == egldisp);

   // TEST initdefault_egldisplay: returns same display
   TEST(0 == initdefault_egldisplay(&egldisp));
   TEST(0 != egldisp);
   TEST(olddisplay == egldisp);

   // TEST free_egldisplay: error
   init_testerrortimer(&s_egldisplay_errtimer, 1, EINVAL);
   TEST(EINVAL == free_egldisplay(&egldisp));
   TEST(0 == egldisp);

   // TEST initdefault_egldisplay: error
   init_testerrortimer(&s_egldisplay_errtimer, 1, EINVAL);
   TEST(EINVAL == initdefault_egldisplay(&egldisp));
   TEST(0 == egldisp);
   init_testerrortimer(&s_egldisplay_errtimer, 2, EALLOC);
   TEST(EALLOC == initdefault_egldisplay(&egldisp));
   TEST(0 == egldisp);

   return 0;
ONABORT:
   return EINVAL;
}

#define KONFIG_x11        1
#if ((KONFIG_USERINTERFACE)&KONFIG_x11)
static int test_initfree_x11(x11display_t * x11disp)
{
   egldisplay_t   egldisp = egldisplay_INIT_FREEABLE;
   EGLDisplay     olddisplay = 0;

   // TEST initx11_egldisplay: freed x11disp
   TEST(EINVAL == initx11_egldisplay(&egldisp, & (x11display_t) x11display_INIT_FREEABLE));
   TEST(0 == egldisp);

   // TEST initx11_egldisplay, free_egldisplay
   TEST(0 == initx11_egldisplay(&egldisp, x11disp));
   TEST(0 != egldisp);
   olddisplay = egldisp;
   TEST(0 == free_egldisplay(&egldisp));
   TEST(0 == egldisp);

   // TEST initx11_egldisplay: returns same display every time
   TEST(0 == initx11_egldisplay(&egldisp, x11disp));
   TEST(0 != egldisp);
   TEST(olddisplay == egldisp);
   TEST(0 == free_egldisplay(&egldisp));

   // TEST initx11_egldisplay: error
   init_testerrortimer(&s_egldisplay_errtimer, 1, EINVAL);
   TEST(EINVAL == initx11_egldisplay(&egldisp, x11disp));
   TEST(0 == egldisp);
   init_testerrortimer(&s_egldisplay_errtimer, 2, EALLOC);
   TEST(EALLOC == initx11_egldisplay(&egldisp, x11disp));
   TEST(0 == egldisp);

   return 0;
ONABORT:
   return EINVAL;
}

#define INIT_x11display(x11disp, display_server_name) \
         init_x11display(x11disp, display_server_name)

#define FREE_x11display(x11disp) \
         free_x11display(x11disp)

#else

static inline int test_initfree_x11(struct x11display_t * x11disp)
{
   (void) x11disp;
   return 0;
}

#define INIT_x11display(x11disp, display_server_name) \
         (0)

#define FREE_x11display(x11disp) \
         (0)

#endif
#undef KONFIG_x11

static int childprocess_unittest(void)
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE;
   x11display_t      x11disp = x11display_INIT_FREEABLE;

   TEST(0 == INIT_x11display(&x11disp, 0));

   // eglInitialize followed by eglTerminate has a resource leak
   if (test_initfree())             goto ONABORT;
   if (test_initfree_x11(&x11disp)) goto ONABORT;

   TEST(0 == init_resourceusage(&usage));

   // no other tests

   TEST(0 == same_resourceusage(&usage));
   TEST(0 == free_resourceusage(&usage));

   TEST(0 == FREE_x11display(&x11disp));

   return 0;
ONABORT:
   (void) free_resourceusage(&usage);
   (void) FREE_x11display(&x11disp);
   return EINVAL;
}

int unittest_platform_opengl_egl_egldisplay()
{
   int err;

   TEST(0 == execasprocess_unittest(&childprocess_unittest, &err));

   return err;
ONABORT:
   return EINVAL;
}

#endif
