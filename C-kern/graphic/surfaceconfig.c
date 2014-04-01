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

   file: C-kern/api/graphic/surfaceconfig.h
    Header file <Graphic-Surface-Configuration>.

   file: C-kern/graphic/surfaceconfig.c
    Implementation file <Graphic-Surface-Configuration impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/graphic/surfaceconfig.h"
#include "C-kern/api/err.h"
#include "C-kern/api/platform/OpenGL/EGL/eglconfig.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/test/resourceusage.h"
#include "C-kern/api/platform/OpenGL/EGL/egldisplay.h"
#endif


// section: surfaceconfig_t

// group: lifetime

#define KONFIG_opengl_egl 1
#if ((KONFIG_USERINTERFACE)&KONFIG_opengl_egl)

static bool eglconfig_filter(eglconfig_t eglconf, struct opengl_display_t * egldisp, int32_t visualid, void * user)
{
   surfaceconfig_filter_t* filter   = user;
   surfaceconfig_t         surfconf = surfaceconfig_INIT(eglconf);
   return filter->accept(&surfconf, egldisp, visualid, filter->user);
}

int initfiltered_surfaceconfig(/*out*/surfaceconfig_t * surfconf, struct opengl_display_t * display, const int32_t config_attributes[], surfaceconfig_filter_t * filter)
{
   int err;
   err = initfiltered_eglconfig(&(surfconf)->config, display, config_attributes, &eglconfig_filter, filter);
   return err;
}

#else

#error "No implementation defined for surfaceconfig_t"

#endif
#undef KONFIG_opengl_egl


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

static inline void compiletimetest_config_enums(void)
{
   static_assert(0 == surfaceconfig_NONE, "end of list or uninitialized value");
   static_assert(1 == surfaceconfig_TYPE, "surface type");
   static_assert(2 == surfaceconfig_TRANSPARENT_ALPHA, "surface must support transparent");
   static_assert(3 == surfaceconfig_BITS_BUFFER, "number of bits of all color channels");
   static_assert(4 == surfaceconfig_BITS_RED, "number of bits of red channel");
   static_assert(5 == surfaceconfig_BITS_GREEN, "number of bits of green channel");
   static_assert(6 == surfaceconfig_BITS_BLUE, "number of blue channel");
   static_assert(7 == surfaceconfig_BITS_ALPHA, "number of bits of alpha channel");
   static_assert(8 == surfaceconfig_BITS_DEPTH, "number of bits of depth channel");
   static_assert(9 == surfaceconfig_BITS_STENCIL, "number of bits of stencil channel");
   static_assert(10 == surfaceconfig_CONFORMANT, "type of supported conformant API");
   static_assert(11 == surfaceconfig_NROFELEMENTS, "number of all configuration values including surfaceconfig_NONE");

   static_assert(1 == surfaceconfig_value_TYPE_PBUFFER_BIT, "surface type value");
   static_assert(2 == surfaceconfig_value_TYPE_PIXMAP_BIT, "surface type value");
   static_assert(4 == surfaceconfig_value_TYPE_WINDOW_BIT, "surface type value");
   static_assert(1 == surfaceconfig_value_CONFORMANT_ES1_BIT, "drawing api support");
   static_assert(2 == surfaceconfig_value_CONFORMANT_OPENVG_BIT, "drawing api support");
   static_assert(4 == surfaceconfig_value_CONFORMANT_ES2_BIT, "drawing api support");
   static_assert(8 == surfaceconfig_value_CONFORMANT_OPENGL_BIT, "drawing api support");
}

static bool dummy_filter(surfaceconfig_t * surfconf, struct opengl_display_t * display, int32_t visualid, void * user)
{
   (void) surfconf;
   (void) display;
   (void) visualid;
   (void) user;
   return false;
}

static int test_configfilter(void)
{
   surfaceconfig_filter_t filter = surfaceconfig_filter_INIT_FREEABLE;

   // TEST surfaceconfig_filter_INIT_FREEABLE
   TEST(0 == filter.user);
   TEST(0 == filter.accept);

   // TEST surfaceconfig_filter_INIT
   filter = (surfaceconfig_filter_t) surfaceconfig_filter_INIT(&dummy_filter, &filter);
   TEST(filter.user   == &filter);
   TEST(filter.accept == &dummy_filter);

   return 0;
ONABORT:
   return EINVAL;
}

static int test_initfree(void)
{
   surfaceconfig_t config = surfaceconfig_INIT_FREEABLE;

   // TEST surfaceconfig_INIT_FREEABLE
   TEST(0 == config.config);

   // TEST free_surfaceconfig
   TEST(0 == free_surfaceconfig(&config));
   TEST(0 == config.config);

   return 0;
ONABORT:
   return EINVAL;
}

static opengl_display_t* s_filter_display;
static int32_t       s_filter_visualid;
static void *        s_filter_user;
static int           s_filter_total_count;
static int           s_filter_valid_count;

static bool filter_count(surfaceconfig_t * surfconf, struct opengl_display_t * display, int32_t visualid, void * user)
{
   int32_t visualid2 = -1;
   s_filter_valid_count += (
         surfconf != 0 && surfconf->config != 0
         && s_filter_display == display
         && (0 == visualid_surfaceconfig(surfconf, display, &visualid2))
         && visualid         == visualid2
         && s_filter_user    == user);
   s_filter_total_count += 1;
   return false;
}

static bool filter_select(surfaceconfig_t * surfconf, struct opengl_display_t * display, int32_t visualid, void * user)
{
   (void) surfconf;
   (void) display;
   s_filter_visualid = visualid;
   return (--*(int*)user) == 0;
}

static bool filter_attribon(surfaceconfig_t * surfconf, struct opengl_display_t * display, int32_t visualid, void * user)
{
   (void) visualid;
   int attrvalue = 0;
   s_filter_valid_count += (
         0 == value_surfaceconfig(surfconf, display, *(int*)user, &attrvalue)
         && attrvalue > 0);
   s_filter_total_count += 1;
   return false;
}

static bool filter_attriboff(surfaceconfig_t * surfconf, struct opengl_display_t * display, int32_t visualid, void * user)
{
   (void) visualid;
   int attrvalue = -1;
   s_filter_valid_count += (
         0 == value_surfaceconfig(surfconf, display, *(int*)user, &attrvalue)
         && attrvalue == 0);
   s_filter_total_count += 1;
   return false;
}

static int test_initfree2(opengl_display_t * display)
{
   surfaceconfig_t  config   = surfaceconfig_INIT_FREEABLE;
   int config_attributes[10];
   int config_attriberr1[]   = { surfaceconfig_TYPE, -1, surfaceconfig_NONE };
   int config_attriberr2[2*surfaceconfig_NROFELEMENTS+1];
   int config_attriberr3[]   = { surfaceconfig_BITS_RED, 1024, surfaceconfig_NONE };

   // prepare
   config_attributes[0] = surfaceconfig_TYPE;
   config_attributes[1] = surfaceconfig_value_TYPE_WINDOW_BIT;
   config_attributes[2] = surfaceconfig_NONE;
   memset(config_attriberr2, surfaceconfig_NONE, sizeof(config_attriberr2));
   for (unsigned i = 0; i < lengthof(config_attriberr2)-2; ) {
      config_attriberr2[i++] = surfaceconfig_BITS_RED;
      config_attriberr2[i++] = 1;
   }

   // TEST init_surfaceconfig: EINVAL (egldisplay_t not initialized)
   TEST(EINVAL == init_surfaceconfig(&config, egldisplay_INIT_FREEABLE, config_attributes));
   TEST(0 == config.config);

   // TEST init_surfaceconfig: EINVAL (value in config_attributes wrong)
   TEST(EINVAL == init_surfaceconfig(&config, display, config_attriberr1));
   TEST(0 == config.config);

   // TEST init_surfaceconfig: E2BIG (config_attributes list too long)
   TEST(E2BIG == init_surfaceconfig(&config, display, config_attriberr2));
   TEST(0 == config.config);

   // TEST init_surfaceconfig: ESRCH (no configuration with 1024 red bits)
   TEST(ESRCH == init_surfaceconfig(&config, display, config_attriberr3));
   TEST(0 == config.config);

   // TEST init_surfaceconfig
   TEST(0 == init_surfaceconfig(&config, display, config_attributes));
   TEST(0 != config.config);

   // TEST free_surfaceconfig
   TEST(0 == free_surfaceconfig(&config));
   TEST(0 == config.config);
   TEST(0 == free_surfaceconfig(&config));
   TEST(0 == config.config);

   // TEST initfiltered_surfaceconfig: filter is called with correct parameter
   s_filter_display = display;
   s_filter_user    = &config;
   s_filter_total_count = 0;
   s_filter_valid_count = 0;
   config_attributes[0] = surfaceconfig_NONE;
   TEST(ESRCH == initfiltered_surfaceconfig(&config, display, config_attributes,
                  &(surfaceconfig_filter_t)surfaceconfig_filter_INIT(&filter_count, &config)));
   TEST(s_filter_valid_count == s_filter_total_count);
   TEST(s_filter_valid_count >= 2);
   TEST(0 == config.config);

   // TEST initfiltered_surfaceconfig: use visualid for which filter signals true
   for (int i = s_filter_total_count; i > 0; --i) {
      int select_count = i; // use ith id
      TEST(0 == initfiltered_surfaceconfig(&config, display, config_attributes,
                  &(surfaceconfig_filter_t)surfaceconfig_filter_INIT(filter_select, &select_count)));
      TEST(0 != config.config);
      TEST(0 == select_count);
      int32_t visualid = -1;
      TEST(0 == visualid_surfaceconfig(&config, display, &visualid));
      TEST(s_filter_visualid == visualid);
      TEST(0 == free_surfaceconfig(&config));
      TEST(0 == config.config);
   }

   // TEST initfiltered_surfaceconfig: only valid configurations (attribute on and off)
   int onoff[] = {
      surfaceconfig_BITS_ALPHA, surfaceconfig_BITS_DEPTH, surfaceconfig_BITS_STENCIL
   };
   for (unsigned i = 0; i < lengthof(onoff); ++i) {
      for (int isOn = 0; isOn <= 1; ++isOn) {
         config_attributes[0] = onoff[i];
         config_attributes[1] = isOn;
         config_attributes[2] = surfaceconfig_NONE;
         s_filter_total_count = 0;
         s_filter_valid_count = 0;
         TEST(ESRCH == initfiltered_surfaceconfig(&config, display, config_attributes,
                        &(surfaceconfig_filter_t)surfaceconfig_filter_INIT(isOn ? &filter_attribon : &filter_attriboff, &onoff[i])));
         TEST(s_filter_valid_count >= 1);
         if (isOn) {
            TEST(s_filter_valid_count == s_filter_total_count);
         } else {
            TEST(s_filter_valid_count <  s_filter_total_count);
         }
         TEST(0 == free_surfaceconfig(&config));
         TEST(0 == config.config);
      }
   }

   return 0;
ONABORT:
   return EINVAL;
}

static int test_query(opengl_display_t * display)
{
   surfaceconfig_t   config = surfaceconfig_INIT_FREEABLE;
   int               attrlist[10];

   // TEST value_surfaceconfig
   int onoff[] = {
      surfaceconfig_BITS_ALPHA, surfaceconfig_BITS_DEPTH, surfaceconfig_BITS_STENCIL
   };
   for (unsigned i = 0; i < lengthof(onoff); ++i) {
      for (int isOn = 0; isOn <= 1; ++isOn) {
         attrlist[0] = onoff[i];
         attrlist[1] = isOn;
         attrlist[2] = surfaceconfig_NONE;
         TEST(0 == init_surfaceconfig(&config, display, attrlist));
         int32_t attrval = -1;
         TEST(0 == value_surfaceconfig(&config, display, onoff[i], &attrval));
         TEST(isOn == (0 < attrval));
         TEST(0 == free_surfaceconfig(&config));
      }
   }

   // TEST visualid_surfaceconfig
   int32_t oldvisualid = -1;
   for (int i = 0; i <= 1; ++i) {
      attrlist[0] = surfaceconfig_TYPE;
      attrlist[1] = i ? surfaceconfig_value_TYPE_WINDOW_BIT : surfaceconfig_value_TYPE_PIXMAP_BIT;
      attrlist[2] = surfaceconfig_NONE;
      TEST(0 == init_surfaceconfig(&config, display, attrlist));
      int32_t visualid = -1;
      TEST(0 == visualid_surfaceconfig(&config, display, &visualid));
      TEST(0 <= visualid);
      TEST(oldvisualid != visualid);
      oldvisualid = visualid;
#define KONFIG_x11        4
#if ((KONFIG_USERINTERFACE)&KONFIG_x11)
      if (i) {
         TEST(0 < visualid);
      } else {
         TEST(0 == visualid/*pixmap has no visual only windows; indicated with 0 (None) in X11*/);
      }
#endif
#undef KONFIG_x11
      TEST(0 == free_surfaceconfig(&config));
   }

   return 0;
ONABORT:
   return EINVAL;
}

#define KONFIG_opengl_egl 1
#if ((KONFIG_USERINTERFACE)&KONFIG_opengl_egl)

#define INITDEFAULT_DISPLAY(display) \
         initdefault_egldisplay(display)

#define FREE_DISPLAY(display) \
         free_egldisplay(display)

#else

#error "Port test_initfree2 ... to different os specific interface"

#endif
#undef KONFIG_opengl_egl

static int childprocess_unittest(void)
{
   resourceusage_t    usage   = resourceusage_INIT_FREEABLE;
   opengl_display_t * display = 0;

   TEST(0 == INITDEFAULT_DISPLAY(&display));

   TEST(0 == init_resourceusage(&usage));

   if (test_configfilter())      goto ONABORT;
   if (test_initfree())          goto ONABORT;
   if (test_initfree2(display))  goto ONABORT;
   if (test_query(display))      goto ONABORT;

   TEST(0 == same_resourceusage(&usage));
   TEST(0 == free_resourceusage(&usage));

   TEST(0 == FREE_DISPLAY(&display));

   return 0;
ONABORT:
   (void) free_resourceusage(&usage);
   (void) FREE_DISPLAY(&display);
   return EINVAL;
}

int unittest_graphic_surfaceconfig()
{
   int err;

   TEST(0 == execasprocess_unittest(&childprocess_unittest, &err));

   return err;
ONABORT:
   return EINVAL;
}

#endif
