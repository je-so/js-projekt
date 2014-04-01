/* title: Graphic-Display impl

   Implements <Graphic-Display>.

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

   file: C-kern/api/graphic/display.h
    Header file <Graphic-Display>.

   file: C-kern/graphic/display.c
    Implementation file <Graphic-Display impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/graphic/display.h"
#include "C-kern/api/err.h"
#include "C-kern/api/platform/X11/x11screen.h"
#include "C-kern/api/test/errortimer.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/test/resourceusage.h"
#endif

#define KONFIG_opengl_egl 1
#define KONFIG_opengl_glx 2
#define KONFIG_x11        4


// section: display_t

// group: variable
#ifdef KONFIG_UNITTEST
/* variable: s_display_errtimer
 * Allows to introduce artificial errors. */
static test_errortimer_t   s_display_errtimer = test_errortimer_INIT_FREEABLE ;
/* variable: s_display_noext
 * Allows to introduce an X11 display withou extensions. */
static bool                s_display_noext = false;
#else
#define s_display_noext false
#endif

// group: helper-macros

#if ((KONFIG_USERINTERFACE)&KONFIG_x11) && ((KONFIG_USERINTERFACE)&KONFIG_opengl_egl)

/* define: OSDISPLAY_DEFAULTNAME
 * Defines default display name to connect with the default screen. */
#define OSDISPLAY_DEFAULTNAME 0

/* define: INIT_OSDISPLAY
 * Initializes the graphic display in an OS specific way. */
#define INIT_OSDISPLAY(disp, display_selector) \
         (s_display_noext ? init2_x11display(&(disp)->osdisplay, display_selector, false) : init_x11display(&(disp)->osdisplay, display_selector))

/* define: FREE_OSDISPLAY
 * Frees the graphic display in an OS specific way. */
#define FREE_OSDISPLAY(disp) \
         free_x11display(&(disp)->osdisplay)

/* define: INIT_OPENGL
 * Initializes the OpenGL part of the graphic display in an OS specific way. */
#define INIT_OPENGL(disp) \
         initx11_egldisplay(&(disp)->gldisplay, &(disp)->osdisplay)

/* define: FREE_OPENGL
 * Frees the OpenGL part of the graphic display in an OS specific way. */
#define FREE_OPENGL(disp) \
         free_egldisplay(&(disp)->gldisplay)

/* define: ISFREE_OSDISPLAY
 * Returns true in case the OS specific display is already freed. */
#define ISFREE_OSDISPLAY(disp) \
         isfree_x11display(disp)

#else

#error "Not implemented"

#endif

// group: lifetime

static int init_display(/*out*/display_t * disp, void * display_name)
{
   int err;
   int isinit = 0;

   err = INIT_OSDISPLAY(disp, display_name);
   if (err) goto ONABORT;
   ++ isinit;

   ONERROR_testerrortimer(&s_display_errtimer, ONABORT);

   err = INIT_OPENGL(disp);
   if (err) goto ONABORT;

   return 0;
ONABORT:
   if (isinit) {
      FREE_OSDISPLAY(disp);
   }
   return err;
}

int initdefault_display(/*out*/display_t * disp)
{
   int err;

   err = init_display(disp, OSDISPLAY_DEFAULTNAME);
   if (err) goto ONABORT;

   return 0;
ONABORT:
   TRACEABORT_ERRLOG(err);
   return err;
}

int free_display(display_t * disp)
{
   int err;
   int err2;

   if (!ISFREE_OSDISPLAY(&disp->osdisplay)) {
      err  = FREE_OPENGL(disp);
      err2 = FREE_OSDISPLAY(disp);
      if (err2) err = err2;
      if (err) goto ONABORT;
   }

   return 0;
ONABORT:
   TRACEABORTFREE_ERRLOG(err);
   return err;
}

#if ((KONFIG_USERINTERFACE)&KONFIG_x11) && ((KONFIG_USERINTERFACE)&KONFIG_opengl_egl)

uint32_t nrofscreens_display(const display_t * disp)
{
   return nrofscreens_x11display(&disp->osdisplay);
}

uint32_t defaultscreennr_display(const display_t * disp)
{
   return defaultscreennr_x11display(&disp->osdisplay);
}

#endif

// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

#if ((KONFIG_USERINTERFACE)&KONFIG_x11) && ((KONFIG_USERINTERFACE)&KONFIG_opengl_egl)

static bool isfree_helper(const display_t * disp)
{
   return   1 == isfree_x11display(&disp->osdisplay)
            && 0 == disp->gldisplay;
}

static void acceptleak_helper(resourceusage_t * usage)
{
   // EGL display has resource leak of 16 bytes
   acceptmallocleak_resourceusage(usage, 16 * 2);
}

#else
   #error "Not implemented"
#endif

static int test_initfree(void)
{
   display_t disp = display_INIT_FREEABLE;

   // TEST display_INIT_FREEABLE
   TEST(1 == isfree_helper(&disp));
   TEST(1 == ISFREE_OSDISPLAY(&disp.osdisplay));

   // TEST initdefault_display
   TEST(0 == initdefault_display(&disp));
   TEST(0 == isfree_helper(&disp));
   TEST(0 == ISFREE_OSDISPLAY(&disp.osdisplay));
   TEST(0 == free_display(&disp));
   TEST(1 == isfree_helper(&disp));
   TEST(1 == ISFREE_OSDISPLAY(&disp.osdisplay));

   return 0;
ONABORT:
   return EINVAL;
}

static int test_query(void)
{
   display_t   disp = display_INIT_FREEABLE;
   uint32_t    N;

   // prepare
   TEST(0 == initdefault_display(&disp));
   N = nrofscreens_display(&disp);

   // TEST nrofscreens_display
   TEST(1 <= nrofscreens_display(&disp));
   TEST(N == nrofscreens_display(&disp));

   // TEST defaultscreennr_display
   TEST(N > defaultscreennr_display(&disp));

   // TEST gl_display
   TEST(0 != gl_display(&disp));

   // TEST os_display
   TEST(0 != os_display(&disp));
   TEST(0 == ISFREE_OSDISPLAY(os_display(&disp)));

   // TEST gl_display: freed display
   TEST(0 == free_display(&disp));
   TEST(0 == gl_display(&disp));

   // TEST os_display: freed display
   TEST(0 != os_display(&disp));
   TEST(1 == ISFREE_OSDISPLAY(os_display(&disp)));

   return 0;
ONABORT:
   free_display(&disp);
   return EINVAL;
}


static int childprocess_unittest(void)
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE;

   if (test_initfree())       goto ONABORT;
   if (test_query())          goto ONABORT;
   s_display_noext = true;
   if (test_initfree())       goto ONABORT;

   TEST(0 == init_resourceusage(&usage));

   s_display_noext = true;

   if (test_initfree())       goto ONABORT;
   if (test_query())          goto ONABORT;

   acceptleak_helper(&usage);
   s_display_noext = false;

   TEST(0 == same_resourceusage(&usage));
   TEST(0 == free_resourceusage(&usage));

   return 0;
ONABORT:
   s_display_noext = false;
   (void) free_resourceusage(&usage);
   return EINVAL;
}

int unittest_graphic_display()
{
   int err;

   TEST(0 == execasprocess_unittest(&childprocess_unittest, &err));

   return err;
ONABORT:
   return EINVAL;
}

#endif

#undef KONFIG_opengl_egl
#undef KONFIG_opengl_glx
#undef KONFIG_x11
