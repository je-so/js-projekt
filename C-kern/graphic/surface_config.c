/* title: Graphic-Surface-Configuration impl

   Implements <Graphic-Surface-Configuration>.

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

   file: C-kern/api/graphic/surface_config.h
    Header file <Graphic-Surface-Configuration>.

   file: C-kern/graphic/surface_config.c
    Implementation file <Graphic-Surface-Configuration impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/graphic/surface_config.h"
#include "C-kern/api/err.h"
#include "C-kern/api/platform/OpenGL/EGL/eglconfig.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/test/resourceusage.h"
#include "C-kern/api/platform/OpenGL/EGL/egldisplay.h"
#endif


// section: surface_config_t

// group: variables

#define KONFIG_opengl_egl 1
#if ((KONFIG_USERINTERFACE)&KONFIG_opengl_egl)
static const surface_config_it s_surfaceconfig_egl_iimpl = {
      &value_eglconfig
};
#endif
#undef KONFIG_opengl_egl

// group: lifetime

int initfromegl_surfaceconfig(/*out*/surface_config_t * config, void * egldisp, const int config_attributes[])
#define KONFIG_opengl_egl 1
#if ! ((KONFIG_USERINTERFACE)&KONFIG_opengl_egl)
{
   (void) config;
   (void) egldisp;
   (void) config_attributes;
   return ENOSYS;
}

#else
{
   int err;

   err = init_eglconfig(&config->config, egldisp, config_attributes);
   if (err) goto ONABORT;

   config->iimpl = &s_surfaceconfig_egl_iimpl;

ONABORT:
   TRACEABORT_ERRLOG(err);
   return err;
}
#endif
#undef KONFIG_opengl_egl


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

static inline void compiletimetest_config_enums(void)
{
   static_assert(0 == surface_config_NONE, "end of list or uninitialized value");
   static_assert(1 == surface_config_TYPE, "surface type");
   static_assert(2 == surface_config_TRANSPARENT_ALPHA, "surface must support transparent");
   static_assert(3 == surface_config_BITS_RED, "number of red channel bits");
   static_assert(4 == surface_config_BITS_GREEN, "number of green channel bits");
   static_assert(5 == surface_config_BITS_BLUE, "number of blue channel bits");
   static_assert(6 == surface_config_BITS_ALPHA, "number of alpha channel bits");
   static_assert(7 == surface_config_BITS_DEPTH, "number of depth channel bits");
   static_assert(8 == surface_config_BITS_STENCIL, "number of stencil channel bits");
   static_assert(9 == surface_config_CONFORMANT, "type of supported conformant API");
   static_assert(10 == surface_config_NROFCONFIGS, "number of all configuration values including surface_config_NONE");

   static_assert(1 == surface_configvalue_TYPE_PBUFFER_BIT, "surface type value");
   static_assert(2 == surface_configvalue_TYPE_PIXMAP_BIT, "surface type value");
   static_assert(4 == surface_configvalue_TYPE_WINDOW_BIT, "surface type value");
   static_assert(1 == surface_configvalue_CONFORMANT_ES1_BIT, "drawing api support");
   static_assert(2 == surface_configvalue_CONFORMANT_OPENVG_BIT, "drawing api support");
   static_assert(4 == surface_configvalue_CONFORMANT_ES2_BIT, "drawing api support");
   static_assert(8 == surface_configvalue_CONFORMANT_OPENGL_BIT, "drawing api support");
}

static int test_initfree(void)
{
   surface_config_t config = surface_config_INIT_FREEABLE;

   // TEST surface_config_INIT_FREEABLE
   TEST(0 == config.iimpl);
   TEST(0 == config.config);

   return 0;
ONABORT:
   return EINVAL;
}

#define KONFIG_opengl_egl 1
#if ! ((KONFIG_USERINTERFACE)&KONFIG_opengl_egl)
static inline int test_initfree_egl(egldisplay_t egldisp)
{
   surface_config_t  config  = surface_config_INIT_FREEABLE;

   TEST(ENOSYS == initfromegl_surfaceconfig(&config, egldisp, (int[]) { surface_config_NONE }));
   TEST(0 == config.iimpl);
   TEST(0 == config.config);

   return 0;
ONABORT:
   return EINVAL;
}

static inline int test_query_egl(egldisplay_t egldisp)
{
   (void) egldisp;
   return 0;
}

#define INITDEFAULT_egldisplay(egldisp) \
         (0)

#define FREE_egldisplay(egldisp) \
         (0)

#else

#define INITDEFAULT_egldisplay(egldisp) \
         initdefault_egldisplay(egldisp)

#define FREE_egldisplay(egldisp) \
         free_egldisplay(egldisp)

static inline int test_initfree_egl(egldisplay_t egldisp)
{
   surface_config_t  config  = surface_config_INIT_FREEABLE;
   int config_attributes[]   = { surface_config_TYPE, surface_configvalue_TYPE_WINDOW_BIT, surface_config_NONE };
   int config_attriberr1[]   = { surface_config_TYPE, -1, surface_config_NONE };
   int config_attriberr2[21];
   int config_attriberr3[]   = { surface_config_BITS_RED, 1024, surface_config_NONE };

   // prepare
   memset(config_attriberr2, surface_config_NONE, sizeof(config_attriberr2));
   for (unsigned i = 0; i < lengthof(config_attriberr2)-2; ) {
      config_attriberr2[i++] = surface_config_BITS_RED;
      config_attriberr2[i++] = 1;
   }

   // TEST initfromegl_surfaceconfig: EINVAL (egldisplay_t not initialized)
   TEST(EINVAL == initfromegl_surfaceconfig(&config, egldisplay_INIT_FREEABLE, config_attributes));
   TEST(0 == config.iimpl);
   TEST(0 == config.config);

   // TEST initfromegl_surfaceconfig: EINVAL (value in config_attributes wrong)
   TEST(EINVAL == initfromegl_surfaceconfig(&config, egldisp, config_attriberr1));
   TEST(0 == config.iimpl);
   TEST(0 == config.config);

   // TEST initfromegl_surfaceconfig: E2BIG (config_attributes list too long)
   TEST(E2BIG == initfromegl_surfaceconfig(&config, egldisp, config_attriberr2));
   TEST(0 == config.iimpl);
   TEST(0 == config.config);

   // TEST initfromegl_surfaceconfig: ESRCH (no configuration with 1024 red bits)
   TEST(ESRCH == initfromegl_surfaceconfig(&config, egldisp, config_attriberr3));
   TEST(0 == config.iimpl);
   TEST(0 == config.config);

   // TEST initfromegl_surfaceconfig
   TEST(0 == initfromegl_surfaceconfig(&config, egldisp, config_attributes));
   TEST(0 != config.iimpl);
   TEST(0 != config.config);
   TEST(config.iimpl == &s_surfaceconfig_egl_iimpl);

   // TEST free_surfaceconfig
   TEST(0 == free_surfaceconfig(&config));
   TEST(0 == config.iimpl);
   TEST(0 == config.config);
   TEST(0 == free_surfaceconfig(&config));
   TEST(0 == config.iimpl);
   TEST(0 == config.config);

   return 0;
ONABORT:
   return EINVAL;
}

static int test_query_egl(egldisplay_t egldisp)
{
   surface_config_t  config  = surface_config_INIT_FREEABLE;
   int               attrlist[10];

   // TEST value_surfaceconfig
   int onoff[] = {
      surface_config_BITS_ALPHA, surface_config_BITS_DEPTH, surface_config_BITS_STENCIL
   };
   for (unsigned i = 0; i < lengthof(onoff); ++i) {
      for (int isOn = 0; isOn <= 1; ++isOn) {
         attrlist[0] = onoff[i];
         attrlist[1] = isOn;
         attrlist[2] = surface_config_NONE;
         TEST(0 == initfromegl_surfaceconfig(&config, egldisp, attrlist));
         int attrval = -1;
         TEST(0 == value_surfaceconfig(&config, egldisp, onoff[i], &attrval));
         TEST(isOn == (0 < attrval));
         TEST(0 == free_surfaceconfig(&config));
      }
   }

   return 0;
ONABORT:
   return EINVAL;
}
#endif
#undef KONFIG_opengl_egl

static int childprocess_unittest(void)
{
   resourceusage_t   usage   = resourceusage_INIT_FREEABLE;
   egldisplay_t      egldisp = egldisplay_INIT_FREEABLE;

   TEST(0 == INITDEFAULT_egldisplay(&egldisp));

   TEST(0 == init_resourceusage(&usage));

   if (test_initfree())          goto ONABORT;
   if (test_query_egl(egldisp))  goto ONABORT;
   if (test_initfree_egl(egldisp)) goto ONABORT;

   TEST(0 == same_resourceusage(&usage));
   TEST(0 == free_resourceusage(&usage));

   TEST(0 == FREE_egldisplay(&egldisp));

   return 0;
ONABORT:
   (void) free_resourceusage(&usage);
   (void) FREE_egldisplay(&egldisp);
   return EINVAL;
}

int unittest_graphic_surface_config()
{
   int err;

   TEST(0 == execasprocess_unittest(&childprocess_unittest, &err));

   return err;
ONABORT:
   return EINVAL;
}

#endif
