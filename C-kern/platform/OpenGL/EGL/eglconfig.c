/* title: EGL-Framebuffer-Configuration impl

   Implements <EGL-Framebuffer-Configuration>.

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

   file: C-kern/api/platform/OpenGL/EGL/eglconfig.h
    Header file <EGL-Framebuffer-Configuration>.

   file: C-kern/platform/OpenGL/EGL/eglconfig.c
    Implementation file <EGL-Framebuffer-Configuration impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/platform/OpenGL/EGL/eglconfig.h"
#include "C-kern/api/err.h"
#include "C-kern/api/graphic/gconfig.h"
#include "C-kern/api/platform/OpenGL/EGL/egl.h"
#include "C-kern/api/platform/OpenGL/EGL/egldisplay.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/test/resourceusage.h"
#include "C-kern/api/platform/X11/x11display.h"
#endif
#include STR(C-kern/api/platform/KONFIG_OS/graphic/sysegl.h)


// section: eglconfig_t

// group: helper

static inline int convertAttribToEGL_eglconfig(int attribute, int32_t value, /*out*/EGLint tuple[2])
{
   switch (attribute) {
   case gconfig_TYPE:
      if ( (value
            & ~(gconfig_value_TYPE_WINDOW_BIT|gconfig_value_TYPE_PBUFFER_BIT|gconfig_value_TYPE_PIXMAP_BIT))) {
         return EINVAL;
      }
      tuple[0] = EGL_SURFACE_TYPE;
      static_assert( EGL_PBUFFER_BIT   == gconfig_value_TYPE_PBUFFER_BIT
                     && EGL_PIXMAP_BIT == gconfig_value_TYPE_PIXMAP_BIT
                     && EGL_WINDOW_BIT == gconfig_value_TYPE_WINDOW_BIT, "no conversion needed");
      tuple[1] = value;
      break;
   case gconfig_TRANSPARENT_ALPHA:
      tuple[0] = EGL_ALPHA_SIZE;
      tuple[1] = (value != 0);
      break;
   case gconfig_BITS_BUFFER:
      tuple[0] = EGL_BUFFER_SIZE;
      tuple[1] = value;
      break;
   case gconfig_BITS_RED:
      tuple[0] = EGL_RED_SIZE;
      tuple[1] = value;
      break;
   case gconfig_BITS_GREEN:
      tuple[0] = EGL_GREEN_SIZE;
      tuple[1] = value;
      break;
   case gconfig_BITS_BLUE:
      tuple[0] = EGL_BLUE_SIZE;
      tuple[1] = value;
      break;
   case gconfig_BITS_ALPHA:
      tuple[0] = EGL_ALPHA_SIZE;
      tuple[1] = value;
      break;
   case gconfig_BITS_DEPTH:
      tuple[0] = EGL_DEPTH_SIZE;
      tuple[1] = value;
      break;
   case gconfig_BITS_STENCIL:
      tuple[0] = EGL_STENCIL_SIZE;
      tuple[1] = value;
      break;
   case gconfig_CONFORMANT:
      if ( (value
            & ~(gconfig_value_CONFORMANT_ES1_BIT|gconfig_value_CONFORMANT_ES2_BIT
                |gconfig_value_CONFORMANT_OPENGL_BIT|gconfig_value_CONFORMANT_OPENVG_BIT))) {
         return EINVAL;
      }
      tuple[0] = EGL_CONFORMANT;
      static_assert( EGL_OPENGL_ES_BIT     == gconfig_value_CONFORMANT_ES1_BIT
                     && EGL_OPENVG_BIT     == gconfig_value_CONFORMANT_OPENVG_BIT
                     && EGL_OPENGL_ES2_BIT == gconfig_value_CONFORMANT_ES2_BIT
                     && EGL_OPENGL_BIT     == gconfig_value_CONFORMANT_OPENGL_BIT, "no conversion needed");
      tuple[1] = value;
      break;
   default:
      return EINVAL;
   }

   return 0;
}

static int convertConfigListToEGL_eglconfig(/*out*/EGLint (*egl_attrib_list)[2*gconfig_NROFELEMENTS], const int32_t config_attributes[])
{
   int err;
   unsigned idx;

   for (idx = 0; config_attributes[idx] != gconfig_NONE; idx += 2) {
      if (idx >= lengthof(*egl_attrib_list)-2) {
         return E2BIG;
      }

      err = convertAttribToEGL_eglconfig(config_attributes[idx], config_attributes[idx+1], &(*egl_attrib_list)[idx]);
      if (err) return err;
   }

   (*egl_attrib_list)[idx] = EGL_NONE;
   return 0;
}

// group: lifetime

int init_eglconfig(/*out*/eglconfig_t * eglconf, opengl_display_t * egldisp, const int32_t config_attributes[])
{
   int err;
   EGLint   egl_attrib_list[2*gconfig_NROFELEMENTS];

   err = convertConfigListToEGL_eglconfig(&egl_attrib_list, config_attributes);
   if (err) goto ONABORT;

   EGLint      num_config;
   EGLConfig   eglconfig;
   EGLBoolean  isOK = eglChooseConfig( egldisp,
                                       egl_attrib_list, &eglconfig, 1, &num_config);

   if (!isOK) {
      err = EINVAL;
      goto ONABORT;
   }

   if (!num_config) {
      return ESRCH;
   }

   *eglconf = eglconfig;

   return 0;
ONABORT:
   TRACEABORT_ERRLOG(err);
   return err;
}

int initfiltered_eglconfig(/*out*/eglconfig_t * eglconf, struct opengl_display_t * egldisp, const int32_t config_attributes[], eglconfig_filter_f filter, void * user)
{
   int err;
   EGLint   egl_attrib_list[2*gconfig_NROFELEMENTS];

   err = convertConfigListToEGL_eglconfig(&egl_attrib_list, config_attributes);
   if (err) goto ONABORT;

   // TODO: implement tempstack_memory_allocator
   //       allocate memory from tempstack instead of real stack

   EGLint      num_config;
   EGLConfig   eglconfig[256];
   EGLBoolean  isOK = eglChooseConfig( egldisp,
                                       egl_attrib_list, eglconfig, lengthof(eglconfig), &num_config);

   if (!isOK) {
      err = EINVAL;
      goto ONABORT;
   }

   EGLint i;
   for (i = 0; i < num_config; ++i) {
      EGLint visualid;
      isOK = eglGetConfigAttrib( egldisp,
                                 eglconfig[i], EGL_NATIVE_VISUAL_ID, &visualid);
      if (filter(eglconfig[i], visualid, user)) break;
   }

   if (i == num_config) {
      return ESRCH;
   }

   *eglconf = eglconfig[i];

   // free possible memory (in case of tempstack)

   return 0;
ONABORT:
   TRACEABORT_ERRLOG(err);
   // free possible memory (in case of tempstack)
   return err;
}

int initfromconfigid_eglconfig(/*out*/eglconfig_t * eglconf, struct opengl_display_t * egldisp, const uint32_t id)
{
   int err;
   EGLint egl_attrib_list[] = {
      EGL_CONFIG_ID, (int32_t) id,
      EGL_NONE
   };

   EGLint      num_config;
   EGLConfig   eglconfig[1];
   EGLBoolean  isOK = eglChooseConfig( egldisp,
                                       egl_attrib_list, eglconfig, lengthof(eglconfig), &num_config);

   if (!isOK) {
      err = EINVAL;
      goto ONABORT;
   }

   if (0 == num_config) {
      return ESRCH;
   }

   *eglconf = eglconfig[0];

   return 0;
ONABORT:
   TRACEABORT_ERRLOG(err);
   return err;
}

// group: group

#define GETVALUE(eglconf, egldisp, attr,/*out*/value) \
         {                                                        \
            static_assert(sizeof(int32_t) == sizeof(EGLint), "conversion without changing value"); \
            EGLBoolean isOK = eglGetConfigAttrib(egldisp, eglconf, attr, value); \
            if (!isOK) {                                          \
               err = aserrcode_egl(eglGetError());                \
               goto ONABORT;                                      \
            }                                                     \
         }

int value_eglconfig(eglconfig_t eglconf, opengl_display_t * egldisp, const int32_t attribute, /*out*/int32_t * value)
{
   EGLint eglattrib[2];
   int err;

   err = convertAttribToEGL_eglconfig(attribute, 1, eglattrib);
   if (err) goto ONABORT;

   GETVALUE(eglconf, egldisp, eglattrib[0], value);

   // no conversion of values needed: see convertAttribToEGL_eglconfig

   if (gconfig_TYPE == attribute) {
      // but this value must be masked (cause not all bits are supported at the moment)
      *value &= ( gconfig_value_TYPE_PBUFFER_BIT
                  | gconfig_value_TYPE_PIXMAP_BIT
                  | gconfig_value_TYPE_WINDOW_BIT);
   }

   return 0;
ONABORT:
   TRACEABORT_ERRLOG(err);
   return err;
}

int visualconfigid_eglconfig(eglconfig_t eglconf, struct opengl_display_t * egldisp, /*out*/int32_t * visualid)
{
   int err;

   GETVALUE(eglconf, egldisp, EGL_NATIVE_VISUAL_ID, visualid);

   return 0;
ONABORT:
   TRACEABORT_ERRLOG(err);
   return err;
}

int configid_eglconfig(eglconfig_t eglconf, struct opengl_display_t * egldisp, /*out*/uint32_t * id)
{
   int err;

   GETVALUE(eglconf, egldisp, EGL_CONFIG_ID, (int32_t*)id);

   return 0;
ONABORT:
   TRACEABORT_ERRLOG(err);
   return err;
}

int maxpbuffer_eglconfig(eglconfig_t eglconf, struct opengl_display_t * egldisp, /*out*/uint32_t * maxwidth, /*out*/uint32_t * maxheight, /*out*/uint32_t * maxpixels)
{
   int err;

   if (maxwidth) {
      GETVALUE(eglconf, egldisp, EGL_MAX_PBUFFER_WIDTH, (int32_t*)maxwidth);
   }

   if (maxheight) {
      GETVALUE(eglconf, egldisp, EGL_MAX_PBUFFER_HEIGHT, (int32_t*)maxheight);
   }

   if (maxpixels) {
      GETVALUE(eglconf, egldisp, EGL_MAX_PBUFFER_PIXELS, (int32_t*)maxpixels);
   }

   return 0;
ONABORT:
   TRACEABORT_ERRLOG(err);
   return err;
}


// group: test

#ifdef KONFIG_UNITTEST

static egldisplay_t  s_filter_display;
static int32_t       s_filter_visualid;
static void *        s_filter_user;
static int           s_filter_total_count;
static int           s_filter_valid_count;

static bool filter_test_count(eglconfig_t eglconf, int32_t visualid, void * user)
{
   int32_t visualid2 = -1;
   s_filter_valid_count += (
         eglconf != 0
         && (0 == visualconfigid_eglconfig(eglconf, s_filter_display, &visualid2))
         && visualid         == visualid2
         && s_filter_user    == user);
   s_filter_total_count += 1;
   return false;
}

static bool filter_test_select(eglconfig_t eglconf, int32_t visualid, void * user)
{
   (void) eglconf;
   s_filter_visualid = visualid;
   return (--*(int*)user) == 0;
}

static bool filter_test_attribon(eglconfig_t eglconf, int32_t visualid, void * user)
{
   (void) visualid;
   int attrvalue = 0;
   s_filter_valid_count += (
         0 == value_eglconfig(eglconf, s_filter_display, *(int*)user, &attrvalue)
         && attrvalue > 0);
   s_filter_total_count += 1;
   return false;
}

static bool filter_test_attriboff(eglconfig_t eglconf, int32_t visualid, void * user)
{
   (void) visualid;
   int attrvalue = -1;
   s_filter_valid_count += (
         0 == value_eglconfig(eglconf, s_filter_display, *(int*)user, &attrvalue)
         && attrvalue == 0);
   s_filter_total_count += 1;
   return false;
}

static int test_initfree(egldisplay_t egldisp)
{
   eglconfig_t eglconf = eglconfig_FREE;
   int32_t     attrlist[2*gconfig_NROFELEMENTS+1];

   // TEST eglconfig_FREE
   TEST(0 == eglconf);

   // TEST init_eglconfig: EINVAL (egldisplay_t not initialized)
   TEST(EINVAL == init_eglconfig(&eglconf, egldisplay_FREE, (int[]) { gconfig_BITS_RED, 1, gconfig_NONE}));
   TEST(0 == eglconf);

   // TEST init_eglconfig: EINVAL (values in config_attributes wrong)
   const int32_t errattr1[][3] = {
      { gconfig_NROFELEMENTS/*!*/, 1, gconfig_NONE },
      { gconfig_TYPE, 0x0f/*!*/, gconfig_NONE },
      { gconfig_CONFORMANT, 0x1f/*!*/, gconfig_NONE }
   };
   for (unsigned i = 0; i < lengthof(errattr1); ++i) {
      TEST(EINVAL == init_eglconfig(&eglconf, egldisp, errattr1[i]));
   }

   // TEST init_eglconfig: E2BIG (config_attributes list too long)
   memset(attrlist, gconfig_NONE, sizeof(attrlist));
   for (int i = 0; i < (int)lengthof(attrlist)-2; i += 2) {
      attrlist[i]   = 1 + (i % (gconfig_NROFELEMENTS-1));
      attrlist[i+1] = 1;
   }
   TEST(E2BIG == init_eglconfig(&eglconf, egldisp, attrlist));
   TEST(0 == eglconf);

   // TEST init_eglconfig: ESRCH (no 1024 bit buffer size)
   attrlist[0] = gconfig_BITS_BLUE;
   attrlist[1] = 1024;
   attrlist[2] = gconfig_NONE;
   TEST(ESRCH == init_eglconfig(&eglconf, egldisp, attrlist));
   TEST(0 == eglconf);

   // TEST init_eglconfig: all gconfig_XXX supported
   for (int i = 1; i < gconfig_NROFELEMENTS; ++i) {
      int32_t value = 1;
      switch (i) {
      case gconfig_TYPE:
         value = gconfig_value_TYPE_WINDOW_BIT
               | gconfig_value_TYPE_PBUFFER_BIT
               | gconfig_value_TYPE_PIXMAP_BIT;
         break;
      case gconfig_CONFORMANT:
         value = gconfig_value_CONFORMANT_ES1_BIT
               | gconfig_value_CONFORMANT_ES2_BIT
               | gconfig_value_CONFORMANT_OPENGL_BIT
               | gconfig_value_CONFORMANT_OPENVG_BIT;
         break;
      }
      attrlist[2*i-2] = i;
      attrlist[2*i-1] = value;
   }
   static_assert(2*gconfig_NROFELEMENTS-2 < lengthof(attrlist), "enough space for all attributes");
   attrlist[2*gconfig_NROFELEMENTS-2] = gconfig_NONE;
   TEST(0 == init_eglconfig(&eglconf, egldisp, attrlist));
   TEST(0 != eglconf);
   TEST(0 == free_eglconfig(&eglconf));
   TEST(0 == eglconf);

   // TEST init_eglconfig, value_eglconfig: returned config supports the number of queried bits or more
   int onoff[] = {
      gconfig_BITS_ALPHA, gconfig_BITS_DEPTH, gconfig_BITS_STENCIL
   };
   for (unsigned i = 0; i < lengthof(onoff); ++i) {
      for (int isOn = 0; isOn <= 1; ++isOn) {
         attrlist[0] = onoff[i];
         attrlist[1] = isOn;
         attrlist[2] = gconfig_NONE;
         TEST(0 == init_eglconfig(&eglconf, egldisp, attrlist));
         int32_t attrval = -1;
         TEST(0 == value_eglconfig(eglconf, egldisp, onoff[i], &attrval));
         TEST(isOn == (0 < attrval));
         TEST(0 == free_eglconfig(&eglconf));
      }
   }

   // TEST init_eglconfig, value_eglconfig: gconfig_CONFORMANT
   static_assert(gconfig_value_CONFORMANT_ES1_BIT == 1 && gconfig_value_CONFORMANT_OPENGL_BIT == 8, "used in for");
   for (int bit = gconfig_value_CONFORMANT_ES1_BIT; bit <= gconfig_value_CONFORMANT_OPENGL_BIT; bit *= 2) {
      attrlist[0] = gconfig_CONFORMANT;
      attrlist[1] = bit;
      attrlist[2] = gconfig_NONE;
      TEST(0 == init_eglconfig(&eglconf, egldisp, attrlist));
      int32_t attrval = 0;
      TEST(0 == value_eglconfig(eglconf, egldisp, gconfig_CONFORMANT, &attrval));
      TEST(0 != (bit & attrval));
      TEST(0 == free_eglconfig(&eglconf));
   }

   // TEST init_eglconfig, value_eglconfig: gconfig_TYPE
   static_assert(gconfig_value_TYPE_PBUFFER_BIT == 1 && gconfig_value_TYPE_WINDOW_BIT == 4, "used in for");
   for (int bit = gconfig_value_TYPE_PBUFFER_BIT; bit <= gconfig_value_TYPE_WINDOW_BIT; bit *= 2) {
      attrlist[0] = gconfig_TYPE;
      attrlist[1] = bit;
      attrlist[2] = gconfig_NONE;
      TEST(0 == init_eglconfig(&eglconf, egldisp, attrlist));
      int32_t attrval = 0;
      TEST(0 == value_eglconfig(eglconf, egldisp, gconfig_TYPE, &attrval));
      TEST(0 != (bit & attrval));
      TEST(0 == free_eglconfig(&eglconf));
   }
   TEST(0 == free_eglconfig(&eglconf));
   TEST(0 == eglconf);

   // TEST initfiltered_eglconfig: filter is called for all entries
   EGLint num_config = 0;
   TEST(EGL_TRUE == eglChooseConfig(egldisp, (EGLint[]){EGL_NONE}, 0, 0, &num_config));
   TEST(num_config > 1);
   attrlist[0] = gconfig_NONE;
   s_filter_display = egldisp;
   s_filter_user    = &num_config;
   s_filter_total_count = 0;
   s_filter_valid_count = 0;
   TEST(ESRCH == initfiltered_eglconfig(&eglconf, egldisp, attrlist, &filter_test_count, &num_config));
   TEST(s_filter_valid_count == s_filter_total_count);
   TEST(s_filter_valid_count == num_config);
   TEST(0 == eglconf);

   // TEST initfiltered_eglconfig: use visualid for which filter signals true
   for (int i = s_filter_total_count; i > 0; --i) {
      int select_count = i; // use ith id
      attrlist[0] = gconfig_NONE;
      TEST(0 == initfiltered_eglconfig(&eglconf, egldisp, attrlist, &filter_test_select, &select_count));
      TEST(0 != eglconf);
      TEST(0 == select_count);
      int32_t visualid = -1;
      TEST(0 == visualconfigid_eglconfig(eglconf, egldisp, &visualid));
      TEST(s_filter_visualid == visualid);
      TEST(0 == free_eglconfig(&eglconf));
      TEST(0 == eglconf);
   }

   // TEST initfiltered_eglconfig: only valid configurations (attribute on and off)
   for (unsigned i = 0; i < lengthof(onoff); ++i) {
      for (int isOn = 0; isOn <= 1; ++isOn) {
         attrlist[0] = onoff[i];
         attrlist[1] = isOn;
         attrlist[2] = gconfig_NONE;
         s_filter_total_count = 0;
         s_filter_valid_count = 0;
         s_filter_display     = egldisp;
         TEST(ESRCH == initfiltered_eglconfig(&eglconf, egldisp, attrlist,
                                       isOn ? &filter_test_attribon : &filter_test_attriboff, &onoff[i]));
         TEST(s_filter_valid_count >= 1);
         if (isOn) {
            TEST(s_filter_valid_count == s_filter_total_count);
         } else {
            TEST(s_filter_valid_count <  s_filter_total_count);
         }
         TEST(0 == free_eglconfig(&eglconf));
      }
   }

   // TEST initfromconfigid_eglconfig
   for (unsigned i = 0; i < lengthof(onoff); ++i) {
      for (int isOn = 0; isOn <= 1; ++isOn) {
         EGLint configid1 = 0;
         EGLint configid2 = 0;
         attrlist[0] = onoff[i];
         attrlist[1] = isOn;
         attrlist[2] = gconfig_NONE;
         TEST(0 == init_eglconfig(&eglconf, egldisp, attrlist));
         TEST(0 != eglGetConfigAttrib(egldisp, eglconf, EGL_CONFIG_ID, &configid1));
         TEST(0 == free_eglconfig(&eglconf));
         // initfromconfigid_eglconfig
         TEST(0 == initfromconfigid_eglconfig(&eglconf, egldisp, (uint32_t) configid1));
         TEST(0 != eglGetConfigAttrib(egldisp, eglconf, EGL_CONFIG_ID, &configid2));
         TEST(configid1 == configid2);
         TEST(0 == free_eglconfig(&eglconf));
      }
   }

   // TEST initfromconfigid_eglconfig
   TEST(ESRCH == initfromconfigid_eglconfig(&eglconf, egldisp, INT32_MAX));

   return 0;
ONABORT:
   free_eglconfig(&eglconf);
   return EINVAL;
}

static int test_query(egldisplay_t egldisp)
{
   eglconfig_t eglconf = eglconfig_FREE;
   int32_t     attrlist[10];

   for (int i = 8; i <= 32; i += 8) {
      for (int t = 0; t < 3; ++ t) {
         int32_t B;
         uint32_t maxwidth = 0;
         uint32_t maxheight = 0;
         uint32_t maxpixels = 0;

         attrlist[0] = gconfig_BITS_BUFFER;
         attrlist[1] = i;
         attrlist[2] = gconfig_TYPE;
         attrlist[3] = 1 << t;
         attrlist[4] = gconfig_NONE;
         TEST(0 == init_eglconfig(&eglconf, egldisp, attrlist));
         int32_t attrvalue = 0;

         // TEST value_eglconfig
         TEST(0 == value_eglconfig(eglconf, egldisp, gconfig_BITS_BUFFER, &attrvalue));
         TEST(attrvalue >= i);
         TEST(0 == value_eglconfig(eglconf, egldisp, gconfig_CONFORMANT, &attrvalue));
         B = gconfig_value_CONFORMANT_ES1_BIT
             | gconfig_value_CONFORMANT_ES2_BIT
             | gconfig_value_CONFORMANT_OPENGL_BIT
             | gconfig_value_CONFORMANT_OPENVG_BIT;
         TEST(B >= attrvalue);
         TEST(0 == value_eglconfig(eglconf, egldisp, gconfig_TYPE, &attrvalue));
         B = gconfig_value_TYPE_PBUFFER_BIT
             | gconfig_value_TYPE_PIXMAP_BIT
             | gconfig_value_TYPE_WINDOW_BIT;
         TEST(B >= attrvalue);
         TEST(0 != ((1 << t) && attrvalue));

         // TEST visualconfigid_eglconfig
         if (0 != (attrvalue & gconfig_value_TYPE_WINDOW_BIT)) {
            int32_t visualid = -1;
            TEST(0 == visualconfigid_eglconfig(eglconf, egldisp, &visualid));
            TEST(0 < visualid);
         }

         if (0 != (attrvalue & gconfig_value_TYPE_PBUFFER_BIT)) {
            // TEST maxpbuffer_eglconfig
            TEST(0 == maxpbuffer_eglconfig(eglconf, egldisp, &maxwidth, &maxheight, &maxpixels));
            TEST(0 < maxheight);
            TEST(0 < maxwidth);
            TEST(0 < maxpixels);
            TEST(maxpixels > maxheight);
            TEST(maxpixels > maxwidth);
            TEST(maxwidth * maxheight >= maxpixels);

            // TEST maxpbuffer_eglconfig: 0 values for out parameters supported
            TEST(0 == maxpbuffer_eglconfig(eglconf, egldisp, 0, 0, 0));
            maxwidth = maxheight = maxpixels = 0;
            TEST(0 == maxpbuffer_eglconfig(eglconf, egldisp, &maxwidth, 0, 0));
            TEST(0 < maxwidth);
            TEST(0 == maxpbuffer_eglconfig(eglconf, egldisp, 0, &maxheight, 0));
            TEST(0 < maxheight);
            TEST(0 == maxpbuffer_eglconfig(eglconf, egldisp, 0, 0, &maxpixels));
            TEST(0 < maxpixels);
         }

         // TEST configid_eglconfig
         EGLint configid = -1;
         TEST(0 != eglGetConfigAttrib(egldisp, eglconf, EGL_CONFIG_ID, &configid));
         uint32_t id = (uint32_t) (configid + 1);
         TEST(0 == configid_eglconfig(eglconf, egldisp, &id));
         TEST(id == (uint32_t)configid);

         TEST(0 == free_eglconfig(&eglconf));
      }
   }


   return 0;
ONABORT:
   free_eglconfig(&eglconf);
   return EINVAL;
}

static int childprocess_unittest(void)
{
   resourceusage_t   usage   = resourceusage_FREE;
   egldisplay_t      egldisp = egldisplay_FREE;

   TEST(0 == initdefault_egldisplay(&egldisp));

   TEST(0 == init_resourceusage(&usage));

   if (test_initfree(egldisp))   goto ONABORT;
   if (test_query(egldisp))      goto ONABORT;

   TEST(0 == same_resourceusage(&usage));
   TEST(0 == free_resourceusage(&usage));

   TEST(0 == free_egldisplay(&egldisp));

   return 0;
ONABORT:
   (void) free_resourceusage(&usage);
   (void) free_egldisplay(&egldisp);
   return EINVAL;
}

int unittest_platform_opengl_egl_eglconfig()
{
   int err;

   TEST(0 == execasprocess_unittest(&childprocess_unittest, &err));

   return err;
ONABORT:
   return EINVAL;
}

#endif
