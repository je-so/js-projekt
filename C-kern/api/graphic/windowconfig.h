/* title: Graphic-Window-Configuration

   Defines a set of configuration attributes for windows.

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

   file: C-kern/api/graphic/windowconfig.h
    Header file <Graphic-Window-Configuration>.

   file: C-kern/graphic/windowconfig.c
    Implementation file <Graphic-Window-Configuration impl>.
*/
#ifndef CKERN_GRAPHIC_WINDOWCONFIG_HEADER
#define CKERN_GRAPHIC_WINDOWCONFIG_HEADER

/* typedef: struct windowconfig_t
 * Export <windowconfig_t> into global namespace. */
typedef struct windowconfig_t windowconfig_t;

/* enum: windowconfig_e
 * windowconfig_NONE   - Marks end of attribute list.
 * windowconfig_FRAME  - Sets window frame border flag to true. Default value is false.
 *                       No additional attribute value is encoded.
 * windowconfig_MAXSIZE - Sets the maximum window size. Default value is unlimited.
 *                       Two additional attribute values are encoded.
 * windowconfig_MINSIZE - Sets the minimum window size. Default value is the same as the size attribute.
 *                       Two additional attribute values are encoded.
 * windowconfig_POS    - Sets the window position. Default value is either (0,0) or
 *                       chosen by the window manager. Two additional attribute values are encoded.
 * windowconfig_SIZE   - Sets the window size. Default value is (100,100).
 *                       Two additional attribute values are encoded.
 * windowconfig_TITLE  - Sets the title string of the window utf8. Default value is "".
 *                       One additional attribute is encoded. The string is expected to be encoded
 *                       as UTF8.
 * windowconfig_TRANSPARENCY - Sets a transparency value for the whole window.
 *                             Value 255: Window is fully opaque. Value 0: Window is fully transparent.
 *                             DEfault value is 255.
 * windowconfig_NROFELEMENTS - Gives the number of all valid configuration options excluding <windowconfig_NROFELEMENTS>.
 *
 * */
enum windowconfig_e {
      windowconfig_NONE,
      windowconfig_FRAME,
      windowconfig_MAXSIZE,
      windowconfig_MINSIZE,
      windowconfig_POS,
      windowconfig_SIZE,
      windowconfig_TITLE,
      windowconfig_TRANSPARENCY,
      windowconfig_NROFELEMENTS,
};

typedef enum windowconfig_e windowconfig_e;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_graphic_windowconfig
 * Test <windowconfig_t> functionality. */
int unittest_graphic_windowconfig(void);
#endif


/* struct: windowconfig_t
 * Type of a single window configuration attribute. */
struct windowconfig_t {
   int32_t        i32;
   uint8_t        u8;
   uint16_t       u16;
   const char *   str;
};

// group: lifetime

/* define: windowconfig_INIT_NONE
 * Static initializer to mark end of configuration list. */
#define windowconfig_INIT_NONE \
         { .i32 = windowconfig_NONE }

/* define: windowconfig_INIT_FRAME
 * Static initializer to switch on window frame drawn by the window manager. */
#define windowconfig_INIT_FRAME \
         { .i32 = windowconfig_FRAME }

/* define: windowconfig_INIT_MAXSIZE
 * Static initializer to set maximum possible size (width, height) of window. */
#define windowconfig_INIT_MAXSIZE(maxwidth, maxheight) \
         { .i32 = windowconfig_MAXSIZE }, { .u16 = maxwidth }, { .u16 = maxheight }

/* define: windowconfig_INIT_MINSIZE
 * Static initializer to set minimum possible size (width, height) of window. */
#define windowconfig_INIT_MINSIZE(minwidth, minheight) \
         { .i32 = windowconfig_MINSIZE }, { .u16 = minwidth }, { .u16 = minheight }

/* define: windowconfig_INIT_POS
 * Static initializer to set initial position (x,y) of window. */
#define windowconfig_INIT_POS(x, y) \
         { .i32 = windowconfig_POS }, { .i32 = x }, { .i32 = y }

/* define: windowconfig_INIT_SIZE
 * Static initializer to set initial size (width, height) of window. */
#define windowconfig_INIT_SIZE(width, height) \
         { .i32 = windowconfig_SIZE }, { .u16 = width }, { .u16 = height }

/* define: windowconfig_INIT_TITLE
 * Static initializer to set initial size of window. */
#define windowconfig_INIT_TITLE(title) \
         { .i32 = windowconfig_TITLE }, { .str = title }

/* define: windowconfig_INIT_TRANSPARENCY
 * Static initializer to set transparency of window. */
#define windowconfig_INIT_TRANSPARENCY(alpha) \
         { .i32 = windowconfig_TRANSPARENCY }, { .u8 = alpha/*only 0..255*/ }

// group: query

/* function: readtype_windowconfig
 * Reads the attribute type <windowconfig_e> at attrindex in array winconf.
 * The attrindex is incremented by one. */
windowconfig_e readtype_windowconfig(const windowconfig_t * winconf, uint_fast8_t * attrindex);

/* function: readmaxsize_windowconfig
 * Reads a maxsize attribute value at attrindex in array winconf.
 * The result is returned in width and height.
 * The attrindex is incremented by two. */
void readmaxsize_windowconfig(const windowconfig_t * winconf, uint_fast8_t * attrindex, /*out*/uint32_t * width, /*out*/uint32_t * height);

/* function: readminsize_windowconfig
 * Reads a minsize attribute value at attrindex in array winconf.
 * The result is returned in width and height.
 * The attrindex is incremented by two. */
void readminsize_windowconfig(const windowconfig_t * winconf, uint_fast8_t * attrindex, /*out*/uint32_t * width, /*out*/uint32_t * height);

/* function: readpos_windowconfig
 * Reads a position attribute value at attrindex in array winconf.
 * The result is returned in x and y.
 * The attrindex is incremented by two. */
void readpos_windowconfig(const windowconfig_t * winconf, uint_fast8_t * attrindex, /*out*/int32_t * x, /*out*/int32_t * y);

/* function: readsize_windowconfig
 * Reads a size attribute value at attrindex in array winconf.
 * The result is returned in width and height.
 * The attrindex is incremented by two. */
void readsize_windowconfig(const windowconfig_t * winconf, uint_fast8_t * attrindex, /*out*/uint32_t * width, /*out*/uint32_t * height);

/* function: readtitle_windowconfig
 * Reads a title attribute value at attrindex in array winconf.
 * The attrindex is incremented by one. */
const char * readtitle_windowconfig(const windowconfig_t * winconf, uint_fast8_t * attrindex);

/* function: readtransparency_windowconfig
 * Reads an alpha value at attrindex in array winconf.
 * The attrindex is incremented by one. */
uint8_t readtransparency_windowconfig(const windowconfig_t * winconf, uint_fast8_t * attrindex);


// section: inline implementation

/* define: readtype_windowconfig
 * Implements <windowconfig_t.readtype_windowconfig>. */
#define readtype_windowconfig(winconf, attrindex) \
         ((windowconfig_e) winconf[(*(attrindex))++].i32)

/* define: readmaxsize_windowconfig
 * Implements <windowconfig_t.readmaxsize_windowconfig>. */
#define readmaxsize_windowconfig(winconf, attrindex, width, height) \
         readsize_windowconfig(winconf, attrindex, width, height)

/* define: readminsize_windowconfig
 * Implements <windowconfig_t.readminsize_windowconfig>. */
#define readminsize_windowconfig(winconf, attrindex, width, height) \
         readsize_windowconfig(winconf, attrindex, width, height)

/* define: readpos_windowconfig
 * Implements <windowconfig_t.readpos_windowconfig>. */
#define readpos_windowconfig(winconf, attrindex, x, y) \
         {                                            \
            *(x) = winconf[(*(attrindex))++].i32;     \
            *(y) = winconf[(*(attrindex))++].i32;     \
         }

/* define: readsize_windowconfig
 * Implements <windowconfig_t.readsize_windowconfig>. */
#define readsize_windowconfig(winconf, attrindex, width, height) \
         {                                                     \
            *(width)  = winconf[(*(attrindex))++].u16;         \
            *(height) = winconf[(*(attrindex))++].u16;         \
         }

/* define: readtitle_windowconfig
 * Implements <windowconfig_t.readtitle_windowconfig>. */
#define readtitle_windowconfig(winconf, attrindex) \
         (winconf[(*(attrindex))++].str)

/* define: readtransparency_windowconfig
 * Implements <windowconfig_t.readtransparency_windowconfig>. */
#define readtransparency_windowconfig(winconf, attrindex) \
         (winconf[(*(attrindex))++].u8)


#endif
