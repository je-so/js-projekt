/* title: GLSystemKonfig

   Contains OpenGL specific include files to support calling of OpenGL
   functions.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2014 Jörg Seebohn

   file: C-kern/api/platform/Linux/graphic/sysgl.h
    Header file <GLSystemKonfig>.
*/
#ifndef CKERN_PLATFORM_LINUX_GRAPHIC_SYSGL_HEADER
#define CKERN_PLATFORM_LINUX_GRAPHIC_SYSGL_HEADER

// section: Opengl-Configuration

// group: Default-Includes

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#undef GL_GLEXT_PROTOTYPES

#endif
