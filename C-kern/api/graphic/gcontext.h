/* title: Graphic-OpenGL-Context

   Abstracts the os specific way to create
   an OpenGL graphic context.

   Uses lower level of abstractions like <eglcontext_t>
   in case of EGL to adapt OpenGL to the local windowing system.

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

   file: C-kern/api/graphic/gcontext.h
    Header file <Graphic-OpenGL-Context>.

   file: C-kern/graphic/gcontext.c
    Implementation file <Graphic-OpenGL-Context impl>.
*/
#ifndef CKERN_GRAPHIC_GCONTEXT_HEADER
#define CKERN_GRAPHIC_GCONTEXT_HEADER

// forward
struct display_t;
struct gconfig_t;
struct surface_t;

#ifdef KONFIG_USERINTERFACE_EGL
#include "C-kern/api/platform/OpenGL/EGL/eglcontext.h"
#endif

/* typedef: struct gcontext_t
 * Export <gcontext_t> into global namespace. */
typedef struct gcontext_t gcontext_t;

/* enums: gcontext_api_e
 * Determines the rendering API the graphic context supports
 * and the client is using for drawing calls.
 *
 * gcontext_api_OPENGLES - The client uses api OpenGL ES for drawing.
 *                         The default version is 2.
 *                         The version can be configured during creation of <eglcontext_t>.
 * gcontext_api_OPENVG   - The client uses api OpenVG for drawing.
 * gcontext_api_OPENGL   - The client uses api OpenGL for drawing.
 * */
enum gcontext_api_e {
   gcontext_api_OPENGLES, // version 2
   gcontext_api_OPENVG,
   gcontext_api_OPENGL,
   gcontext_api_NROFELEMENTS
};

typedef enum gcontext_api_e gcontext_api_e;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_graphic_gcontext
 * Test <gcontext_t> functionality. */
int unittest_graphic_gcontext(void);
#endif


/* struct: gcontext_t
 * Wraps the native implementation of an OpenGL context. */
struct gcontext_t {
   struct opengl_context_t * glcontext;
};

// group: lifetime

/* define: gcontext_INIT_FREEABLE
 * Static initializer. */
#define gcontext_INIT_FREEABLE \
         { 0 }

/* function: init_gcontext
 * Creates a new native OpenGL graphic context.
 * The context supports rendering with a render API determined by api.
 * Parameter api must be a value from <gcontext_api_e>.
 * If eglconf does not support the rendering API EINVAL is returned.
 * A copy of the pointer disp is stored internally so do not free <display_t> disp
 * until win has been freed. */
int init_gcontext(/*out*/gcontext_t * cont, struct display_t * disp, struct gconfig_t * gconf, uint8_t api);

/* function: free_gcontext
 * Frees graphic context cont and associated resources (frame buffers).
 * Before freeing you should make sure that this context is
 * not current to any thread. */
int free_gcontext(gcontext_t * cont, struct display_t * disp);

// group: query

/* function: gl_gcontext
 * Returns the native OpenGL context. */
struct opengl_context_t * gl_gcontext(const gcontext_t * cont);

/* function: api_gcontext
 * Returns in api the client rendering API cont supports.
 * See <gcontext_api_e> for a list of possible values. */
int api_gcontext(const gcontext_t * cont, struct display_t * disp, /*out*/uint8_t * api);

/* function: configid_gcontext
 * Returns id of the configuration cont in configid used during context creation. */
int configid_gcontext(const gcontext_t * cont, struct display_t * disp, /*out*/uint32_t * configid);

/* function: current_gcontext
 * Returns the current native OpenGL context and associated native resources.
 * A an out pointer is set to 0 no value is returned.
 *
 * Bug: This function returns 0 for any out parameter in case the current context
 * uses not API <gcontext_api_OPENGLES>. */
void current_gcontext(/*out*/struct opengl_context_t ** cont, /*out*/struct opengl_display_t ** disp, /*out*/struct opengl_surface_t ** drawsurf, /*out*/struct opengl_surface_t ** readsurf);

// group: update

/* function: setcurrent_gcontext
 * Setzt den aktuellen OpenGL Kontext für diesen Thread auf cont.
 * Das aktuelle render API wird auf das von cont unterstützte API umgeschalten.
 * Gibt EACCES zurück falls cont gerade von einem anderen Thread verwendet wird
 * oder drawsurf bzw. readsurf an einen anderen Kontext gebunden sind.
 *
 * Argumente drawsurf / readsurf akzeptieren (da als Makro implementiert):
 * - Pointer zu <window_t>
 * - Pointer zu <pixelbuffer_t
 * */
int setcurrent_gcontext(const gcontext_t cont, struct display_t * disp, struct surface_t * drawsurf, struct surface_t * readsurf);

/* function: releasecurrent_gcontext
 * Der Verbindung des Threads mit dem aktuellen Kontextes wird aufgehoben.
 * Nach erfolgreicher Rückkehr ist der Thread an keinen Graphik-Kontext mehr gebunden. */
int releasecurrent_gcontext(struct display_t * disp);


// section: inline implementation

// group: gcontext_t

/* define: gl_gcontext
 * Implements <gcontext_t.gl_gcontext>. */
#define gl_gcontext(cont) \
         ((cont)->glcontext)

#if defined(KONFIG_USERINTERFACE_EGL)

// EGL specific implementation (OpenGL ES...)

/* define: api_gcontext
 * Implements <gcontext_t.api_gcontext>. */
#define api_gcontext(cont, disp, api) \
         api_eglcontext(gl_gcontext(cont), gl_display(disp), api)

/* define: configid_gcontext
 * Implements <gcontext_t.configid_gcontext>. */
#define configid_gcontext(cont, disp, configid) \
         configid_eglcontext(gl_gcontext(cont), gl_display(disp), configid)

/* define: current_gcontext
 * Implements <gcontext_t.current_gcontext>. */
#define current_gcontext(cont, disp, drawsurf, readsurf) \
         current_eglcontext(cont, disp, drawsurf, readsurf)

/* define: releasecurrent_gcontext
 * Implements <gcontext_t.releasecurrent_gcontext>. */
#define releasecurrent_gcontext(disp) \
         releasecurrent_eglcontext(gl_display(disp))

/* define: setcurrent_gcontext
 * Implements <gcontext_t.setcurrent_gcontext>. */
#define setcurrent_gcontext(cont, disp, drawsurf, readsurf) \
         setcurrent_eglcontext(gl_gcontext(cont), gl_display(disp), gl_surface(drawsurf), gl_surface(readsurf))

#else
   #error "Not implemented"
#endif

#endif
