/* title: X11SystemKonfig

   Contains X11 windows and OpenGL specific include files.

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

   file: C-kern/api/platform/X11/x11syskonfig.h
    Header file <X11SystemKonfig>.
*/
#ifndef CKERN_PLATFORM_X11_X11SYSKONFIG_HEADER
#define CKERN_PLATFORM_X11_X11SYSKONFIG_HEADER

// section: X11-System-Configuration

// group: X11

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>

// group: X11-Extensions

#include <X11/extensions/Xdbe.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/Xrender.h>

// group: OpenGL

#define KONFIG_opengl 1
#if ((KONFIG_USERINTERFACE)&KONFIG_opengl)

#include <GL/gl.h>
#include <GL/glx.h>

#endif
#undef KONFIG_opengl

#endif
