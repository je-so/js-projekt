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

// forward
struct native_display_t;

/* typedef: struct native_surfconfig_t
 * Export <native_surfconfig_t > into global namespace. */
typedef struct native_surfconfig_t * native_surfconfig_t ;

/* typedef: struct eglconfig_t
 * Export <eglconfig_t> into global namespace. */
typedef struct native_surfconfig_t * eglconfig_t;

/* typedef: eglconfig_filter_f
 * Declares filter to select between different possible configuration.
 * The filter function must return true if it accepts the visual ID
 * given in parameter visualid else false.
 * If no visualid passes the filter <initfiltered_eglconfig> returns ESRCH. */
typedef bool (* eglconfig_filter_f) (eglconfig_t eglconf, struct native_display_t * egldisp, int32_t visualid, void * user);


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_platform_opengl_egl_eglconfig
 * Test <eglconfig_t> functionality. */
int unittest_platform_opengl_egl_eglconfig(void);
#endif


/* struct: eglconfig_t
 * Contains an EGL frame buffer configuration. */
typedef struct native_surfconfig_t * eglconfig_t;

// group: lifetime

/* define: eglconfig_INIT_FREEABLE
 * Static initializer. */
#define eglconfig_INIT_FREEABLE  0

/* function: init_eglconfig
 * Returns a surface (frame buffer) configuration which matches the given attributes.
 * The parameters stored in config_attributes must be tupels of type (<surfaceconfig_e> value, int value)
 * followed by the termination value <surfaceconfig_NONE>.
 *
 * Returns:
 * 0      - Success, eglconf is valid.
 * E2BIG  - Attributes list in config_attributes is too long, eglconf is not changed.
 * EINVAL - Either egldisp is invalid, an invalid <surfaceconfig_e> is supplied or the supplied integer value
 *          is invalid for the corresponding <surfaceconfig_e> attribute id.
 * ESRCH  - No configuration matches the supplied attribues.
 * */
int init_eglconfig(/*out*/eglconfig_t * eglconf, struct native_display_t * egldisp, const int32_t config_attributes[]);

/* function: initfiltered_eglconfig
 * Same as <init_eglconfig> except that more than one possible configuration is considered.
 * The provided filter is repeatedly called for every possible configuration as long as it returns false.
 * The first configuration which passes the filter is used.
 * Parameter user is passed as user data into the filter function. */
int initfiltered_eglconfig(/*out*/eglconfig_t * eglconf, struct native_display_t * egldisp, const int32_t config_attributes[], eglconfig_filter_f filter, void * user);

/* function: free_eglconfig
 * Frees any associated resources.
 * Currently this is a no-op. */
int free_eglconfig(eglconfig_t * eglconf);

// group: query

/* function: value_eglconfig
 * Returns the value of attribute.
 * The parameter egldisp must be of type <egldisplay_t> and parameter attribute must be a value from <surfaceconfig_e>.
 * On error EINVAL is returned else 0. */
int value_eglconfig(eglconfig_t eglconf, struct native_display_t * egldisp, const int32_t attribute, /*out*/int32_t * value);

/* function: visualid_eglconfig
 * Returns the native visualid of the configuration.
 * Use this id to create a native window with the correct
 * surface graphic attributes. */
int visualid_eglconfig(eglconfig_t eglconf, struct native_display_t * egldisp, /*out*/int32_t * visualid);

// section: inline implementation

/* define: free_eglconfig
 * Implements <eglconfig_t.free_eglconfig>. */
#define free_eglconfig(eglconf) \
         (*(eglconf) = eglconfig_INIT_FREEABLE, 0)

#endif
