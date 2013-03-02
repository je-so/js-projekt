/* title: X11-Attribute

   Describes a type which is used for configuration of <x11window_t>, <glxwindow_t>
   and possible other window subtypes.

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
   (C) 2013 Jörg Seebohn

   file: C-kern/api/platform/X11/x11attribute.h
    Header file <X11-Attribute>.

   file: C-kern/platform/shared/X11/x11attribute.c
    Implementation file <X11-Attribute impl>.
*/
#ifndef CKERN_PLATFORM_X11_X11ATTRIBUTE_HEADER
#define CKERN_PLATFORM_X11_X11ATTRIBUTE_HEADER

/* typedef: struct x11attribute_t
 * Export <x11attribute_t> into global namespace. */
typedef struct x11attribute_t                x11attribute_t ;

/* typedef: union x11attribute_value_t
 * Export <x11attribute_value_t> into global namespace. */
typedef union x11attribute_value_t           x11attribute_value_t ;

/* enums: x11attribute_name_e
 * Used as values for <x11attribute_t.name>.
 * x11attribute_VOID            - Attribute is an undefined value.
 * x11attribute_WINTITLE        - Attribute describing the window title as string.
 * x11attribute_WINFRAME        - Attribute sets a flag which turns the window manager frame of a top level window on.
 * x11attribute_WINPOS          - Sets the xy position of a window else the window manager chooses one.
 * x11attribute_WINSIZE         - Sets the window size (width and height).
 * x11attribute_WINMINSIZE      - Sets the window minimum size (width and height).
 *                                If the window has a frame the window manager allows to resize the window.
 *                                But it can never be resized below this size.
 * x11attribute_WINMAXSIZE      - Sets the window maximum size (width and height).
 *                                If the window has a frame the window manager allows to resize the window.
 *                                But it can never be resized beyond this size.
 * x11attribute_WINOPACITY      - Attribute sets an uint32_t value which determines the opacity of the whole window
 *                                including the fram drawn by the window manager.
 *                                See also <x11attribute_INIT_WINOPACITY>.
 * x11attribute_ALPHAOPACITY    - Attribute sets a flag which makes lets the background of the window shine through.
 *                                The alpha value of a pixel determines its opacity.
 *                                See also <x11attribute_INIT_ALPHAOPACITY>.
 *                                In 2d mode (X11) this is the same as setting <x11attribute_ALPHABITS> to a value != 0.
 * x11attribute_DOUBLEBUFFER    - Allocates an X11(2d) or OpenGL(3d) double buffer.
 * x11attribute_REDBITS         - The minimum number of bits per pixel which determine the red color (True Color).
 *                                The number of red bits the color buffer supports, e.g. 8 on current hardware.
 * x11attribute_GREENBITS       - The minimum number of bits per pixel which determine the green color (True Color).
 *                                The number of green bits the color buffer supports, e.g. 8 on current hardware.
 * x11attribute_BLUEBITS        - The minimum number of bits per pixel which determine the blue color (True Color).
 *                                The number of blue bits the color buffer supports, e.g. 8 on current hardware.
 * x11attribute_ALPHABITS       - The minimum number of bits per pixel which determine the alpha value (1 opaque, 0 transparent).
 *                                The number of alpha bits the color buffer supports, e.g. 8 on current hardware.
 *                                In 2d mode (X11) this is the same as setting <x11attribute_ALPHAOPACITY>.
 * x11attribute_DEPTHBITS       - The number of bits the depth buffer supports. If set to 0 no depth buffer is supported.
 * x11attribute_STENCILBITS     - The number of bits the stencil buffer supports. If set to 0 no stencil buffer is supported.
 * x11attribute_ACCUM_REDBITS   - The number of red bits the accumulation buffer supports, e.g. 16 on current hardware.
 * x11attribute_ACCUM_GREENBITS - The number of green bits the accumulation buffer supports, e.g. 16 on current hardware.
 * x11attribute_ACCUM_BLUEBITS  - The number of blue bits the accumulation buffer supports, e.g. 16 on current hardware.
 * x11attribute_ACCUM_ALPHABITS - The number of alpha bits the accumulation buffer supports, e.g. 16 on current hardware.
 *
 * It is possible that a subtype extends the value range.
 * */
enum x11attribute_name_e {
   x11attribute_VOID,
   // window attributes
   x11attribute_WINTITLE,
   x11attribute_WINFRAME,
   x11attribute_WINPOS,
   x11attribute_WINSIZE,
   x11attribute_WINMINSIZE,
   x11attribute_WINMAXSIZE,
   x11attribute_WINOPACITY,
   // generic graphics attributes supported by <x11window_t> and <glxwindow_t>
   x11attribute_ALPHAOPACITY = 32,
   x11attribute_DOUBLEBUFFER,
   x11attribute_REDBITS,
   x11attribute_GREENBITS,
   x11attribute_BLUEBITS,
   x11attribute_ALPHABITS,
   // additional OpenGL attributes for <glxwindow_t>
   x11attribute_DEPTHBITS = 64,
   x11attribute_STENCILBITS,
   x11attribute_ACCUM_REDBITS,
   x11attribute_ACCUM_GREENBITS,
   x11attribute_ACCUM_BLUEBITS,
   x11attribute_ACCUM_ALPHABITS,
   // add other ranges here
} ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_platform_X11_x11attribute
 * Test <x11attribute_t> functionality. */
int unittest_platform_X11_x11attribute(void) ;
#endif


/* union: x11attribute_value_t
 * Contains a single value or a value pair. */
union x11attribute_value_t {
   struct {
      int32_t     width ;
      int32_t     height ;
   } ;
   struct {
      int32_t     x ;
      int32_t     y ;
   } ;
   int32_t        i32 ;
   uint32_t       u32 ;
   const char *   str ;
   bool           isOn ;
} ;

/* define: x11attribute_value_INIT_FREEABLE
 * Static initializer. */
#define x11attribute_value_INIT_FREEABLE \
         { { 0, 0 } }


/* struct: x11attribute_t
 * Stores a name value pair.
 * The type of value is determined by <name> which is set to one of the values out of <x11attribute_name_e>. */
struct x11attribute_t {
   /* variable: name
    * The name of the attribute. Set it to a value from <x11attribute_name_e>. */
   uint32_t             name ;
   /* variable: value
    * The value of the attribute. The value is determined by the attribute name <x11attribute_name_e>. */
   x11attribute_value_t value ;
} ;

// group: lifetime

/* define: x11attribute_INIT_FREEABLE
 * Static initializer. */
#define x11attribute_INIT_FREEABLE \
         { x11attribute_VOID, x11attribute_value_INIT_FREEABLE }

/* define: x11attribute_INIT_WINTITLE
 * Sets the window title. */
#define x11attribute_INIT_WINTITLE(window_title_str)  \
         { x11attribute_WINTITLE, { .str = window_title_str } }

/* define: x11attribute_INIT_WINFRAME
 * Requests window manager to draw a frame around the window.
 * The window allows the window to be resized, minimized maximized and closed by the user. */
#define x11attribute_INIT_WINFRAME \
         { x11attribute_WINFRAME, { .isOn = true } }

/* define: x11attribute_INIT_WINPOS
 * Determines the x and y position of a window. If you do not set this attribute the window manager chooses an appropriate value. */
#define x11attribute_INIT_WINPOS(xpos_i32, ypos_i32)      \
         { x11attribute_WINPOS, { .y = ypos_i32, .x = xpos_i32 } }

/* define: x11attribute_INIT_WINSIZE
 * Determines the width and height of a window. */
#define x11attribute_INIT_WINSIZE(width_u32, height_u32)    \
         { x11attribute_WINSIZE, { .height = height_u32, .width = width_u32 } }

/* define: x11attribute_INIT_WINMINSIZE
 * Determines the minimum width and height of a window. The window manager does not allow the user to resize the window to a smaller size. */
#define x11attribute_INIT_WINMINSIZE(width_u32, height_u32) \
         { x11attribute_WINMINSIZE, { .height = height_u32, .width = width_u32 } }

/* define: x11attribute_INIT_WINMAXSIZE
 * Determines the maximum width and height of a window. The window manager does not allow the user to resize the window to a bigger size. */
#define x11attribute_INIT_WINMAXSIZE(width_u32, height_u32) \
         { x11attribute_WINMAXSIZE, { .height = height_u32, .width = width_u32 } }

/* define: x11attribute_INIT_WINOPACITY
 * Sets the overall windows opacity. The blending function is the same as in <x11attribute_INIT_ALPHAOPACITY>
 * but no alpha channel is needed. All pixel values including the window manager frame get their alpha value from
 * the value set in this attribute. Use a value of UINT32_MAX for fully opaque and a value of 0 for fully transparent. */
#define x11attribute_INIT_WINOPACITY(opacity_u32) \
         { x11attribute_WINOPACITY, { .u32 = opacity_u32 } }

/* define: x11attribute_INIT_ALPHAOPACITY
 * Makes content of window transparent determined.
 * An X11 RGBA visual (X Render Extension) is chosen which interprets the alpha
 * value of a pixel as blending factor with the underlying background.
 * An alpha value of 1 means the pixel is fully opaque. An alpha value of 0
 * means the pixel is fully transparent so the background of a window is visible with 100%.
 * If the alpha channel has 8 bits then the value 255 corresponds to 1 and the value 128 to 0.5.
 *
 * Blending function:
 * The blending function assumes that all pixels color values in the window are premultiplied by alpha.
 * > Screen-Pixel-RGB = Window-Pixel-RGB + (1 - Window-Pixel-Alpha) * Background-Pixel-RGB
 * Before drawing change the color of your draw content to the value
 * > Window-Pixel-RGB = Window-Pixel-RGB * Window-Pixel-Alpha */
#define x11attribute_INIT_ALPHAOPACITY  \
         { x11attribute_ALPHAOPACITY, { .isOn = true } }

/* define: x11attribute_INIT_DOUBLEBUFFER
 * Allocates an additional back buffer. This allows to draw to the back buffer and then to swap the content.
 * The user gets the illusion that all updates to a window occur at the same time. */
#define x11attribute_INIT_DOUBLEBUFFER \
         { x11attribute_DOUBLEBUFFER, { .isOn = true } }

/* define: x11attribute_INIT_REDBITS
 * The number of bits of a pixel for setting its red color. 8 bits allow to set 256 shades of red (0 is black). */
#define x11attribute_INIT_REDBITS(red_bits)     \
         { x11attribute_REDBITS, { .u32 = red_bits } }

/* define: x11attribute_INIT_GREENBITS
 * The number of bits of a pixel for setting its green color. 8 bits allow to set 256 shades of green (0 is black). */
#define x11attribute_INIT_GREENBITS(green_bits) \
         { x11attribute_GREENBITS, { .u32 = green_bits } }

/* define: x11attribute_INIT_BLUEBITS
 * The number of bits of a pixel for setting its blue color. 8 bits allow to set 256 shades of blue (0 is black). */
#define x11attribute_INIT_BLUEBITS(blue_bits)   \
         { x11attribute_BLUEBITS, { .u32 = blue_bits } }

/* define: x11attribute_INIT_ALPHABITS
 * The number of bits of a pixel for setting its alpha value. 8 bits allow to set 256 shades of opacity (0 is fully transparent). */
#define x11attribute_INIT_ALPHABITS(alpha_bits) \
         { x11attribute_ALPHABITS, { .u32 = alpha_bits } }

/* define: x11attribute_INIT_RGBA
 * Static initializer of 4 <x11attribute_t>.
 * Sets attrribute values <x11attribute_REDBITS>, <x11attribute_GREENBITS>, <x11attribute_BLUEBITS>, and <x11attribute_ALPHABITS>. */
#define x11attribute_INIT_RGBA(redbits,greenbits,bluebits,alphabits) \
         x11attribute_INIT_REDBITS(redbits),                         \
         x11attribute_INIT_GREENBITS(greenbits),                     \
         x11attribute_INIT_BLUEBITS(bluebits),                       \
         x11attribute_INIT_ALPHABITS(alphabits)                      \

/* define: x11attribute_INIT_DEPTHBITS
 * Sets the number of bits the depth buffer supports. If set to 0 no depth buffer is supported. */
#define x11attribute_INIT_DEPTHBITS(depth_bits) \
         { x11attribute_DEPTHBITS, { .u32 = depth_bits } }

/* define: x11attribute_INIT_STENCILBITS
 * Sets the number of bits the stencil buffer should support. If set to 0 no stencil buffer is supported.*/
#define x11attribute_INIT_STENCILBITS(stencil_bits) \
         { x11attribute_STENCILBITS, { .u32 = stencil_bits } }

 /* define: x11attribute_INIT_ACCUM_REDBITS
 * Sets the number of red bits the accumulation buffer should support at least, e.g. 16 on current hardware. */
#define x11attribute_INIT_ACCUM_REDBITS(red_bits) \
         { x11attribute_ACCUM_REDBITS, { .u32 = red_bits } }

/* define: x11attribute_INIT_ACCUM_GREENBITS
 * Sets the number of green bits the accumulation buffer should support at least, e.g. 16 on current hardware. */
#define x11attribute_INIT_ACCUM_GREENBITS(green_bits) \
         { x11attribute_ACCUM_GREENBITS, { .u32 = green_bits } }

/* define: x11attribute_INIT_ACCUM_BLUEBITS
 * Sets the number of blue bits the accumulation buffer should support at least, e.g. 16 on current hardware. */
#define x11attribute_INIT_ACCUM_BLUEBITS(blue_bits) \
         { x11attribute_ACCUM_BLUEBITS, { .u32 = blue_bits } }

/* define: x11attribute_INIT_ACCUM_ALPHABITS
 * Sets the number of alpha bits the accumulation buffer should support at least, e.g. 16 on current hardware. */
#define x11attribute_INIT_ACCUM_ALPHABITS(alpha_bits) \
         { x11attribute_ACCUM_ALPHABITS, { .u32 = alpha_bits } }

// group: query
// Access data fields directly. No wrapper functions are provided for query.


#endif
