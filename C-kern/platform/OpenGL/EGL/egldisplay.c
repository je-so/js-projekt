/* title: EGL-Display impl

   Implements <EGL-Display>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

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
#include "C-kern/api/platform/OpenGL/EGL/egl.h"
#include "C-kern/api/platform/X11/x11display.h"
#include "C-kern/api/test/errortimer.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/test/resourceusage.h"
#endif
#include STR(C-kern/api/platform/KONFIG_OS/graphic/sysegl.h)



// section: egldisplay_t

// group: static variables

#ifdef KONFIG_UNITTEST
/* variable: s_egldisplay_errtimer
 * Allows to introduce artificial errors during test. */
static test_errortimer_t   s_egldisplay_errtimer = test_errortimer_FREE;
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
   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int init_egldisplay(/*out*/egldisplay_t * egldisp, struct sys_display_t * sysdisp)
{
   int err;

   VALIDATE_INPARAM_TEST(sysdisp != 0, ONERR,);

   EGLDisplay display = eglGetDisplay((EGLNativeDisplayType)sysdisp);

   err = initshared_egldisplay(egldisp, display);
   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int free_egldisplay(egldisplay_t * egldisp)
{
   int err;

   if (*egldisp) {
      EGLBoolean isTerminate = eglTerminate(*egldisp);

      *egldisp = 0;

      if (EGL_FALSE == isTerminate) {
         err = aserrcode_egl(eglGetError());
         goto ONERR;
      }

      ONERROR_testerrortimer(&s_egldisplay_errtimer, &err, ONERR);
   }

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(sys_display_t * sysdisp, sys_display_t * freesysdisp)
{
   egldisplay_t egldisp = egldisplay_FREE;
   EGLDisplay   olddisplay = 0;

   // TEST egldisplay_FREE
   TEST(0 == egldisp);

   // TEST init_egldisplay: freed native display
   TEST(EINVAL == init_egldisplay(&egldisp, freesysdisp));
   TEST(0 == egldisp);

   // TEST init_egldisplay, free_egldisplay
   TEST(0 == init_egldisplay(&egldisp, sysdisp));
   TEST(0 != egldisp);
   olddisplay = egldisp;
   TEST(0 == free_egldisplay(&egldisp));
   TEST(0 == egldisp);

   // TEST init_egldisplay: returns same display every time
   TEST(0 == init_egldisplay(&egldisp, sysdisp));
   TEST(0 != egldisp);
   TEST(olddisplay == egldisp);
   TEST(0 == free_egldisplay(&egldisp));

   // TEST init_egldisplay: error
   init_testerrortimer(&s_egldisplay_errtimer, 1, EINVAL);
   TEST(EINVAL == init_egldisplay(&egldisp, sysdisp));
   TEST(0 == egldisp);
   init_testerrortimer(&s_egldisplay_errtimer, 2, EALLOC);
   TEST(EALLOC == init_egldisplay(&egldisp, sysdisp));
   TEST(0 == egldisp);

   return 0;
ONERR:
   return EINVAL;
}

static int test_initfree_default(void)
{
   egldisplay_t egldisp = egldisplay_FREE;
   EGLDisplay   olddisplay = 0;

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

   // TEST free_egldisplay: ERROR
   init_testerrortimer(&s_egldisplay_errtimer, 1, ENODATA);
   TEST(ENODATA == free_egldisplay(&egldisp));
   TEST(0 == egldisp);

   // TEST initdefault_egldisplay: ERROR
   init_testerrortimer(&s_egldisplay_errtimer, 1, EINVAL);
   TEST(EINVAL == initdefault_egldisplay(&egldisp));
   TEST(0 == egldisp);
   init_testerrortimer(&s_egldisplay_errtimer, 2, EALLOC);
   TEST(EALLOC == initdefault_egldisplay(&egldisp));
   TEST(0 == egldisp);

   return 0;
ONERR:
   return EINVAL;
}

#if defined(KONFIG_USERINTERFACE_X11)

#define osdisplay_t x11display_t

#define osdisplay_FREE x11display_FREE

#define init_osdisplay(osdisp, display_server_name) \
         init_x11display(osdisp, display_server_name)

#define free_osdisplay(osdisp) \
         free_x11display(osdisp)

#define sysdisplay_osdisplay(osdisp) \
         sysdisplay_x11display(osdisp)

#else

   #error "Not implemented"

#endif

static int childprocess_unittest(void)
{
   resourceusage_t   usage = resourceusage_FREE;
   osdisplay_t       osdisp = osdisplay_FREE;
   struct sys_display_t * sysdisp = sysdisplay_osdisplay(&osdisp);
   struct sys_display_t * freesysdisp = sysdisplay_osdisplay(&osdisp);

   TEST(0 == init_osdisplay(&osdisp, 0));
   sysdisp = sysdisplay_osdisplay(&osdisp);

   // eglInitialize followed by eglTerminate has a resource leak
   if (test_initfree_default())     goto ONERR;
   if (test_initfree(sysdisp, freesysdisp))  goto ONERR;

   size_t    logsize;
   uint8_t * logbuffer;
   GETBUFFER_ERRLOG(&logbuffer, &logsize);

   TEST(0 == init_resourceusage(&usage));

   if (test_initfree(sysdisp, freesysdisp))  goto ONERR;
   TRUNCATEBUFFER_ERRLOG(logsize);

   TEST(0 == same_resourceusage(&usage));
   TEST(0 == free_resourceusage(&usage));

   TEST(0 == free_osdisplay(&osdisp));

   return 0;
ONERR:
   (void) free_resourceusage(&usage);
   (void) free_osdisplay(&osdisp);
   return EINVAL;
}

int unittest_platform_opengl_egl_egldisplay()
{
   int err;

   TEST(0 == execasprocess_unittest(&childprocess_unittest, &err));

   return err;
ONERR:
   return EINVAL;
}

#endif
