/* title: X11-Screen

   Describes a single monitor (or group of monitors depending on configuration).

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

   file: C-kern/api/platform/X11/x11screen.h
    Header file <X11-Screen>.

   file: C-kern/platform/X11/x11screen.c
    Implementation file <X11-Screen impl>.
*/
#ifndef CKERN_PLATFORM_X11_X11SCREEN_HEADER
#define CKERN_PLATFORM_X11_X11SCREEN_HEADER

/* typedef: struct x11screen_t
 * Export <x11screen_t> into global namespace. */
typedef struct x11screen_t x11screen_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_platform_X11_x11screen
 * Test <x11screen_t> functionality. */
int unittest_platform_X11_x11screen(void);
#endif


/* struct: x11screen_t
 * Associates a <x11display_t> with a screen number.
 * A display normally corresponds to a graphics card and the screen to an attached monitor. */
struct x11screen_t {
   struct x11display_t* display;
   int32_t              nrscreen;
};

// group: lifetime

/* define: x11screen_INIT_FREEABLE
 * Static initializer. */
#define x11screen_INIT_FREEABLE              \
         { 0, 0 }

/* define: x11screen_INIT
 * Static initializer. */
#define x11screen_INIT(display, nrscreen)    \
         { display, nrscreen }

/* function: init_x11screen
 * Initializes x11screen_t with reference to <x11display_t> and screen number.
 * If nrscreen >= <nrofscreens_x11display>(display) then error EINVAL is returned.
 * Do not free the display as long as <x11screen_t> is not freed. */
int init_x11screen(/*out*/x11screen_t * x11screen, struct x11display_t * display, int32_t nrscreen);

// group: query

/* function: display_x11screen
 * Returns the display of the screen of. */
struct x11display_t * display_x11screen(const x11screen_t * x11screen);

/* function: number_x11screen
 * Returns the number of the screen of <x11display_t>. */
int32_t number_x11screen(const x11screen_t * x11screen);

/* function: isequal_x11screen
 * Returns true if the two objects refer to the same screen. */
bool isequal_x11screen(const x11screen_t * lx11screen, const x11screen_t * rx11screen);


// section: inline implementation

// group: x11screen_t

/* define: display_x11screen
 * Implements <x11screen_t.display_x11screen>. */
#define display_x11screen(x11screen)    \
         ((x11screen)->display)

/* define: number_x11screen
 * Implements <x11screen_t.number_x11screen>. */
#define number_x11screen(x11screen)    \
         ((x11screen)->nrscreen)

#endif
