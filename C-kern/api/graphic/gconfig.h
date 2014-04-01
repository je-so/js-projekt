/* title: Graphic-Surface-Configuration

   A configuration which describes the capabilities of an OpenGL graphic surface.
   The configuration is used during construction of a surface.
   A surface supports additional configuration attributes
   which are specified during surface construction.

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

   file: C-kern/api/graphic/gconfig.h
    Header file <Graphic-Surface-Configuration>.

   file: C-kern/graphic/gconfig.c
    Implementation file <Graphic-Surface-Configuration impl>.
*/
#ifndef CKERN_GRAPHIC_SURFACE_CONFIG_HEADER
#define CKERN_GRAPHIC_SURFACE_CONFIG_HEADER

// forward
struct opengl_config_t;
struct display_t;

#if defined(KONFIG_USERINTERFACE_EGL)
#include "C-kern/api/platform/OpenGL/EGL/eglconfig.h"
#endif

/* typedef: struct gconfig_t
 * Export <gconfig_t> into global namespace. */
typedef struct gconfig_t gconfig_t;

/* typedef: gconfig_filter_t
 * Declares filter to select between different possible configuration.
 * The filter function must return true if it accepts the visual ID
 * given in parameter visualid else false.
 * If no visualid passes the filter <initfiltered_eglconfig> returns ESRCH. */
typedef struct gconfig_filter_t gconfig_filter_t;


/* enums: gconfig_e
 * gconfig_NONE         - This is the last entry in a list to indicate termination.
 * gconfig_TYPE         - The configuration supports drawing into a specific surface type.
 *                        Supported values are any combination of bits ored together from
 *                        <gconfig_value_TYPE_WINDOW_BIT>, ... .
 * gconfig_TRANSPARENT_ALPHA - A value != 0 sets a flag which makes the background of a window surface shine through.
 *                        Default value is 0.
 *                        The alpha value of a pixel determines its opacity.
 *                        Use this to overlay a window surface to another window surface.
 *                        This attribute must be supplied during creation of a window to enable alpha transparency.
 *                        An alpha value of 1 means the pixel is fully opaque.
 *                        An alpha value of 0 means the pixel is fully transparent so the background
 *                        of a window is visible with 100%.
 *                        If the alpha channel has 8 bits then the value 255 corresponds to 1 and the value 128 to 0.5.
 *                        Blending function on X11:
 *                        The blending function assumes that every pixel's color value is premultiplied by alpha.
 *                        Screen-Pixel-RGB = Window-Pixel-RGB + (1 - Window-Pixel-Alpha) * Background-Pixel-RGB
 *                        Before drawing you need to change the color of the pixels to:
 *                        Window-Pixel-RGB = Window-Pixel-RGB * Window-Pixel-Alpha
 * gconfig_BITS_BUFFER  - The minimum number of bits per pixel for all color channels together including alpha.
 * gconfig_BITS_RED     - The minimum number of bits per pixel which determine the red color.
 *                        The number of red bits the color buffer supports, e.g. 8 on current hardware.
 * gconfig_BITS_GREEN   - The minimum number of bits per pixel which determine the green color.
 *                        The number of green bits the color buffer supports, e.g. 8 on current hardware.
 * gconfig_BITS_BLUE    - The minimum number of bits per pixel which determine the blue color.
 *                        The number of blue bits the color buffer supports, e.g. 8 on current hardware.
 * gconfig_BITS_ALPHA   - The minimum number of bits per pixel which determine the alpha value (1: fully opaque, 0: fully transparent).
 *                        The number of alpha bits the color buffer supports, e.g. 8 on current hardware.
 * gconfig_BITS_DEPTH   - The minimum number of bits the depth buffer must support. If set to 0 no depth buffer is supported.
 * gconfig_BITS_STENCIL - The minimum number of bits the stencil buffer must support. If set to 0 no stencil buffer is supported.
 * gconfig_CONFORMANT   - Determine the supported drawing APIs.
 *                        Supported values are any combination of bits ored together from
 *                        <gconfig_value_CONFORMANT_ES1_BIT>, <gconfig_value_CONFORMANT_ES2_BIT>,
 *                        <gconfig_value_CONFORMANT_OPENGL_BIT>, <gconfig_value_CONFORMANT_OPENVG_BIT>.
 * gconfig_NROFELEMENTS - Gives the number of all valid configuration options excluding <gconfig_NROFELEMENTS>.
 *
 * */
enum gconfig_e {
   gconfig_NONE,
   gconfig_TYPE,
   gconfig_TRANSPARENT_ALPHA,
   gconfig_BITS_BUFFER,
   gconfig_BITS_RED,
   gconfig_BITS_GREEN,
   gconfig_BITS_BLUE,
   gconfig_BITS_ALPHA,
   gconfig_BITS_DEPTH,
   gconfig_BITS_STENCIL,
   gconfig_CONFORMANT,
   gconfig_NROFELEMENTS
};

typedef enum gconfig_e gconfig_e;

/* enums: gconfig_value_e
 * gconfig_value_TYPE_PBUFFER_BIT  - The config supports drawing into a OpenGL (ES) pixel buffer surface.
 *                                   A pbuffer surface always supports single buffering.
 * gconfig_value_TYPE_PIXMAP_BIT   - The config supports drawing into a native pixmap surface.
 *                                   A pixmap surface always supports single buffering.
 * gconfig_value_TYPE_WINDOW_BIT   - The config supports drawing into a window surface.
 *                                   A window surface always supports double buffering.
 * gconfig_value_CONFORMANT_ES1_BIT    - Config supports creating OpenGL ES 1.0/1.1 graphic contexts.
 * gconfig_value_CONFORMANT_OPENVG_BIT - Config supports creating OpenVG graphic contexts.
 * gconfig_value_CONFORMANT_ES2_BIT    - Config supports creating OpenGL ES 2.0 graphic contexts.
 * gconfig_value_CONFORMANT_OPENGL_BIT - Config supports creating OpenGL graphic contexts.
 * */
enum gconfig_value_e {
   gconfig_value_TYPE_PBUFFER_BIT = 1,
   gconfig_value_TYPE_PIXMAP_BIT  = 2,
   gconfig_value_TYPE_WINDOW_BIT  = 4,
   gconfig_value_CONFORMANT_ES1_BIT    = 1,
   gconfig_value_CONFORMANT_OPENVG_BIT = 2,
   gconfig_value_CONFORMANT_ES2_BIT    = 4,
   gconfig_value_CONFORMANT_OPENGL_BIT = 8,
};

typedef enum gconfig_value_e gconfig_value_e;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_graphic_gconfig
 * Test <gconfig_t> functionality. */
int unittest_graphic_gconfig(void);
#endif


/* struct: gconfig_filter_t
 * Declares filter to select between different possible configuration.
 * The filter function accept must return true if it accepts the visual ID
 * given in parameter visualid else false.
 * If no visualid passes the constructor <initfiltered_gconfig> returns ESRCH. */
struct gconfig_filter_t {
   void  * user;
   bool (* accept) (gconfig_t * gconf, struct display_t * display, int32_t visualid, void * user);
};

/* define: gconfig_filter_INIT
 * Static initializer. */
#define gconfig_filter_INIT(accept, user) \
         { user, accept }

#define gconfig_filter_INIT_FREEABLE \
         { 0, 0 }


/* struct: gconfig_t
 * Contains a specific surface configuration. */
struct gconfig_t {
   struct opengl_config_t * glconfig;
};

// group: lifetime

/* define: gconfig_INIT
 * Static initializer. */
#define gconfig_INIT(glconfig) \
         { glconfig }

/* define: gconfig_INIT_FREEABLE
 * Static initializer. */
#define gconfig_INIT_FREEABLE \
         { 0 }

/* function: init_gconfig
 * Returns an OpenGL surface configuration for display.
 * The parameters stored in config_attributes must be tupels of type (<gconfig_e> value, int value)
 * followed by the end of list value <gconfig_NONE>.
 * The display parameter must be valid as long as gconf is valid.
 * Uses <initfiltered_gconfig> to implement its functionality.
 * The internal filter is created from a call to <configfilter_x11window> (in case of X11 as selected UI). */
int init_gconfig(/*out*/gconfig_t * gconf, struct display_t * display, const int32_t config_attributes[]);

/* function: initfiltered_gconfig
 * Same as <init_gconfig> except that more than one possible configuration is considered.
 * The provided filter is repeatedly called for every possible configuration as long as it returns false.
 * The first configuration which passes the filter is used.
 * Parameter user is passed as user data into the filter function. */
int initfiltered_gconfig(/*out*/gconfig_t * gconf, struct display_t * display, const int32_t config_attributes[], gconfig_filter_t * filter);

/* function: initfromconfigid_gconfig
 * Returns an OpenGL surface configuration whose ID matches configid.
 * Use this function to create a copy of an existing configuration. */
int initfromconfigid_gconfig(/*out*/gconfig_t * gconf, struct display_t * display, const uint32_t configid);

/* function: free_gconfig
 * Frees any memory associated with a configuration. */
int free_gconfig(gconfig_t * gconf);

// group: query

/* function: gl_gconfig
 * Returns the native OpenGL surface config. */
struct opengl_config_t * gl_gconfig(const gconfig_t * gconf);

/* function: value_gconfig
 * Returns the value of attribute stored in gconf.
 * Set attribute to a value from <gconfig_e> and display must be set to the same display
 * given in the init function. */
int value_gconfig(const gconfig_t * gconf, struct display_t * display, int32_t attribute, /*out*/int32_t * value);

/* function: configid_gconfig
 * Returns the configuration ID of gconf.
 * Use this id to create a native window with the correct
 * surface graphic attributes. */
int configid_gconfig(const gconfig_t * gconf, struct display_t * display, /*out*/uint32_t * configid);

/* function: visualid_gconfig
 * Returns the native visualid of the configuration.
 * Use this id to create a native window with the correct
 * surface graphic attributes. */
int visualid_gconfig(const gconfig_t * gconf, struct display_t * display, /*out*/int32_t * visualid);

/* function: maxpbuffer_gconfig
 * Returns the maximum size of an off-screen pixel buffer.
 * The values are returned in maxwidth(in pixels), maxheight(in pixels) and maxpixels(width*height).
 * The parameter maxpixels may be less than maxwidth multiplied by maxheight.
 * If one of the out parameter is set to NULL no value is returned. */
int maxpbuffer_gconfig(const gconfig_t * gconf, struct display_t * display, /*out*/uint32_t * maxwidth, /*out*/uint32_t * maxheight, /*out*/uint32_t * maxpixels);


// section: inline implementation

// group: gconfig_t

/* define: free_gconfig
 * Implements <gconfig_t.free_gconfig>. */
#define free_gconfig(gconf) \
         (*(gconf) = (gconfig_t) gconfig_INIT_FREEABLE, 0)

#if defined(KONFIG_USERINTERFACE_EGL)

// EGL specific implementation (OpenGL ES...)

/* define: configid_gconfig
 * Implements <gconfig_t.configid_gconfig>. */
#define configid_gconfig(gconf, display, configid) \
         configid_eglconfig(gl_gconfig(gconf), gl_display(display), configid)

/* define: gl_gconfig
 * Implements <gconfig_t.gl_gconfig>. */
#define gl_gconfig(gconf) \
         ((gconf)->glconfig)

/* define: maxpbuffer_gconfig
 * Implements <gconfig_t.maxpbuffer_gconfig>. */
#define maxpbuffer_gconfig(gconf, display, maxwidth, maxheight, maxpixels) \
         maxpbuffer_eglconfig(gl_gconfig(gconf), gl_display(display), maxwidth, maxheight, maxpixels)

/* define: value_gconfig
 * Implements <gconfig_t.value_gconfig>. */
#define value_gconfig(gconf, display, attribute, value) \
         value_eglconfig(gl_gconfig(gconf), gl_display(display), attribute, value)

/* define: visualid_gconfig
 * Implements <gconfig_t.visualid_gconfig>. */
#define visualid_gconfig(gconf, display, visualid) \
         visualconfigid_eglconfig(gl_gconfig(gconf), gl_display(display), visualid)

#else
   #error "Not implemented"
#endif

#endif
