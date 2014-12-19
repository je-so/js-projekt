/* title: Graphic-Surface-Configuration impl

   Implements <Graphic-Surface-Configuration>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2014 Jörg Seebohn

   file: C-kern/api/graphic/gconfig.h
    Header file <Graphic-Surface-Configuration>.

   file: C-kern/graphic/gconfig.c
    Implementation file <Graphic-Surface-Configuration impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/graphic/gconfig.h"
#include "C-kern/api/err.h"
#include "C-kern/api/graphic/display.h"
#if defined(KONFIG_USERINTERFACE_X11)
#include "C-kern/api/platform/X11/x11window.h"
#endif
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/test/resourceusage.h"
#include "C-kern/api/platform/OpenGL/EGL/egldisplay.h"
#endif


// section: gconfig_t

// group: lifetime

#if defined(KONFIG_USERINTERFACE_EGL)

int init_gconfig(/*out*/gconfig_t * gconf, display_t * display, const int32_t config_attributes[])
{
   int err;
   gconfig_filter_t filter;

#if defined(KONFIG_USERINTERFACE_X11)
   err = configfilter_x11window(&filter, config_attributes);
   if (err) goto ONERR;
#else
   #error "Not implemented"
#endif

   err = initfiltered_gconfig(gconf, display, config_attributes, &filter);
   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int initPid_gconfig(/*out*/gconfig_t * gconf, struct display_t * display, const uint32_t configid)
{
   int err;

   err = initPid_eglconfig(&gl_gconfig(gconf), gl_display(display), configid);
   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

struct eglfilter_param_t {
   display_t        * display;
   gconfig_filter_t * filter;
};

static bool eglconfig_filter(eglconfig_t eglconf, int32_t visualid, void * user)
{
   struct eglfilter_param_t * param = user;
   gconfig_t gconf = gconfig_INIT(eglconf);
   return param->filter->accept(&gconf, param->display, visualid, param->filter->user);
}

int initfiltered_gconfig(/*out*/gconfig_t * gconf, display_t * display, const int32_t config_attributes[], gconfig_filter_t * filter)
{
   int err;
   struct eglfilter_param_t user = { display, filter };

   err = initfiltered_eglconfig(&gl_gconfig(gconf), gl_display(display), config_attributes, &eglconfig_filter, &user);
   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

#else
   #error "No implementation defined"
#endif



// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

static inline void compiletimetest_config_enums(void)
{
   static_assert(0 == gconfig_NONE, "end of list or uninitialized value");
   static_assert(1 == gconfig_TYPE, "surface type");
   static_assert(2 == gconfig_TRANSPARENT_ALPHA, "surface must support transparent");
   static_assert(3 == gconfig_BITS_BUFFER, "number of bits of all color channels");
   static_assert(4 == gconfig_BITS_RED, "number of bits of red channel");
   static_assert(5 == gconfig_BITS_GREEN, "number of bits of green channel");
   static_assert(6 == gconfig_BITS_BLUE, "number of blue channel");
   static_assert(7 == gconfig_BITS_ALPHA, "number of bits of alpha channel");
   static_assert(8 == gconfig_BITS_DEPTH, "number of bits of depth channel");
   static_assert(9 == gconfig_BITS_STENCIL, "number of bits of stencil channel");
   static_assert(10 == gconfig_CONFORMANT, "type of supported conformant API");
   static_assert(11 == gconfig__NROF, "number of all configuration values including gconfig_NONE");

   static_assert(1 == gconfig_value_TYPE_PBUFFER_BIT, "surface type value");
   static_assert(2 == gconfig_value_TYPE_PIXMAP_BIT, "surface type value");
   static_assert(4 == gconfig_value_TYPE_WINDOW_BIT, "surface type value");
   static_assert(1 == gconfig_value_CONFORMANT_ES1_BIT, "drawing api support");
   static_assert(2 == gconfig_value_CONFORMANT_OPENVG_BIT, "drawing api support");
   static_assert(4 == gconfig_value_CONFORMANT_ES2_BIT, "drawing api support");
   static_assert(8 == gconfig_value_CONFORMANT_OPENGL_BIT, "drawing api support");
}

static bool dummy_filter(gconfig_t * gconf, struct display_t * display, int32_t visualid, void * user)
{
   (void) gconf;
   (void) display;
   (void) visualid;
   (void) user;
   return false;
}

static int test_configfilter(void)
{
   gconfig_filter_t filter = gconfig_filter_FREE;

   // TEST gconfig_filter_FREE
   TEST(0 == filter.user);
   TEST(0 == filter.accept);

   // TEST gconfig_filter_INIT
   filter = (gconfig_filter_t) gconfig_filter_INIT(&dummy_filter, &filter);
   TEST(filter.user   == &filter);
   TEST(filter.accept == &dummy_filter);

   return 0;
ONERR:
   return EINVAL;
}

static display_t *   s_filter_display;
static int32_t       s_filter_visualid;
static void *        s_filter_user;
static int           s_filter_total_count;
static int           s_filter_valid_count;

static bool filter_count(gconfig_t * gconf, struct display_t * display, int32_t visualid, void * user)
{
   int32_t visualid2 = -1;
   s_filter_valid_count += (
         gconf != 0 && gl_gconfig(gconf) != 0
         && s_filter_display == display
         && (0 == visualid_gconfig(gconf, display, &visualid2))
         && visualid         == visualid2
         && s_filter_user    == user);
   s_filter_total_count += 1;
   return false;
}

static bool filter_select(gconfig_t * gconf, struct display_t * display, int32_t visualid, void * user)
{
   (void) gconf;
   (void) display;
   s_filter_visualid = visualid;
   return (--*(int*)user) == 0;
}

static bool filter_attribon(gconfig_t * gconf, struct display_t * display, int32_t visualid, void * user)
{
   (void) visualid;
   int attrvalue = 0;
   s_filter_valid_count += (
         0 == value_gconfig(gconf, display, *(int*)user, &attrvalue)
         && attrvalue > 0);
   s_filter_total_count += 1;
   return false;
}

static bool filter_attriboff(gconfig_t * gconf, struct display_t * display, int32_t visualid, void * user)
{
   (void) visualid;
   int attrvalue = -1;
   s_filter_valid_count += (
         0 == value_gconfig(gconf, display, *(int*)user, &attrvalue)
         && attrvalue == 0);
   s_filter_total_count += 1;
   return false;
}

static int test_initfree(display_t * display)
{
   gconfig_t config = gconfig_FREE;
   int config_attributes[10];
   int config_attriberr1[] = { gconfig_TYPE, -1, gconfig_NONE };
   int config_attriberr2[2*gconfig__NROF+1];
   int config_attriberr3[] = { gconfig_BITS_RED, 1024, gconfig_NONE };

   // TEST gconfig_FREE
   TEST(0 == config.glconfig);

   // TEST free_gconfig: initialized with gconfig_FREE
   TEST(0 == free_gconfig(&config));
   TEST(0 == config.glconfig);

   // prepare
   config_attributes[0] = gconfig_TYPE;
   config_attributes[1] = gconfig_value_TYPE_WINDOW_BIT;
   config_attributes[2] = gconfig_NONE;
   memset(config_attriberr2, gconfig_NONE, sizeof(config_attriberr2));
   for (unsigned i = 0; i < lengthof(config_attriberr2)-2; ) {
      config_attriberr2[i++] = gconfig_BITS_RED;
      config_attriberr2[i++] = 1;
   }

   // TEST init_gconfig: EINVAL (egldisplay_t not initialized)
   TEST(EINVAL == init_gconfig(&config, &(display_t) display_FREE, config_attributes));
   TEST(0 == config.glconfig);

   // TEST init_gconfig: EINVAL (value in config_attributes wrong)
   TEST(EINVAL == init_gconfig(&config, display, config_attriberr1));
   TEST(0 == config.glconfig);

   // TEST init_gconfig: E2BIG (config_attributes list too long)
   TEST(E2BIG == init_gconfig(&config, display, config_attriberr2));
   TEST(0 == config.glconfig);

   // TEST init_gconfig: ESRCH (no configuration with 1024 red bits)
   TEST(ESRCH == init_gconfig(&config, display, config_attriberr3));
   TEST(0 == config.glconfig);

   // TEST init_gconfig
   TEST(0 == init_gconfig(&config, display, config_attributes));
   TEST(0 != config.glconfig);

   // TEST free_gconfig
   TEST(0 == free_gconfig(&config));
   TEST(0 == config.glconfig);
   TEST(0 == free_gconfig(&config));
   TEST(0 == config.glconfig);

   // TEST initfiltered_gconfig: filter is called with correct parameter
   s_filter_display = display;
   s_filter_user    = &config;
   s_filter_total_count = 0;
   s_filter_valid_count = 0;
   config_attributes[0] = gconfig_NONE;
   TEST(ESRCH == initfiltered_gconfig(&config, display, config_attributes,
                  &(gconfig_filter_t)gconfig_filter_INIT(&filter_count, &config)));
   TEST(s_filter_valid_count == s_filter_total_count);
   TEST(s_filter_valid_count >= 2);
   TEST(0 == config.glconfig);
   const int totalcount = s_filter_total_count;

   // TEST initfiltered_gconfig: use visualid for which filter signals true
   for (int i = totalcount; i > 0; --i) {
      int select_count = i; // use ith id
      TEST(0 == initfiltered_gconfig(&config, display, config_attributes,
                  &(gconfig_filter_t)gconfig_filter_INIT(filter_select, &select_count)));
      TEST(0 != config.glconfig);
      TEST(0 == select_count);
      int32_t visualid = -1;
      TEST(0 == visualid_gconfig(&config, display, &visualid));
      TEST(s_filter_visualid == visualid);
      TEST(0 == free_gconfig(&config));
      TEST(0 == config.glconfig);
   }

   // TEST initfiltered_gconfig: only valid configurations (attribute on and off)
   int onoff[] = {
      gconfig_BITS_ALPHA, gconfig_BITS_DEPTH, gconfig_BITS_STENCIL
   };
   for (unsigned i = 0; i < lengthof(onoff); ++i) {
      for (int isOn = 0; isOn <= 1; ++isOn) {
         config_attributes[0] = onoff[i];
         config_attributes[1] = isOn;
         config_attributes[2] = gconfig_NONE;
         s_filter_total_count = 0;
         s_filter_valid_count = 0;
         TEST(ESRCH == initfiltered_gconfig(&config, display, config_attributes,
                        &(gconfig_filter_t)gconfig_filter_INIT(isOn ? &filter_attribon : &filter_attriboff, &onoff[i])));
         TEST(s_filter_valid_count >= 1);
         if (isOn) {
            TEST(s_filter_valid_count == s_filter_total_count);
         } else {
            TEST(s_filter_valid_count <  s_filter_total_count);
         }
         TEST(0 == free_gconfig(&config));
         TEST(0 == config.glconfig);
      }
   }

   config_attributes[0] = gconfig_NONE;
   for (int i = totalcount; i > 0; --i) {
      uint32_t configid  = INT32_MAX;
      uint32_t configid2 = INT32_MAX;
      int32_t  visualid  = -1;
      int32_t  visualid2 = -1;

      // TEST configid_gconfig
      int select_count = i; // use ith id
      TEST(0 == initfiltered_gconfig(&config, display, config_attributes,
                  &(gconfig_filter_t)gconfig_filter_INIT(filter_select, &select_count)));
      TEST(0 == visualid_gconfig(&config, display, &visualid));
      TEST(0 == configid_gconfig(&config, display, &configid));
      TEST(0 < configid);
      TEST(INT32_MAX > configid);
      TEST(0 == free_gconfig(&config));
      TEST(0 == config.glconfig);

      // TEST initPid_gconfig
      TEST(0 == initPid_gconfig(&config, display, configid));
      TEST(0 != config.glconfig);
      TEST(0 == configid_gconfig(&config, display, &configid2));
      TEST(configid2 == configid);
      TEST(0 == visualid_gconfig(&config, display, &visualid2));
      TEST(visualid2 == visualid);
      TEST(0 == free_gconfig(&config));
      TEST(0 == config.glconfig);
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_query(display_t * display)
{
   gconfig_t config = gconfig_FREE;
   int       attrlist[10];

   // TEST gl_gconfig
   for (uintptr_t i = 1; i; i <<= 1) {
      config.glconfig = (void*)i;
      TEST((void*)i == gl_gconfig(&config));
   }
   config.glconfig = 0;
   TEST(0 == gl_gconfig(&config));

   // TEST value_gconfig
   int onoff[] = {
      gconfig_BITS_ALPHA, gconfig_BITS_DEPTH, gconfig_BITS_STENCIL
   };
   for (unsigned i = 0; i < lengthof(onoff); ++i) {
      for (int isOn = 0; isOn <= 1; ++isOn) {
         attrlist[0] = onoff[i];
         attrlist[1] = isOn;
         attrlist[2] = gconfig_NONE;
         TEST(0 == init_gconfig(&config, display, attrlist));
         int32_t attrval = -1;
         TEST(0 == value_gconfig(&config, display, onoff[i], &attrval));
         TEST(isOn == (0 < attrval));
         TEST(0 == free_gconfig(&config));
      }
   }

   int32_t  oldvisualid = -1;
   uint32_t oldconfigid = INT32_MAX;
   for (int i = 0; i <= 2; ++i) {
      int32_t visualid = -1;
      uint32_t configid = INT32_MAX;

      // prepare
      attrlist[0] = gconfig_TYPE;
      attrlist[1] = !i ? gconfig_value_TYPE_PIXMAP_BIT : i == 1 ? gconfig_value_TYPE_WINDOW_BIT : gconfig_value_TYPE_PBUFFER_BIT;
      attrlist[2] = gconfig_NONE;
      TEST(0 == init_gconfig(&config, display, attrlist));

      // TEST visualid_gconfig
      TEST(0 == visualid_gconfig(&config, display, &visualid));
      TEST(0 <= visualid);
      TEST(oldvisualid != visualid);
      oldvisualid = visualid;
#if defined(KONFIG_USERINTERFACE_X11)
      if (i == 1) {
         TEST(0 < visualid/*only windows have a valid visual id*/);
      } else {
         TEST(0 == visualid/*invalid*/);
      }
#endif

      // TEST configid_gconfig
      TEST(0 == configid_gconfig(&config, display, &configid));
      TEST(INT32_MAX > configid);
      TEST(oldconfigid != configid);
      oldconfigid = configid;

      if (i == 2) {
         // TEST maxpbuffer_gconfig
         uint32_t maxwidth  = 0;
         uint32_t maxheight = 0;
         uint32_t maxpixels = 0;
         TEST(0 == maxpbuffer_gconfig(&config, display, &maxwidth, &maxheight, &maxpixels));
         TEST(16 < maxheight);
         TEST(16 < maxwidth);
         TEST(16 < maxpixels);
         TEST(maxpixels > maxheight);
         TEST(maxpixels > maxwidth);
         TEST(maxwidth * maxheight >= maxpixels);

         // TEST maxpbuffer_gconfig: 0 values for out parameters supported
         TEST(0 == maxpbuffer_gconfig(&config, display, 0, 0, 0));
      }

      // unprepare
      TEST(0 == free_gconfig(&config));
   }

   return 0;
ONERR:
   return EINVAL;
}

#if defined(KONFIG_USERINTERFACE_EGL)

#define INITDEFAULT_DISPLAY(display) \
         initdefault_display(display)

#define FREE_DISPLAY(display) \
         free_display(display)

#else
   #error "No implementation defined"
#endif


static int childprocess_unittest(void)
{
   resourceusage_t   usage   = resourceusage_FREE;
   display_t         display = display_FREE;

   TEST(0 == INITDEFAULT_DISPLAY(&display));

   TEST(0 == init_resourceusage(&usage));

   if (test_configfilter())      goto ONERR;
   if (test_initfree(&display))  goto ONERR;
   if (test_query(&display))     goto ONERR;

   TEST(0 == same_resourceusage(&usage));
   TEST(0 == free_resourceusage(&usage));

   TEST(0 == FREE_DISPLAY(&display));

   return 0;
ONERR:
   (void) free_resourceusage(&usage);
   (void) FREE_DISPLAY(&display);
   return EINVAL;
}

int unittest_graphic_gconfig()
{
   int err;

   TEST(0 == execasprocess_unittest(&childprocess_unittest, &err));

   return err;
ONERR:
   return EINVAL;
}

#endif
