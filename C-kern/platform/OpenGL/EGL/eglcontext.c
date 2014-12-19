/* title: EGl-Context impl

   Implements <EGl-Context>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2014 Jörg Seebohn

   file: C-kern/api/platform/OpenGL/EGL/eglcontext.h
    Header file <EGl-Context>.

   file: C-kern/platform/OpenGL/EGL/eglcontext.c
    Implementation file <EGl-Context impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/platform/OpenGL/EGL/eglcontext.h"
#include "C-kern/api/err.h"
#include "C-kern/api/graphic/gcontext.h"
#include "C-kern/api/platform/OpenGL/EGL/egl.h"
#include "C-kern/api/platform/OpenGL/EGL/eglconfig.h"
#include "C-kern/api/platform/OpenGL/EGL/egldisplay.h"
#include "C-kern/api/platform/OpenGL/EGL/eglpbuffer.h"
#include "C-kern/api/platform/OpenGL/EGL/eglwindow.h"
#include "C-kern/api/test/errortimer.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/test/resourceusage.h"
#include "C-kern/api/platform/OpenGL/EGL/eglpbuffer.h"
#include "C-kern/api/platform/task/thread.h"
#endif
#include STR(C-kern/api/platform/KONFIG_OS/graphic/sysegl.h)


// section: eglcontext_t

// group: static variables

#ifdef KONFIG_UNITTEST
/* variable: s_eglcontext_errtimer
 * Allows to introduce artificial errors during test. */
static test_errortimer_t   s_eglcontext_errtimer = test_errortimer_FREE ;
#endif

// group: lifetime

int init_eglcontext(/*out*/eglcontext_t * eglcont, egldisplay_t egldisp, eglconfig_t eglconf, uint8_t api)
{
   int err;
   EGLContext ctx;
   EGLenum eglapi = eglQueryAPI();

   VALIDATE_INPARAM_TEST(api < gcontext_api__NROF, ONERR_INPARAM,);

   static_assert( EGL_OPENGL_ES_API != 0
                  && EGL_OPENVG_API == EGL_OPENGL_ES_API+1
                  && EGL_OPENGL_API == EGL_OPENGL_ES_API+2
                  && gcontext_api_OPENGLES == 0
                  && gcontext_api_OPENVG   == 1
                  && gcontext_api_OPENGL   == 2
                  && gcontext_api__NROF == 3, "simple conversion");

   if (EGL_FALSE == eglBindAPI((EGLenum)(EGL_OPENGL_ES_API + api))) {
      goto ONERR;
   }

   const EGLint * attr = (api == gcontext_api_OPENGLES)
                       ? (const EGLint[]) { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE }
                       : 0;

   ctx = eglCreateContext(egldisp, eglconf, EGL_NO_CONTEXT, attr);
   if (ctx == EGL_NO_CONTEXT) goto ONERR;

   *eglcont = ctx;

   (void) eglBindAPI(eglapi);

   return 0;
ONERR:
   err = convert2errno_egl(eglGetError());
ONERR_INPARAM:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int free_eglcontext(eglcontext_t * eglcont, egldisplay_t egldisp)
{
   int err;

   if (*eglcont) {
      EGLBoolean isDestoyed = eglDestroyContext(egldisp, *eglcont);

      *eglcont = 0;

      if (EGL_FALSE == isDestoyed) {
         err = convert2errno_egl(eglGetError());
         goto ONERR;
      }

      ONERROR_testerrortimer(&s_eglcontext_errtimer, &err, ONERR);
   }

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

// group: query

int api_eglcontext(const eglcontext_t eglcont, egldisplay_t egldisp, /*out*/uint8_t * api)
{
   int err;
   EGLint value = 0;

   if (EGL_FALSE == eglQueryContext(egldisp, eglcont, EGL_CONTEXT_CLIENT_TYPE, &value)) {
      err = convert2errno_egl(eglGetError());
      goto ONERR;
   }

   static_assert( EGL_OPENGL_ES_API != 0
                  && EGL_OPENVG_API == EGL_OPENGL_ES_API+1
                  && EGL_OPENGL_API == EGL_OPENGL_ES_API+2
                  && gcontext_api_OPENGLES == 0
                  && gcontext_api_OPENVG   == 1
                  && gcontext_api_OPENGL   == 2
                  && gcontext_api__NROF == 3, "simple conversion");

   *api = (uint8_t) (value - EGL_OPENGL_ES_API);

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int configid_eglcontext(const eglcontext_t eglcont, struct opengl_display_t * egldisp, /*out*/uint32_t * configid)
{
   int err;
   EGLint value = 0;
   static_assert(sizeof(EGLint) <= sizeof(uint32_t), "attribute type can be converted to returned type");

   if (EGL_FALSE == eglQueryContext(egldisp, eglcont, EGL_CONFIG_ID, &value)) {
      err = convert2errno_egl(eglGetError());
      goto ONERR;
   }

   *configid = (uint32_t) value;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

void current_eglcontext(/*out*/eglcontext_t * eglcont, /*out*/egldisplay_t * egldisp, /*out*/struct opengl_surface_t ** drawsurf, /*out*/struct opengl_surface_t ** readsurf)
{
   if (eglcont) {
      static_assert(EGL_NO_CONTEXT == 0 && eglcontext_FREE == 0, "are the same");
      *eglcont = eglGetCurrentContext();
   }

   if (egldisp) {
      static_assert(EGL_NO_DISPLAY == 0 && egldisplay_FREE == 0, "are the same");
      *egldisp  = eglGetCurrentDisplay();
   }

   if (drawsurf) {
      static_assert(EGL_NO_SURFACE == 0 && eglwindow_FREE == 0 && eglpbuffer_FREE == 0, "are the same");
      *drawsurf = eglGetCurrentSurface(EGL_DRAW);
   }

   if (readsurf) {
      static_assert(EGL_NO_SURFACE == 0 && eglwindow_FREE == 0 && eglpbuffer_FREE == 0, "are the same");
      *readsurf = eglGetCurrentSurface(EGL_READ);
   }
}

// group: update

int setcurrent_eglcontext(const eglcontext_t eglcont, egldisplay_t egldisp, struct opengl_surface_t * drawsurf, struct opengl_surface_t * readsurf)
{
   int err;
   EGLenum eglapi = eglQueryAPI();
   eglcontext_t old_eglcont;
   egldisplay_t old_egldisp = 0;
   struct opengl_surface_t * old_drawsurf;
   struct opengl_surface_t * old_readsurf;

   current_eglcontext(&old_eglcont, &old_egldisp, &old_drawsurf, &old_readsurf);

   if (EGL_FALSE == eglMakeCurrent(egldisp, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT)) {
      goto ONERR;
   }

   EGLint contextapi = 0;

   if (EGL_FALSE == eglQueryContext(egldisp, eglcont, EGL_CONTEXT_CLIENT_TYPE, &contextapi)) {
      goto ONERR;
   }

   if (EGL_FALSE == eglBindAPI((EGLenum)contextapi)) {
      goto ONERR;
   }

   if (EGL_FALSE == eglMakeCurrent(egldisp, drawsurf, readsurf, eglcont)) {
      goto ONERR;
   }

   return 0;
ONERR:
   err = convert2errno_egl(eglGetError());
   if (old_egldisp != 0) {
      (void) eglBindAPI(eglapi);
      (void) eglMakeCurrent(old_egldisp, old_drawsurf, old_readsurf, old_eglcont);
   }
   TRACEEXIT_ERRLOG(err);
   return err;
}

int releasecurrent_eglcontext(struct opengl_display_t * egldisp)
{
   int err;

   if (EGL_FALSE == eglMakeCurrent(egldisp, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT)) {
      err = convert2errno_egl(eglGetError());
      goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(egldisplay_t disp)
{
   eglcontext_t   cont = eglcontext_FREE;
   gcontext_api_e capi[] = { gcontext_api_OPENGLES, gcontext_api_OPENVG, gcontext_api_OPENGL };
   EGLenum        api[]  = { EGL_OPENGL_ES_API, EGL_OPENVG_API, EGL_OPENGL_API };
   EGLint         apibit[] = { EGL_OPENGL_ES2_BIT, EGL_OPENVG_BIT, EGL_OPENGL_BIT };

   // TEST eglcontext_FREE
   TEST(0 == cont);

   for (unsigned i = 0; i < lengthof(apibit); ++i) {
      EGLint    listsize = 0;
      EGLConfig conflist[8];
      EGLint    attr[] = { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
                           EGL_CONFORMANT, apibit[i],
                           EGL_NONE };
      TEST(0 != eglChooseConfig(disp, attr, conflist, lengthof(conflist), &listsize));
      TEST(0 <  listsize);

      EGLenum oldapi = api[! i];
      TEST(EGL_FALSE != eglBindAPI(oldapi));

      for (int ci = 0; ci < listsize; ++ci) {
         EGLint id1 = 0;
         EGLint id2 = 1;
         EGLint api2 = -1;

         TEST(EGL_FALSE != eglGetConfigAttrib(disp, conflist[ci], EGL_CONFIG_ID, &id2));

         // TEST init_eglcontext
         TEST(0 == init_eglcontext(&cont, disp, conflist[ci], capi[i]));
         TEST(0 != cont);
         TEST(EGL_FALSE != eglQueryContext(disp, cont, EGL_CONFIG_ID, &id1));
         TEST(id1 == id2);
         TEST(EGL_FALSE != eglQueryContext(disp, cont, EGL_CONTEXT_CLIENT_TYPE, &api2));
         TEST(api2 == (EGLint)api[i])

         TEST(oldapi == eglQueryAPI());

         // TEST free_eglcontext
         TEST(0 == free_eglcontext(&cont, disp));
         TEST(0 == cont);
         TEST(0 == free_eglcontext(&cont, disp));
         TEST(0 == cont);
      }
   }

   TEST(EGL_FALSE != eglBindAPI(EGL_OPENGL_ES_API));

   // prepare
   EGLint    listsize = 0;
   EGLConfig conflist[1];
   EGLint    attr[] = { EGL_CONFORMANT, EGL_OPENGL_ES2_BIT, EGL_NONE };
   TEST(0 != eglChooseConfig(disp, attr, conflist, lengthof(conflist), &listsize));
   TEST(1 == listsize);

   // TEST init_eglcontext: EINVAL
   TEST(EINVAL == init_eglcontext(&cont, disp, conflist[0], gcontext_api__NROF));
   TEST(0 == cont);
   TEST(EGL_OPENGL_ES_API == eglQueryAPI());

   // TEST free_eglcontext: ERROR
   TEST(0 == init_eglcontext(&cont, disp, conflist[0], gcontext_api_OPENGLES));
   TEST(0 != cont);
   init_testerrortimer(&s_eglcontext_errtimer, 1, 9);
   TEST(9 == free_eglcontext(&cont, disp));
   TEST(0 == cont);

   return 0;
ONERR:
   eglBindAPI(EGL_OPENGL_ES_API);
   free_eglcontext(&cont, disp);
   return EINVAL;
}

static int test_query(egldisplay_t disp)
{
   eglcontext_t   cont = eglcontext_FREE;
   gcontext_api_e capi[] = { gcontext_api_OPENGLES, gcontext_api_OPENVG, gcontext_api_OPENGL };
   EGLint         apibit[] = { EGL_OPENGL_ES2_BIT, EGL_OPENVG_BIT, EGL_OPENGL_BIT };

   TEST(EGL_OPENGL_ES_API == eglQueryAPI());

   for (unsigned i = 0; i < lengthof(apibit); ++i) {
      EGLint    listsize = 0;
      EGLConfig conflist[4];
      EGLint    attr[] = { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
                           EGL_CONFORMANT, apibit[i],
                           EGL_NONE };
      TEST(0 != eglChooseConfig(disp, attr, conflist, lengthof(conflist), &listsize));
      TEST(0 <  listsize);

      for (int ci = 0; ci < listsize; ++ci) {
         TEST(0 == init_eglcontext(&cont, disp, conflist[ci], capi[i]));

         // TEST api_eglcontext
         uint8_t api2;
         TEST(0 == api_eglcontext(cont, disp, &api2));
         TEST(api2 == capi[i]);

         // TEST configid_eglcontext
         uint32_t configid  = 0;
         EGLint   configid2 = 1;
         TEST(0 == configid_eglcontext(cont, disp, &configid));
         TEST(EGL_FALSE != eglGetConfigAttrib(disp, conflist[ci], EGL_CONFIG_ID, &configid2));
         TEST(configid2 == (EGLint)configid);

         TEST(0 == free_eglcontext(&cont, disp));
      }

   }

   TEST(EGL_OPENGL_ES_API == eglQueryAPI());

   return 0;
ONERR:
   free_eglcontext(&cont, disp);
   return EINVAL;
}

static int test_current(egldisplay_t disp)
{
   eglcontext_t   cont = eglcontext_FREE;
   eglcontext_t   cont2 = eglcontext_FREE;
   gcontext_api_e capi[] = { gcontext_api_OPENGLES, gcontext_api_OPENVG, gcontext_api_OPENGL };
   EGLenum        api[]  = { EGL_OPENGL_ES_API, EGL_OPENVG_API, EGL_OPENGL_API };
   EGLint         apibit[] = { EGL_OPENGL_ES2_BIT, EGL_OPENVG_BIT, EGL_OPENGL_BIT };
   eglpbuffer_t   pbuf[2] = { eglpbuffer_FREE, eglpbuffer_FREE };
   eglcontext_t   current_cont = eglcontext_FREE;
   egldisplay_t   current_disp = egldisplay_FREE;
   eglpbuffer_t   current_draw = eglpbuffer_FREE;
   eglpbuffer_t   current_read = eglpbuffer_FREE;

   for (unsigned i = 0; i < lengthof(apibit); ++i) {
      EGLint    listsize = 0;
      EGLConfig conflist[1];
      EGLint    attr[] = { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
                           EGL_CONFORMANT, apibit[i],
                           EGL_NONE };
      TEST(0 != eglChooseConfig(disp, attr, conflist, lengthof(conflist), &listsize));
      TEST(1 == listsize);

      // create 2 pixel buffer and 2 context with correct configuration
      TEST(0 == init_eglpbuffer(&pbuf[0/*draw*/], disp, conflist[0], 16, 16));
      TEST(0 == init_eglpbuffer(&pbuf[1/*read*/], disp, conflist[0], 16, 16));
      TEST(0 == init_eglcontext(&cont, disp, conflist[0], capi[i]));
      TEST(0 == init_eglcontext(&cont2, disp, conflist[0], capi[i]));

      // TEST setcurrent_eglcontext
      TEST(EGL_FALSE != eglBindAPI(api[!i]));
      TEST(0 == setcurrent_eglcontext(cont, disp, pbuf[0], pbuf[1]));
      TEST(api[i] == eglQueryAPI()); // api changed !

      // current_eglcontext: pointer set to 0
      current_eglcontext(0,0,0,0);

      // TEST current_eglcontext
      current_eglcontext(&current_cont, &current_disp, &current_draw, &current_read);
      if (0 < i && current_cont == 0) {
         // EGL_OPENVG_API and EGL_OPENGL_BIT are mapped to EGL_OPENGL_ES_API (Linux & MESA)
         TEST(EGL_FALSE != eglBindAPI(EGL_OPENGL_ES_API));
         current_eglcontext(&current_cont, &current_disp, &current_draw, &current_read);
         TEST(EGL_FALSE != eglBindAPI(api[i]));
      }
      TEST(current_cont == cont);
      TEST(current_disp == disp);
      TEST(current_draw == pbuf[0]);
      TEST(current_read == pbuf[1]);

      // TEST setcurrent_eglcontext: previous is released
      TEST(0 == setcurrent_eglcontext(cont2, disp, pbuf[0], pbuf[1]));
      current_eglcontext(&current_cont, &current_disp, &current_draw, &current_read);
      if (0 < i && current_cont == 0) {
         // EGL_OPENVG_API and EGL_OPENGL_BIT are mapped to EGL_OPENGL_ES_API (Linux & MESA)
         TEST(EGL_FALSE != eglBindAPI(EGL_OPENGL_ES_API));
         current_eglcontext(&current_cont, &current_disp, &current_draw, &current_read);
         TEST(EGL_FALSE != eglBindAPI(api[i]));
      }
      TEST(current_cont == cont2);
      TEST(current_disp == disp);
      TEST(current_draw == pbuf[0]);
      TEST(current_read == pbuf[1]);

      // TEST setcurrent_eglcontext: ERROR -> previous is restored
      TEST(EINVAL == setcurrent_eglcontext(cont, EGL_NO_DISPLAY, pbuf[0], pbuf[0]));
      TEST(EINVAL == setcurrent_eglcontext(EGL_NO_CONTEXT, disp, pbuf[0], pbuf[0]));
      current_eglcontext(&current_cont, &current_disp, &current_draw, &current_read);
      if (0 < i && current_cont == 0) {
         // EGL_OPENVG_API and EGL_OPENGL_BIT are mapped to EGL_OPENGL_ES_API (Linux & MESA)
         TEST(EGL_FALSE != eglBindAPI(EGL_OPENGL_ES_API));
         current_eglcontext(&current_cont, &current_disp, &current_draw, &current_read);
         TEST(EGL_FALSE != eglBindAPI(api[i]));
      }
      TEST(current_cont == cont2);
      TEST(current_disp == disp);
      TEST(current_draw == pbuf[0]);
      TEST(current_read == pbuf[1]);

      // TEST releasecurrent_eglcontext
      TEST(0 == releasecurrent_eglcontext(disp));

      // current_eglcontext: released context
      current_eglcontext(&current_cont, &current_disp, &current_draw, &current_read);
      TEST(current_cont == eglcontext_FREE);
      TEST(current_disp == egldisplay_FREE);
      TEST(current_draw == eglpbuffer_FREE);
      TEST(current_read == eglpbuffer_FREE);

      // unprepare
      TEST(0 == free_eglpbuffer(&pbuf[0], disp));
      TEST(0 == free_eglpbuffer(&pbuf[1], disp));
      TEST(0 == free_eglcontext(&cont, disp));
      TEST(0 == free_eglcontext(&cont2, disp));
      TEST(EGL_FALSE != eglBindAPI(EGL_OPENGL_ES_API));
   }

   return 0;
ONERR:
   releasecurrent_eglcontext(disp);
   free_eglcontext(&cont, disp);
   free_eglcontext(&cont2, disp);
   free_eglpbuffer(&pbuf[0], disp);
   free_eglpbuffer(&pbuf[1], disp);
   return EINVAL;
}

static int comparecurrent_thread(void * dummy)
{
   (void) dummy;
   eglcontext_t   current_cont;
   egldisplay_t   current_disp;
   eglpbuffer_t   current_draw;
   eglpbuffer_t   current_read;

   TEST(EGL_OPENGL_ES_API == eglQueryAPI());

   current_eglcontext(&current_cont, &current_disp, &current_draw, &current_read);
   TEST(current_cont == eglcontext_FREE);
   TEST(current_disp == egldisplay_FREE);
   TEST(current_draw == eglpbuffer_FREE);
   TEST(current_read == eglpbuffer_FREE);

   return 0;
ONERR:
   CLEARBUFFER_ERRLOG();
   return EINVAL;
}

struct setcurrent_args_t {
   eglcontext_t   cont;
   egldisplay_t   disp;
   eglpbuffer_t   pbuf;
};

static int setcurrent_thread(struct setcurrent_args_t * args)
{
   eglcontext_t   current_cont;
   egldisplay_t   current_disp;
   eglpbuffer_t   current_draw;
   eglpbuffer_t   current_read;

   TEST(EGL_OPENGL_ES_API == eglQueryAPI());
   TEST(EACCES == setcurrent_eglcontext(args->cont, args->disp, args->pbuf, args->pbuf));

   current_eglcontext(&current_cont, &current_disp, &current_draw, &current_read);
   TEST(current_cont == eglcontext_FREE);
   TEST(current_disp == egldisplay_FREE);
   TEST(current_draw == eglpbuffer_FREE);
   TEST(current_read == eglpbuffer_FREE);

   CLEARBUFFER_ERRLOG();
   return 0;
ONERR:
   CLEARBUFFER_ERRLOG();
   return EINVAL;
}


static int test_thread(egldisplay_t disp)
{
   eglcontext_t   cont  = eglcontext_FREE;
   eglpbuffer_t   pbuf  = eglpbuffer_FREE;
   thread_t     * thread = 0;

   // prepare
   EGLint    listsize = 0;
   EGLConfig conflist[1];
   EGLint    attr[] = { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
                        EGL_CONFORMANT, EGL_OPENGL_ES2_BIT|EGL_OPENGL_BIT,
                        EGL_NONE };
   TEST(0 != eglChooseConfig(disp, attr, conflist, lengthof(conflist), &listsize));
   TEST(1 == listsize);

   TEST(EGL_OPENGL_ES_API == eglQueryAPI());
   TEST(0 == init_eglpbuffer(&pbuf, disp, conflist[0], 16, 16));
   TEST(0 == init_eglcontext(&cont, disp, conflist[0], gcontext_api_OPENGLES));

   // TEST setcurrent_eglcontext
   TEST(0 == setcurrent_eglcontext(cont, disp, pbuf, pbuf));

   // TEST current_eglcontext: other thread compare to 0
   TEST(0 == new_thread(&thread, comparecurrent_thread, 0));
   TEST(0 == join_thread(thread));
   TEST(0 == returncode_thread(thread));
   TEST(0 == delete_thread(&thread));

   // TEST setcurrent_eglcontext: EACCESS if other thread tries to set current
   struct setcurrent_args_t args = { .cont = cont, .disp = disp, .pbuf = pbuf };
   TEST(0 == newgeneric_thread(&thread, &setcurrent_thread, &args));
   TEST(0 == join_thread(thread));
   TEST(0 == returncode_thread(thread));
   TEST(0 == delete_thread(&thread));

   // unprepare
   TEST(0 == releasecurrent_eglcontext(disp));
   TEST(0 == free_eglpbuffer(&pbuf, disp));
   TEST(0 == free_eglcontext(&cont, disp));

   return 0;
ONERR:
   releasecurrent_eglcontext(disp);
   free_eglcontext(&cont, disp);
   free_eglpbuffer(&pbuf, disp);
   return EINVAL;
}

static int childprocess_unittest(void)
{
   resourceusage_t   usage = resourceusage_FREE;
   egldisplay_t      disp  = egldisplay_FREE;

   // prepare
   TEST(0 == initdefault_egldisplay(&disp));

   if (test_initfree(disp))   goto ONERR;
   if (test_query(disp))      goto ONERR;
   if (test_current(disp))    goto ONERR;
   if (test_thread(disp))     goto ONERR;

   TEST(0 == init_resourceusage(&usage));

   // no other tests

   TEST(0 == same_resourceusage(&usage));
   TEST(0 == free_resourceusage(&usage));

   // unprepare
   TEST(0 == free_egldisplay(&disp));

   return 0;
ONERR:
   (void) free_resourceusage(&usage);
   (void) free_egldisplay(&disp);
   return EINVAL;
}

int unittest_platform_opengl_egl_eglcontext()
{
   int err;

   TEST(0 == execasprocess_unittest(&childprocess_unittest, &err));

   return err;
ONERR:
   return EINVAL;
}

#endif
