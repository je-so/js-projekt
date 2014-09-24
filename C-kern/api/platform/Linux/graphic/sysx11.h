/* title: X11SystemKonfig

   Contains X11 windows specific include files.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2014 Jörg Seebohn

   file: C-kern/api/platform/Linux/graphic/sysx11.h
    Header file <X11SystemKonfig>.
*/
#ifndef CKERN_PLATFORM_LINUX_GRAPHIC_SYSX11_HEADER
#define CKERN_PLATFORM_LINUX_GRAPHIC_SYSX11_HEADER

// section: X11-Window-System-Configuration

// group: Default-Includes

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>

// group: Extension-Includes

#include <X11/extensions/Xdbe.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/Xrender.h>


#endif
