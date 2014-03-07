/* title: X11-Attribute impl

   Implements <X11-Attribute>.

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

   file: C-kern/platform/X11/x11attribute.c
    Implementation file <X11-Attribute impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/platform/X11/x11attribute.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#endif


// section: x11attribute_t

// group: lifetime


// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   x11attribute_t x11attr = x11attribute_INIT_FREEABLE ;

   // TEST x11attribute_name_e
   static_assert(x11attribute_VOID == 0, "0 is freed") ;
   static_assert(x11attribute_WINOPACITY < x11attribute_ALPHAOPACITY, "number regions do not overlap") ;
   static_assert(x11attribute_ALPHABITS < x11attribute_DEPTHBITS, "number regions do not overlap") ;

   // TEST x11attribute_INIT_FREEABLE
   TEST(x11attr.name      == 0) ;
   TEST(x11attr.value.i32 == 0) ;

   // TEST x11attribute_INIT_WINTITLE
   const char * const title = "test title" ;
   x11attr = (x11attribute_t) x11attribute_INIT_WINTITLE(title) ;
   TEST(x11attr.name      == x11attribute_WINTITLE) ;
   TEST(x11attr.value.str == title) ;

   // TEST x11attribute_INIT_WINFRAME
   x11attr = (x11attribute_t) x11attribute_INIT_WINFRAME ;
   TEST(x11attr.name       == x11attribute_WINFRAME) ;
   TEST(x11attr.value.isOn == true) ;

   // TEST x11attribute_INIT_WINPOS
   x11attr = (x11attribute_t) x11attribute_INIT_WINPOS(-100, 200) ;
   TEST(x11attr.name        == x11attribute_WINPOS) ;
   TEST(x11attr.value.x == -100) ;
   TEST(x11attr.value.y ==  200) ;

   // TEST x11attribute_INIT_WINSIZE
   x11attr = (x11attribute_t) x11attribute_INIT_WINSIZE(100, 200) ;
   TEST(x11attr.name         == x11attribute_WINSIZE) ;
   TEST(x11attr.value.width  == 100) ;
   TEST(x11attr.value.height == 200) ;

   // TEST x11attribute_INIT_WINMINSIZE
   x11attr = (x11attribute_t) x11attribute_INIT_WINMINSIZE(101, 201) ;
   TEST(x11attr.name         == x11attribute_WINMINSIZE) ;
   TEST(x11attr.value.width  == 101) ;
   TEST(x11attr.value.height == 201) ;

   // TEST x11attribute_INIT_WINMAXSIZE
   x11attr = (x11attribute_t) x11attribute_INIT_WINMAXSIZE(102, 202) ;
   TEST(x11attr.name         == x11attribute_WINMAXSIZE) ;
   TEST(x11attr.value.width  == 102) ;
   TEST(x11attr.value.height == 202) ;

   // TEST x11attribute_INIT_ALPHAOPACITY
   x11attr = (x11attribute_t) x11attribute_INIT_ALPHAOPACITY ;
   TEST(x11attr.name       == x11attribute_ALPHAOPACITY) ;
   TEST(x11attr.value.isOn == true) ;

   // TEST x11attribute_INIT_WINOPACITY
   x11attr = (x11attribute_t) x11attribute_INIT_WINOPACITY(0xffff1234) ;
   TEST(x11attr.name      == x11attribute_WINOPACITY) ;
   TEST(x11attr.value.u32 == 0xffff1234) ;

   // TEST x11attribute_INIT_DOUBLEBUFFER
   x11attr = (x11attribute_t) x11attribute_INIT_DOUBLEBUFFER ;
   TEST(x11attr.name       == x11attribute_DOUBLEBUFFER) ;
   TEST(x11attr.value.isOn == true) ;

   // TEST x11attribute_INIT_REDBITS
   x11attr = (x11attribute_t) x11attribute_INIT_REDBITS(UINT32_MAX) ;
   TEST(x11attr.name      == x11attribute_REDBITS) ;
   TEST(x11attr.value.u32 == UINT32_MAX) ;

   // TEST x11attribute_INIT_GREENBITS
   x11attr = (x11attribute_t) x11attribute_INIT_GREENBITS(UINT32_MAX-1) ;
   TEST(x11attr.name      == x11attribute_GREENBITS) ;
   TEST(x11attr.value.u32 == UINT32_MAX-1) ;

   // TEST x11attribute_INIT_BLUEBITS
   x11attr = (x11attribute_t) x11attribute_INIT_BLUEBITS(UINT32_MAX-2) ;
   TEST(x11attr.name      == x11attribute_BLUEBITS) ;
   TEST(x11attr.value.u32 == UINT32_MAX-2) ;

   // TEST x11attribute_INIT_ALPHABITS
   x11attr = (x11attribute_t) x11attribute_INIT_ALPHABITS(UINT32_MAX-3) ;
   TEST(x11attr.name      == x11attribute_ALPHABITS) ;
   TEST(x11attr.value.u32 == UINT32_MAX-3) ;

   // TEST x11attribute_INIT_RGBA
   x11attribute_t x11attr2[4] = { x11attribute_INIT_RGBA(1,2,3,4) } ;
   TEST(x11attr2[0].name      == x11attribute_REDBITS) ;
   TEST(x11attr2[0].value.u32 == 1) ;
   TEST(x11attr2[1].name      == x11attribute_GREENBITS) ;
   TEST(x11attr2[1].value.u32 == 2) ;
   TEST(x11attr2[2].name      == x11attribute_BLUEBITS) ;
   TEST(x11attr2[2].value.u32 == 3) ;
   TEST(x11attr2[3].name      == x11attribute_ALPHABITS) ;
   TEST(x11attr2[3].value.u32 == 4) ;

   // TEST x11attribute_INIT_DEPTHBITS
   x11attr = (x11attribute_t) x11attribute_INIT_DEPTHBITS(UINT32_MAX-4) ;
   TEST(x11attr.name      == x11attribute_DEPTHBITS) ;
   TEST(x11attr.value.u32 == UINT32_MAX-4) ;

   // TEST x11attribute_INIT_STENCILBITS
   x11attr = (x11attribute_t) x11attribute_INIT_STENCILBITS(UINT32_MAX-5) ;
   TEST(x11attr.name      == x11attribute_STENCILBITS) ;
   TEST(x11attr.value.u32 == UINT32_MAX-5) ;

   // TEST x11attribute_INIT_ACCUM_REDBITS
   x11attr = (x11attribute_t) x11attribute_INIT_ACCUM_REDBITS(UINT32_MAX-6) ;
   TEST(x11attr.name      == x11attribute_ACCUM_REDBITS) ;
   TEST(x11attr.value.u32 == UINT32_MAX-6) ;

   // TEST x11attribute_INIT_ACCUM_GREENBITS
   x11attr = (x11attribute_t) x11attribute_INIT_ACCUM_GREENBITS(UINT32_MAX-7) ;
   TEST(x11attr.name      == x11attribute_ACCUM_GREENBITS) ;
   TEST(x11attr.value.u32 == UINT32_MAX-7) ;

   // TEST x11attribute_INIT_ACCUM_BLUEBITS
   x11attr = (x11attribute_t) x11attribute_INIT_ACCUM_BLUEBITS(UINT32_MAX-7) ;
   TEST(x11attr.name      == x11attribute_ACCUM_BLUEBITS) ;
   TEST(x11attr.value.u32 == UINT32_MAX-7) ;

   // TEST x11attribute_INIT_ACCUM_BLUEBITS
   x11attr = (x11attribute_t) x11attribute_INIT_ACCUM_BLUEBITS(UINT32_MAX-8) ;
   TEST(x11attr.name      == x11attribute_ACCUM_BLUEBITS) ;
   TEST(x11attr.value.u32 == UINT32_MAX-8) ;

   // TEST x11attribute_INIT_ACCUM_ALPHABITS
   x11attr = (x11attribute_t) x11attribute_INIT_ACCUM_ALPHABITS(UINT32_MAX-9) ;
   TEST(x11attr.name      == x11attribute_ACCUM_ALPHABITS) ;
   TEST(x11attr.value.u32 == UINT32_MAX-9) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

int unittest_platform_X11_x11attribute()
{
   if (test_initfree())       goto ONABORT ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

#endif
