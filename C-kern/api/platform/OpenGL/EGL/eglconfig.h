/* title: EGL-Framebuffer-Configuration

   Describes a frame buffer / surface configuration for
   a given <egldisplay_t>.

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
#ifndef CKERN_PLATFORM_OPENGL_EGL_EGLCONFIG_HEADER
#define CKERN_PLATFORM_OPENGL_EGL_EGLCONFIG_HEADER

/* typedef: struct eglconfig_t
 * Export <eglconfig_t> into global namespace. */
typedef void * eglconfig_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_platform_opengl_egl_eglconfig
 * Test <eglconfig_t> functionality. */
int unittest_platform_opengl_egl_eglconfig(void);
#endif


/* struct: eglconfig_t
 * Contains an EGL frame buffer configuration. */
typedef void * eglconfig_t;

// group: lifetime

/* define: eglconfig_INIT_FREEABLE
 * Static initializer. */
#define eglconfig_INIT_FREEABLE  0

/* function: init_eglconfig
 * Returns a surface (frame buffer) configuration which matches the given attributes.
 * The parameters stored in config_attributes must be tupels of type (<surface_config_e> value, int value)
 * followed by the termination value <surface_config_NONE>.
 *
 * Returns:
 * 0      - Success, eglconf is valid.
 * E2BIG  - Attributes list in config_attributes is too long, eglconf is not changed.
 * EINVAL - Either egldisp is invalid, an invalid <surface_config_e> is supplied or the supplied integer value
 *          is invalid for the corresponding <surface_config_e> attribute id.
 * ESRCH  - No configuration matches the supplied attribues.
 * */
int init_eglconfig(/*out*/eglconfig_t * eglconf, void * egldisp, const int config_attributes[]);

/* function: free_eglconfig
 * Frees any associated resources.
 * Currently this is a no-op. */
int free_eglconfig(eglconfig_t * eglconf);

// group: query

/* function: value_eglconfig
 * Returns the value of attribute.
 * The parameter egldisp must be of type <egldisplay_t> and parameter attribute must be a value from <surface_config_e>.
 * On error EINVAL is returned else 0. */
int value_eglconfig(eglconfig_t eglconf, void * egldisp, int attribute, /*out*/int * value);


// section: inline implementation

/* define: free_eglconfig
 * Implements <eglconfig_t.free_eglconfig>. */
#define free_eglconfig(eglconf) \
         (*(eglconf) = eglconfig_INIT_FREEABLE, 0)

#endif
