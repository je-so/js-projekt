/* title: EGl-Context

   Wraps the creation of an OpenGL graphics context
   using the EGL api.

   Includes:
   You have to include <Graphic-OpenGL-Context> (C-kern/api/graphic/gcontext.h) before.

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

   file: C-kern/api/platform/OpenGL/EGL/eglcontext.h
    Header file <EGl-Context>.

   file: C-kern/platform/OpenGL/EGL/eglcontext.c
    Implementation file <EGl-Context impl>.
*/
#ifndef CKERN_PLATFORM_OPENGL_EGL_EGLCONTEXT_HEADER
#define CKERN_PLATFORM_OPENGL_EGL_EGLCONTEXT_HEADER

// forward
struct opengl_config_t;
struct opengl_display_t;
struct opengl_surface_t;

/* typedef: opengl_context_t
 * Type which tags the native implementation of an OpenGL capable graphics context. */
typedef struct opengl_context_t opengl_context_t;

/* typedef: struct eglcontext_t
 * Export <eglcontext_t> into global namespace. */
typedef opengl_context_t * eglcontext_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_platform_opengl_egl_eglcontext
 * Test <eglcontext_t> functionality. */
int unittest_platform_opengl_egl_eglcontext(void);
#endif


/* struct: eglcontext_t
 * Points to an OpenGL graphics context.
 * The implementation uses EGL to create the context. */
typedef opengl_context_t * eglcontext_t;

// group: lifetime

/* define: eglcontext_INIT_FREEABLE
 * Static initializer. */
#define eglcontext_INIT_FREEABLE 0

/* function: init_eglcontext
 * Initializes eglcont with a graphic context.
 * The context supports rendering with a render API determined by api.
 * Parameter api must be a value from <gcontext_api_e>.
 * If eglconf does not support the rendering API EINVAL is returned.
 * The version used by OpenGLES is hard coded to 2. */
int init_eglcontext(/*out*/eglcontext_t * eglcont, struct opengl_display_t * egldisp, struct opengl_config_t * eglconf, uint8_t api);

/* function: free_eglcontext
 * Frees all associated resources with this context.
 * Make sure that the context is not current (active)
 * to any thread before calling this function. */
int free_eglcontext(eglcontext_t * eglcont, struct opengl_display_t * egldisp);

// group: query

/* function: api_eglcontext
 * Returns in api the client rendering API the context supports.
 * See <gcontext_api_e> for a list of possible values. */
int api_eglcontext(const eglcontext_t eglcont, struct opengl_display_t * egldisp, /*out*/uint8_t  * api);

/* function: configid_eglcontext
 * Returns id of the configuration used during context creation in configid. */
int configid_eglcontext(const eglcontext_t eglcont, struct opengl_display_t * egldisp, /*out*/uint32_t * configid);

/* function: current_eglcontext
 * Gibt den aktuellen OpenGL Kontext in eglcont zurück.
 * Die damit verbundenen Typen egldisp, drawsurf and readsurf werden auch zurückgegeben,
 * es sei denn, ein Parameter ist auf 0 gesetzt.
 * FEHLER: Kann für alle out Parameter 0 zurückliefern, falls aktuelles API != <gcontext_api_OPENGLES>. */
void current_eglcontext(/*out*/eglcontext_t * eglcont, /*out*/struct opengl_display_t ** egldisp, /*out*/struct opengl_surface_t ** drawsurf, /*out*/struct opengl_surface_t ** readsurf);

// group: update

/* function: setcurrent_eglcontext
 * Setzt den aktuellen OpenGL Kontext für diesen Thread auf eglcont.
 * Das aktuelle render API wird auf das von eglcont unterstützte API umgeschalten.
 * Gibt EACCES zurück falls eglcont gerade von einem anderen Thread verwendet wird
 * oder drawsurf bzw. readsurf an einen anderen Kontext gebunden sind. */
int setcurrent_eglcontext(const eglcontext_t eglcont, struct opengl_display_t * egldisp, struct opengl_surface_t * drawsurf, struct opengl_surface_t * readsurf);

/* function: releasecurrent_eglcontext
 * Der Verbindung des Threads mit dem aktuellen Kontextes wird aufgehoben.
 * Nach erfolgreicher Rückkehr gibt <current_eglcontext> 0 in allen out Parametern zurück. */
int releasecurrent_eglcontext(struct opengl_display_t * egldisp);

#endif
