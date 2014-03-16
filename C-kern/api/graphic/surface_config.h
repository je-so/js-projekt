/* title: Graphic-Surface-Configuration

   A configuration which describes a graphic surface.
   The configuration is used during construction of a surface.
   A surface could support additional configuration attribues
   which are specific to the surface.

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
#ifndef CKERN_GRAPHIC_SURFACE_CONFIG_HEADER
#define CKERN_GRAPHIC_SURFACE_CONFIG_HEADER

/* typedef: struct surface_config_t
 * Export <surface_config_t> into global namespace. */
typedef struct surface_config_t  surface_config_t;

/* typedef: struct surface_config_it
 * Export <surface_config_it> into global namespace. */
typedef struct surface_config_it surface_config_it;

/* enums: surface_config_e
 * surface_config_NONE        - This is the last entry in a list to indicate termination.
 * surface_config_TYPE        - The configuration supports drawing into a specific surface type.
 *                              Supported values are any combination of bits ored together from
 *                              <surface_configvalue_TYPE_WINDOW_BIT>, ... .
 * surface_config_TRANSPARENT_ALPHA - A value != 0 sets a flag which makes the background of a window surface shine through.
 *                              The alpha value of a pixel determines its opacity.
 *                              Use this to overlay a window surface to another window surface.
 *                              This attribute must be supplied during creation of a window to enable alpha transparency.
 * surface_config_BITS_RED    - The minimum number of bits per pixel which determine the red color (True Color).
 *                              The number of red bits the color buffer supports, e.g. 8 on current hardware.
 * surface_config_BITS_GREEN  - The minimum number of bits per pixel which determine the green color (True Color).
 *                              The number of green bits the color buffer supports, e.g. 8 on current hardware.
 * surface_config_BITS_BLUE   - The minimum number of bits per pixel which determine the blue color (True Color).
 *                              The number of blue bits the color buffer supports, e.g. 8 on current hardware.
 * surface_config_BITS_ALPHA  - The minimum number of bits per pixel which determine the alpha value (1: fully opaque, 0: fully transparent).
 *                              The number of alpha bits the color buffer supports, e.g. 8 on current hardware.
 * surface_config_BITS_DEPTH  - The minimum number of bits the depth buffer must support. If set to 0 no depth buffer is supported.
 * surface_config_BITS_STENCIL - The minimum number of bits the stencil buffer must support. If set to 0 no stencil buffer is supported.
 * surface_config_CONFORMANT  - Determine the supported drawing APIs.
 *                              Supported values are any combination of bits ored together from
 *                              <surface_configvalue_CONFORMANT_ES1_BIT>, <surface_configvalue_CONFORMANT_ES2_BIT>,
 *                              <surface_configvalue_CONFORMANT_OPENGL_BIT>, <surface_configvalue_CONFORMANT_OPENVG_BIT>.
 * surface_config_NROFCONFIGS - Its value is the number of all valid configuration options excluding <surface_config_NROFCONFIGS>.
 *
 * */
enum surface_config_e {
   surface_config_NONE,
   surface_config_TYPE,
   surface_config_TRANSPARENT_ALPHA,
   surface_config_BITS_RED,
   surface_config_BITS_GREEN,
   surface_config_BITS_BLUE,
   surface_config_BITS_ALPHA,
   surface_config_BITS_DEPTH,
   surface_config_BITS_STENCIL,
   surface_config_CONFORMANT,
   surface_config_NROFCONFIGS
};

typedef enum surface_config_e surface_config_e;

/* enums: surface_configvalue_e
 * surface_configvalue_TYPE_PBUFFER_BIT  - The config supports drawing into a OpenGL (ES) pixel buffer surface.
 *                                         A pbuffer surface always supports single buffering.
 * surface_configvalue_TYPE_PIXMAP_BIT   - The config supports drawing into a native pixmap surface.
 *                                         A pixmap surface always supports single buffering.
 * surface_configvalue_TYPE_WINDOW_BIT   - The config supports drawing into a window surface.
 *                                         A window surface always supports double buffering.
 * surface_configvalue_CONFORMANT_ES1_BIT - Config supports creating OpenGL ES 1.0/1.1 graphic contexts.
 * surface_configvalue_CONFORMANT_OPENVG_BIT - Config supports creating OpenVG graphic contexts.
 * surface_configvalue_CONFORMANT_ES2_BIT - Config supports creating OpenGL ES 2.0 graphic contexts.
 * surface_configvalue_CONFORMANT_OPENGL_BIT - Config supports creating OpenGL graphic contexts.
 * */
enum surface_configvalue_e {
   surface_configvalue_TYPE_PBUFFER_BIT = 1,
   surface_configvalue_TYPE_PIXMAP_BIT  = 2,
   surface_configvalue_TYPE_WINDOW_BIT  = 4,
   surface_configvalue_CONFORMANT_ES1_BIT    = 1,
   surface_configvalue_CONFORMANT_OPENVG_BIT = 2,
   surface_configvalue_CONFORMANT_ES2_BIT    = 4,
   surface_configvalue_CONFORMANT_OPENGL_BIT = 8,
};

typedef enum surface_configvalue_e surface_configvalue_e;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_graphic_surface_config
 * Test <surface_config_t> functionality. */
int unittest_graphic_surface_config(void);
#endif


/* struct: surface_config_it
 * Contains a specific surface configuration. */
struct surface_config_it {
   int (*value) (void * config, void * egldisp, int attribute, /*out*/int * value);
};


/* struct: surface_config_t
 * Contains a specific surface configuration. */
struct surface_config_t {
   const surface_config_it * iimpl;
   void * config;
};

// group: lifetime

/* define: surface_config_INIT
 * Static initializer. */
#define surface_config_INIT(iimpl, config) \
         { iimpl, config }

/* define: surface_config_INIT_FREEABLE
 * Static initializer. */
#define surface_config_INIT_FREEABLE \
         { 0, 0 }

/* function: initfromegl_surfaceconfig
 * Matches a specific surface configuration for an EGL display.
 * See <chooseconfig_egldisplay>. */
int initfromegl_surfaceconfig(/*out*/surface_config_t * config, void * egldisp, const int config_attributes[]);

/* function: free_surfaceconfig
 * Frees any memory associated with a configuration. */
int free_surfaceconfig(surface_config_t * config);

// group: query

/* function: value_surfaceconfig
 * Returns the value of attribute stored in config.
 * Set attribute to a value from <surface_config_e> and display must be set to the same display
 * given in the init function. */
int value_surfaceconfig(surface_config_t * config, void * display, int attribute, /*out*/int * value);


// section: inline implementation

// group: surface_config_t

/* define: free_surfaceconfig
 * Implements <surface_config_t.free_surfaceconfig>. */
#define free_surfaceconfig(config) \
         (*(config) = (surface_config_t) surface_config_INIT_FREEABLE, 0)

/* define: value_surfaceconfig
 * Implements <surface_config_t.value_surfaceconfig>. */
#define value_surfaceconfig(_config, display, attribute, _value) \
         ( __extension__ ({                                       \
            typeof(_config) _c = (_config);                       \
            _c->iimpl->value( _c->config,                         \
                              display, attribute, _value);        \
         }))

#endif
