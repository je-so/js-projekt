/* title: X11-Videomode

   Allows to query and change the video mode of a screen of <x11screen_t>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2013 Jörg Seebohn

   file: C-kern/api/platform/X11/x11videomode.h
    Header file <X11-Videomode>.

   file: C-kern/platform/X11/x11videomode.c
    Implementation file <X11-Videomode impl>.
*/
#ifndef CKERN_PLATFORM_X11_X11VIDEOMODE_HEADER
#define CKERN_PLATFORM_X11_X11VIDEOMODE_HEADER

// forward
struct x11screen_t ;

/* typedef: struct x11videomode_t
 * Export <x11videomode_t> into global namespace. */
typedef struct x11videomode_t             x11videomode_t ;

/* typedef: struct x11videomode_iterator_t
 * Export <x11videomode_iterator_t> into global namespace. */
typedef struct x11videomode_iterator_t    x11videomode_iterator_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_platform_X11_x11videomode
 * Test <x11videomode_t> functionality. */
int unittest_platform_X11_x11videomode(void) ;
#endif


/* struct: x11videomode_t
 * Describes a single supported videmode. */
struct x11videomode_t {
   /* variable: width_in_pixel
    * Pixel size in horizontal direction. */
   uint32_t    width_in_pixel ;
   /* variable: height_in_pixel
    * Pixel size in vertical direction. */
   uint32_t    height_in_pixel ;
   /* variable: modeid
    * Internal implementation specific id. */
   uint16_t    modeid ;
} ;

// group: lifetime

/* define: x11videomode_FREE
 * Static initializer. */
#define x11videomode_FREE { 0, 0, 0 }

/* function: init_x11videomode
 * Initializes xvidmode with the parameter values. */
void init_x11videomode(/*out*/x11videomode_t * xvidmode, uint32_t width_in_pixel, uint32_t height_in_pixel, uint16_t modeid) ;

/* function: initcurrent_x11videomode
 * Returns the current active video mode of x11screen. */
int initcurrent_x11videomode(/*out*/x11videomode_t * current_xvidmode, struct x11screen_t * x11screen) ;

// group: change x11screen_t

/* function: set_x11videomode
 * Sets a new videomode of <x11screen_t>. */
int set_x11videomode(const x11videomode_t * xvidmode, struct x11screen_t * x11screen) ;


/* struct: x11videomode_iterator_t
 * Allows to query all supported videmodes of a <x11screen_t>. */
struct x11videomode_iterator_t {
   uint16_t nextindex ;
   uint16_t nrmodes ;
   void     * config ;
} ;

// group: lifetime

/* define: x11videomode_iterator_FREE
 * Static initializer. */
#define x11videomode_iterator_FREE { 0, 0, 0 }

/* function: init_x11videomodeiterator
 * Returns an iterator to a list of all video modes supported by <x11screen_t>. */
int init_x11videomodeiterator(/*out*/x11videomode_iterator_t * xvidit, struct x11screen_t * x11screen) ;

/* function: free_x11videomodeiterator
 * Frees the iterator and the associated list of all videomodes. */
int free_x11videomodeiterator(x11videomode_iterator_t * xvidit) ;

// group: iterator

/* function: gofirst_x11videomodeiterator
 * Resets iterator to first element. */
void gofirst_x11videomodeiterator(x11videomode_iterator_t * xvidit) ;

/* function: next_x11videomodeiterator
 * Returns next videomode element.
 * The first call after <gofirst_x11videomodeiterator> returns the first element. */
bool next_x11videomodeiterator(x11videomode_iterator_t * xvidit, /*out*/x11videomode_t * xvidmode) ;


// section: inline implementation

// group: x11videomode_t

/* define: init_x11videomode
 * Implements <x11videomode_t.init_x11videomode>. */
#define init_x11videomode(xvidmode, width_in_pixel, height_in_pixel, modeid) \
         ((void)(*(xvidmode) = (x11videomode_t) { width_in_pixel, height_in_pixel, modeid }))

#endif
