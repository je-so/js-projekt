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
#include "C-kern/api/graphic/surface_config.h"
#include "C-kern/api/platform/OpenGL/EGL/egldisplay.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/test/resourceusage.h"
#endif
#include STR(C-kern/api/platform/KONFIG_OS/graphic/sysegl.h)


// section: eglconfig_t

// group: variables

static int convertIntoEGL_eglconfig(int attribute, int value, /*out*/int tuple[2])
{
   switch (attribute) {
   case surface_config_TYPE:
      if ( (value
            & ~(surface_configvalue_TYPE_WINDOW_BIT|surface_configvalue_TYPE_PBUFFER_BIT|surface_configvalue_TYPE_PIXMAP_BIT))) {
         return EINVAL;
      }
      tuple[0] = EGL_SURFACE_TYPE;
      static_assert( EGL_PBUFFER_BIT   == surface_configvalue_TYPE_PBUFFER_BIT
                     && EGL_PIXMAP_BIT == surface_configvalue_TYPE_PIXMAP_BIT
                     && EGL_WINDOW_BIT == surface_configvalue_TYPE_WINDOW_BIT, "no conversion needed");
      tuple[1] = value;
      break;
   case surface_config_TRANSPARENT_ALPHA:
      tuple[0] = EGL_ALPHA_SIZE;
      tuple[1] = (value != 0);
      break;
   case surface_config_BITS_RED:
      tuple[0] = EGL_RED_SIZE;
      tuple[1] = value;
      break;
   case surface_config_BITS_GREEN:
      tuple[0] = EGL_GREEN_SIZE;
      tuple[1] = value;
      break;
   case surface_config_BITS_BLUE:
      tuple[0] = EGL_BLUE_SIZE;
      tuple[1] = value;
      break;
   case surface_config_BITS_ALPHA:
      tuple[0] = EGL_ALPHA_SIZE;
      tuple[1] = value;
      break;
   case surface_config_BITS_DEPTH:
      tuple[0] = EGL_DEPTH_SIZE;
      tuple[1] = value;
      break;
   case surface_config_BITS_STENCIL:
      tuple[0] = EGL_STENCIL_SIZE;
      tuple[1] = value;
      break;
   case surface_config_CONFORMANT:
      if ( (value
            & ~(surface_configvalue_CONFORMANT_ES1_BIT|surface_configvalue_CONFORMANT_ES2_BIT
                |surface_configvalue_CONFORMANT_OPENGL_BIT|surface_configvalue_CONFORMANT_OPENVG_BIT))) {
         return EINVAL;
      }
      tuple[0] = EGL_CONFORMANT;
      static_assert( EGL_OPENGL_ES_BIT     == surface_configvalue_CONFORMANT_ES1_BIT
                     && EGL_OPENVG_BIT     == surface_configvalue_CONFORMANT_OPENVG_BIT
                     && EGL_OPENGL_ES2_BIT == surface_configvalue_CONFORMANT_ES2_BIT
                     && EGL_OPENGL_BIT     == surface_configvalue_CONFORMANT_OPENGL_BIT, "no conversion needed");
      tuple[1] = value;
      break;
   default:
      return EINVAL;
   }

   return 0;
}

// group: lifetime

int init_eglconfig(/*out*/eglconfig_t * eglconf, void * egldisp, const int config_attributes[])
{
   int err;
   EGLint   egl_attrib_list[20];
   unsigned idx;

   for (idx = 0; config_attributes[idx] != surface_config_NONE; idx += 2) {
      if (idx >= lengthof(egl_attrib_list)-2) {
         err = E2BIG;
         goto ONABORT;
      }

      err = convertIntoEGL_eglconfig(config_attributes[idx], config_attributes[idx+1], &egl_attrib_list[idx]);
      if (err) goto ONABORT;
   }

   egl_attrib_list[idx] = EGL_NONE;

   EGLConfig   eglconfig;
   EGLint      num_config;
   EGLBoolean  isOK = eglChooseConfig( egldisp,
                                       egl_attrib_list, &eglconfig, 1, &num_config);

   if (!isOK) {
      err = EINVAL;
      goto ONABORT;
   }

   if (!num_config) {
      err = ESRCH;
      goto ONABORT;
   }

   *eglconf = eglconfig;

   return 0;
ONABORT:
   TRACEABORT_ERRLOG(err);
   return err;
}


// group: group

int value_eglconfig(eglconfig_t eglconf, void * egldisp, const int attribute, /*out*/int * value)
{
   int eglattrib[2];
   int err;

   err = convertIntoEGL_eglconfig(attribute, 1, eglattrib);
   if (err) goto ONABORT;

   EGLBoolean isOK = eglGetConfigAttrib(
                        egldisp, eglconf, eglattrib[0], value);
   if (!isOK) {
      err = EINVAL;
      goto ONABORT;
   }

   if (surface_config_TYPE == attribute) {
      // bits corresponds see convertIntoEGL_eglconfig
      *value &= ( surface_configvalue_TYPE_PBUFFER_BIT
                  | surface_configvalue_TYPE_PIXMAP_BIT
                  | surface_configvalue_TYPE_WINDOW_BIT);
   }

   return 0;
ONABORT:
   TRACEABORT_ERRLOG(err);
   return err;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(egldisplay_t egldisp)
{
   eglconfig_t eglconf = eglconfig_INIT_FREEABLE;
   int attrlist[21];

   // TEST eglconfig_INIT_FREEABLE
   TEST(0 == eglconf);

   // TEST init_eglconfig: EINVAL (egldisplay_t not initialized)
   TEST(EINVAL == init_eglconfig(&eglconf, egldisplay_INIT_FREEABLE, (int[]) { surface_config_BITS_RED, 1, surface_config_NONE}));
   TEST(0 == eglconf);

   // TEST init_eglconfig: EINVAL (values in config_attributes wrong)
   const int errattr1[][3] = {
      { surface_config_NROFCONFIGS/*!*/, 1, surface_config_NONE },
      { surface_config_TYPE, 0x0f/*!*/, surface_config_NONE },
      { surface_config_CONFORMANT, 0x1f/*!*/, surface_config_NONE }
   };
   for (unsigned i = 0; i < lengthof(errattr1); ++i) {
      TEST(EINVAL == init_eglconfig(&eglconf, egldisp, errattr1[i]));
   }

   // TEST init_eglconfig: E2BIG (config_attributes list too long)
   memset(attrlist, surface_config_NONE, sizeof(attrlist));
   for (int i = 0; i < (int)lengthof(attrlist)-2; i += 2) {
      attrlist[i]   = 1 + (i % (surface_config_NROFCONFIGS-1));
      attrlist[i+1] = 1;
   }
   TEST(E2BIG == init_eglconfig(&eglconf, egldisp, attrlist));
   TEST(0 == eglconf);

   // TEST init_eglconfig: ESRCH (no 1024 bit buffer size)
   attrlist[0] = surface_config_BITS_BLUE;
   attrlist[1] = 1024;
   attrlist[2] = surface_config_NONE;
   TEST(ESRCH == init_eglconfig(&eglconf, egldisp, attrlist));
   TEST(0 == eglconf);

   // TEST init_eglconfig: all surface_config_XXX supported
   for (int i = 1; i < surface_config_NROFCONFIGS; ++i) {
      int value = 1;
      switch (i) {
      case surface_config_TYPE:
         value = surface_configvalue_TYPE_WINDOW_BIT
               | surface_configvalue_TYPE_PBUFFER_BIT
               | surface_configvalue_TYPE_PIXMAP_BIT;
         break;
      case surface_config_CONFORMANT:
         value = surface_configvalue_CONFORMANT_ES1_BIT
               | surface_configvalue_CONFORMANT_ES2_BIT
               | surface_configvalue_CONFORMANT_OPENGL_BIT
               | surface_configvalue_CONFORMANT_OPENVG_BIT;
         break;
      }
      attrlist[2*i-2] = i;
      attrlist[2*i-1] = value;
   }
   static_assert(2*surface_config_NROFCONFIGS-2 < lengthof(attrlist), "enough space for all attributes");
   attrlist[2*surface_config_NROFCONFIGS-2] = surface_config_NONE;
   TEST(0 == init_eglconfig(&eglconf, egldisp, attrlist));
   TEST(0 != eglconf);
   TEST(0 == free_eglconfig(&eglconf));
   TEST(0 == eglconf);

   // TEST init_eglconfig, value_eglconfig: returned config supports the number of queried bits or more
   int onoff[] = {
      surface_config_BITS_ALPHA, surface_config_BITS_DEPTH, surface_config_BITS_STENCIL
   };
   for (unsigned i = 0; i < lengthof(onoff); ++i) {
      for (int isOn = 0; isOn <= 1; ++isOn) {
         attrlist[0] = onoff[i];
         attrlist[1] = isOn;
         attrlist[2] = surface_config_NONE;
         TEST(0 == init_eglconfig(&eglconf, egldisp, attrlist));
         int attrval = -1;
         TEST(0 == value_eglconfig(eglconf, egldisp, onoff[i], &attrval));
         TEST(isOn == (0 < attrval));
         TEST(0 == free_eglconfig(&eglconf));
      }
   }

   // TEST init_eglconfig, value_eglconfig: surface_config_CONFORMANT
   static_assert(surface_configvalue_CONFORMANT_ES1_BIT == 1 && surface_configvalue_CONFORMANT_OPENGL_BIT == 8, "used in for");
   for (int bit = surface_configvalue_CONFORMANT_ES1_BIT; bit <= surface_configvalue_CONFORMANT_OPENGL_BIT; bit *= 2) {
      attrlist[0] = surface_config_CONFORMANT;
      attrlist[1] = bit;
      attrlist[2] = surface_config_NONE;
      TEST(0 == init_eglconfig(&eglconf, egldisp, attrlist));
      int attrval = 0;
      TEST(0 == value_eglconfig(eglconf, egldisp, surface_config_CONFORMANT, &attrval));
      TEST(0 != (bit & attrval));
      TEST(0 == free_eglconfig(&eglconf));
   }

   // TEST init_eglconfig, value_eglconfig: surface_config_TYPE
   static_assert(surface_configvalue_TYPE_PBUFFER_BIT == 1 && surface_configvalue_TYPE_WINDOW_BIT == 4, "used in for");
   for (int bit = surface_configvalue_TYPE_PBUFFER_BIT; bit <= surface_configvalue_TYPE_WINDOW_BIT; bit *= 2) {
      attrlist[0] = surface_config_TYPE;
      attrlist[1] = bit;
      attrlist[2] = surface_config_NONE;
      TEST(0 == init_eglconfig(&eglconf, egldisp, attrlist));
      int attrval = 0;
      TEST(0 == value_eglconfig(eglconf, egldisp, surface_config_TYPE, &attrval));
      TEST(0 != (bit & attrval));
      TEST(0 == free_eglconfig(&eglconf));
   }

   TEST(0 == free_eglconfig(&eglconf));
   TEST(0 == eglconf);

   return 0;
ONABORT:
   free_eglconfig(&eglconf);
   return EINVAL;
}

static int childprocess_unittest(void)
{
   resourceusage_t   usage   = resourceusage_INIT_FREEABLE;
   egldisplay_t      egldisp = egldisplay_INIT_FREEABLE;

   TEST(0 == initdefault_egldisplay(&egldisp));

   TEST(0 == init_resourceusage(&usage));

   if (test_initfree(egldisp))  goto ONABORT;

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
