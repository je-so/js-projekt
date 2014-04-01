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

   file: C-kern/api/graphic/surfaceconfig.h
    Header file <Graphic-Surface-Configuration>.

   file: C-kern/graphic/surfaceconfig.c
    Implementation file <Graphic-Surface-Configuration impl>.
*/
#ifndef CKERN_GRAPHIC_SURFACE_CONFIG_HEADER
#define CKERN_GRAPHIC_SURFACE_CONFIG_HEADER

// forward
struct opengl_config_t;
struct display_t;

/* typedef: struct surfaceconfig_t
 * Export <surfaceconfig_t> into global namespace. */
typedef struct surfaceconfig_t surfaceconfig_t;

/* typedef: surfaceconfig_filter_t
 * Declares filter to select between different possible configuration.
 * The filter function must return true if it accepts the visual ID
 * given in parameter visualid else false.
 * If no visualid passes the filter <initfiltered_eglconfig> returns ESRCH. */
typedef struct surfaceconfig_filter_t surfaceconfig_filter_t;


/* enums: surfaceconfig_e
 * surfaceconfig_NONE        - This is the last entry in a list to indicate termination.
 * surfaceconfig_TYPE        - The configuration supports drawing into a specific surface type.
 *                             Supported values are any combination of bits ored together from
 *                             <surfaceconfig_value_TYPE_WINDOW_BIT>, ... .
 * surfaceconfig_TRANSPARENT_ALPHA - A value != 0 sets a flag which makes the background of a window surface shine through.
 *                             Default value is 0.
 *                             The alpha value of a pixel determines its opacity.
 *                             Use this to overlay a window surface to another window surface.
 *                             This attribute must be supplied during creation of a window to enable alpha transparency.
 *                             An alpha value of 1 means the pixel is fully opaque.
 *                             An alpha value of 0 means the pixel is fully transparent so the background
 *                             of a window is visible with 100%.
 *                             If the alpha channel has 8 bits then the value 255 corresponds to 1 and the value 128 to 0.5.
 *                             Blending function on X11:
 *                             The blending function assumes that every pixel's color value is premultiplied by alpha.
 *                             Screen-Pixel-RGB = Window-Pixel-RGB + (1 - Window-Pixel-Alpha) * Background-Pixel-RGB
 *                             Before drawing you need to change the color of the pixels to:
 *                             Window-Pixel-RGB = Window-Pixel-RGB * Window-Pixel-Alpha
 * surfaceconfig_BITS_BUFFER - The minimum number of bits per pixel for all color channels together including alpha.
 * surfaceconfig_BITS_RED    - The minimum number of bits per pixel which determine the red color.
 *                             The number of red bits the color buffer supports, e.g. 8 on current hardware.
 * surfaceconfig_BITS_GREEN  - The minimum number of bits per pixel which determine the green color.
 *                             The number of green bits the color buffer supports, e.g. 8 on current hardware.
 * surfaceconfig_BITS_BLUE   - The minimum number of bits per pixel which determine the blue color.
 *                             The number of blue bits the color buffer supports, e.g. 8 on current hardware.
 * surfaceconfig_BITS_ALPHA  - The minimum number of bits per pixel which determine the alpha value (1: fully opaque, 0: fully transparent).
 *                             The number of alpha bits the color buffer supports, e.g. 8 on current hardware.
 * surfaceconfig_BITS_DEPTH  - The minimum number of bits the depth buffer must support. If set to 0 no depth buffer is supported.
 * surfaceconfig_BITS_STENCIL - The minimum number of bits the stencil buffer must support. If set to 0 no stencil buffer is supported.
 * surfaceconfig_CONFORMANT  - Determine the supported drawing APIs.
 *                             Supported values are any combination of bits ored together from
 *                             <surfaceconfig_value_CONFORMANT_ES1_BIT>, <surfaceconfig_value_CONFORMANT_ES2_BIT>,
 *                             <surfaceconfig_value_CONFORMANT_OPENGL_BIT>, <surfaceconfig_value_CONFORMANT_OPENVG_BIT>.
 * surfaceconfig_NROFELEMENTS - Gives the number of all valid configuration options excluding <surfaceconfig_NROFELEMENTS>.
 *
 * */
enum surfaceconfig_e {
   surfaceconfig_NONE,
   surfaceconfig_TYPE,
   surfaceconfig_TRANSPARENT_ALPHA,
   surfaceconfig_BITS_BUFFER,
   surfaceconfig_BITS_RED,
   surfaceconfig_BITS_GREEN,
   surfaceconfig_BITS_BLUE,
   surfaceconfig_BITS_ALPHA,
   surfaceconfig_BITS_DEPTH,
   surfaceconfig_BITS_STENCIL,
   surfaceconfig_CONFORMANT,
   surfaceconfig_NROFELEMENTS
};

typedef enum surfaceconfig_e surfaceconfig_e;

/* enums: surfaceconfig_value_e
 * surfaceconfig_value_TYPE_PBUFFER_BIT  - The config supports drawing into a OpenGL (ES) pixel buffer surface.
 *                                         A pbuffer surface always supports single buffering.
 * surfaceconfig_value_TYPE_PIXMAP_BIT   - The config supports drawing into a native pixmap surface.
 *                                         A pixmap surface always supports single buffering.
 * surfaceconfig_value_TYPE_WINDOW_BIT   - The config supports drawing into a window surface.
 *                                         A window surface always supports double buffering.
 * surfaceconfig_value_CONFORMANT_ES1_BIT - Config supports creating OpenGL ES 1.0/1.1 graphic contexts.
 * surfaceconfig_value_CONFORMANT_OPENVG_BIT - Config supports creating OpenVG graphic contexts.
 * surfaceconfig_value_CONFORMANT_ES2_BIT - Config supports creating OpenGL ES 2.0 graphic contexts.
 * surfaceconfig_value_CONFORMANT_OPENGL_BIT - Config supports creating OpenGL graphic contexts.
 * */
enum surfaceconfig_value_e {
   surfaceconfig_value_TYPE_PBUFFER_BIT = 1,
   surfaceconfig_value_TYPE_PIXMAP_BIT  = 2,
   surfaceconfig_value_TYPE_WINDOW_BIT  = 4,
   surfaceconfig_value_CONFORMANT_ES1_BIT    = 1,
   surfaceconfig_value_CONFORMANT_OPENVG_BIT = 2,
   surfaceconfig_value_CONFORMANT_ES2_BIT    = 4,
   surfaceconfig_value_CONFORMANT_OPENGL_BIT = 8,
};

typedef enum surfaceconfig_value_e surfaceconfig_value_e;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_graphic_surfaceconfig
 * Test <surfaceconfig_t> functionality. */
int unittest_graphic_surfaceconfig(void);
#endif


/* struct: surfaceconfig_filter_t
 * Declares filter to select between different possible configuration.
 * The filter function accept must return true if it accepts the visual ID
 * given in parameter visualid else false.
 * If no visualid passes the constructor <initfiltered_surfaceconfig> returns ESRCH. */
struct surfaceconfig_filter_t {
   void  * user;
   bool (* accept) (surfaceconfig_t * surfconf, struct display_t * display, int32_t visualid, void * user);
};

/* define: surfaceconfig_filter_INIT
 * Static initializer. */
#define surfaceconfig_filter_INIT(accept, user) \
         { user, accept }

#define surfaceconfig_filter_INIT_FREEABLE \
         { 0, 0 }


/* struct: surfaceconfig_t
 * Contains a specific surface configuration. */
struct surfaceconfig_t {
   struct opengl_config_t * config;
};

// group: lifetime

/* define: surfaceconfig_INIT
 * Static initializer. */
#define surfaceconfig_INIT(config) \
         { config }

/* define: surfaceconfig_INIT_FREEABLE
 * Static initializer. */
#define surfaceconfig_INIT_FREEABLE \
         { 0 }

/* function: init_surfaceconfig
 * Matches a specific surface configuration for an EGL display.
 * See also <init_eglconfig> and <configfilter_x11window>.
 * The display parameter must be valid as long as surfconf is valid.
 * Uses <initfiltered_surfaceconfig> to implement its functionality.
 * The provided filter is returned from a call to <configfilter_x11window> (in case of X11 as selected UI). */
int init_surfaceconfig(/*out*/surfaceconfig_t * surfconf, struct display_t * display, const int32_t config_attributes[]);

/* function: initfiltered_surfaceconfig
 * Same as <init_surfaceconfig> except that more than one possible configuration is considered.
 * The provided filter is repeatedly called for every possible configuration as long as it returns false.
 * The first configuration which passes the filter is used.
 * Parameter user is passed as user data into the filter function. */
int initfiltered_surfaceconfig(/*out*/surfaceconfig_t * surfconf, struct display_t * display, const int32_t config_attributes[], surfaceconfig_filter_t * filter);

/* function: free_surfaceconfig
 * Frees any memory associated with a configuration. */
int free_surfaceconfig(surfaceconfig_t * surfconf);

// group: query

/* function: value_surfaceconfig
 * Returns the value of attribute stored in surfconf.
 * Set attribute to a value from <surfaceconfig_e> and display must be set to the same display
 * given in the init function. */
int value_surfaceconfig(const surfaceconfig_t * surfconf, struct display_t * display, int32_t attribute, /*out*/int32_t * value);

/* function: visualid_surfaceconfig
 * Returns the native visualid of the configuration.
 * Use this id to create a native window with the correct
 * surface graphic attributes. */
int visualid_surfaceconfig(const surfaceconfig_t * surfconf, struct display_t * display, /*out*/int32_t * visualid);


// section: inline implementation

// group: surfaceconfig_t

/* define: free_surfaceconfig
 * Implements <surfaceconfig_t.free_surfaceconfig>. */
#define free_surfaceconfig(surfconf) \
         (*(surfconf) = (surfaceconfig_t) surfaceconfig_INIT_FREEABLE, 0)

#if defined(KONFIG_USERINTERFACE_EGL)

// EGL specific implementation (OpenGL ES...)

/* define: value_surfaceconfig
 * Implements <surfaceconfig_t.value_surfaceconfig>. */
#define value_surfaceconfig(surfconf, display, attribute, value) \
         value_eglconfig((surfconf)->config, gl_display(display), attribute, value)

/* define: visualid_surfaceconfig
 * Implements <surfaceconfig_t.visualid_surfaceconfig>. */
#define visualid_surfaceconfig(surfconf, display, visualid) \
         visualid_eglconfig((surfconf)->config, gl_display(display), visualid)

#else
   #error "Not implemented"
#endif

#endif
