/* title: EGL-Window

   Wraps a native window into an EGL window surface.
   The EGL surface serves as a drawing target for
   OpenGL and OpenGL ES drawing commands.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2014 Jörg Seebohn

   file: C-kern/api/platform/OpenGL/EGL/eglwindow.h
    Header file <EGL-Window>.

   file: C-kern/platform/OpenGL/EGL/eglwindow.c
    Implementation file <EGL-Window impl>.
*/
#ifndef CKERN_PLATFORM_OPENGL_EGL_EGLWINDOW_HEADER
#define CKERN_PLATFORM_OPENGL_EGL_EGLWINDOW_HEADER

// forward
struct opengl_config_t;
struct opengl_display_t;

/* typedef: sys_window_t
 * Type which tags the native implementation of a window handle. */
typedef struct sys_window_t sys_window_t;

/* typedef: struct eglwindow_t
 * Export <eglwindow_t> into global namespace. */
typedef struct opengl_surface_t * eglwindow_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_platform_opengl_egl_eglwindow
 * Test <eglwindow_t> functionality. */
int unittest_platform_opengl_egl_eglwindow(void);
#endif


/* struct: eglwindow_t
 * Wraps a native window into an EGL specific type. */
typedef struct opengl_surface_t * eglwindow_t;

// group: lifetime

/* define: eglwindow_FREE
 * Static initializer. */
#define eglwindow_FREE 0

/* function: init_eglwindow
 * Initializes eglwin with syswin.
 * Returns EINVAL if any parameter is not initialized or invalid
 * or EALLOC if the egl specific part of the window could not be initialized.
 * Parameter egldisp must be a valid egldisplay_t and parameter eglconf a valid eglconfig_t.
 * Do not free syswin as long as eglwin is not freed. */
int init_eglwindow(/*out*/eglwindow_t * eglwin, struct opengl_display_t * egldisp, struct opengl_config_t * eglconf, struct sys_window_t * syswin);

/* function: free_eglwindow
 * Frees resources allocated by eglwin window surface.
 * Parameter egldisp must point to a valid egl egldisp. */
int free_eglwindow(eglwindow_t * eglwin, struct opengl_display_t * egldisp);

// group: update

/* function: swapbuffer_eglwindow
 * Swaps the content of the back and from buffer.
 * The front is the one which is shown on the screen.
 * The back buffer is the offscreen drawing area.
 * Call this function whenever you are ready with drawing.
 * After return the content of the back buffer is undefined. */
int swapbuffer_eglwindow(eglwindow_t eglwin, struct opengl_display_t * egldisp);


#endif
