/* title: EGL-OpenGL-Binding

   Contains basic functionality like error handling.

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

   file: C-kern/api/platform/OpenGL/EGL/egl.h
    Header file <EGL-OpenGL-Binding>.

   file: C-kern/platform/OpenGL/EGL/egl.c
    Implementation file <EGL-OpenGL-Binding impl>.
*/
#ifndef CKERN_PLATFORM_OPENGL_EGL_EGL_HEADER
#define CKERN_PLATFORM_OPENGL_EGL_EGL_HEADER


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_platform_opengl_egl_egl
 * Test <egl_t> functionality. */
int unittest_platform_opengl_egl_egl(void);
#endif


// struct: egl_t

// group: query

/* function: eglerr_egl
 * Returns the an EGL specific error code of the last called EGL function.
 * This error code is a per thread value. The error code
 * is reset to EGL_SUCCESS before return. You need to convert this error code
 * into a system/application specific error code with <aserrcode_egl>. */
int eglerr_egl(void);

/* function: aserrcode_egl
 * Converts value returned by <eglerr_egl> into a system/application specific error code. */
int aserrcode_egl(int eglerr);



#endif
