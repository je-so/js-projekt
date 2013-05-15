/* title: ProcessContext impl
   Implements the global context of the running system process.
   See <ProcessContext>.

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
   (C) 2012 Jörg Seebohn

   file: C-kern/api/context/processcontext.h
    Header file <ProcessContext>.

   file: C-kern/context/processcontext.c
    Implementation file <ProcessContext impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/context/processcontext.h"
#include "C-kern/api/err.h"
#include "C-kern/api/test/errortimer.h"
// TEXTDB:SELECT('#include "'header-name'"')FROM("C-kern/resource/config/initprocess")
#include "C-kern/api/context/errorcontext.h"
#include "C-kern/api/platform/locale.h"
#include "C-kern/api/platform/sysuser.h"
#include "C-kern/api/platform/sync/signal.h"
#include "C-kern/api/cache/valuecache.h"
#include "C-kern/api/platform/task/thread.h"
#include "C-kern/api/platform/X11/x11.h"
// TEXTDB:END
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


// section: processcontext_t

// group: variables

#ifdef KONFIG_UNITTEST
/* variable: s_processcontext_errtimer
 * Simulates an error in <init_processcontext>. */
static test_errortimer_t   s_processcontext_errtimer = test_errortimer_INIT_FREEABLE ;
#endif

// group: lifetime

int init_processcontext(/*out*/processcontext_t * pcontext)
{
   int err ;

   pcontext->initcount = 0 ;

// TEXTDB:SELECT(\n"   ONERROR_testerrortimer(&s_processcontext_errtimer, ONABORT) ;"\n"   err = initonce_"module"("(if (parameter!="") "&pcontext->" else "")parameter") ;"\n"   if (err) goto ONABORT ;"\n"   ++ pcontext->initcount ;")FROM("C-kern/resource/config/initprocess")

   ONERROR_testerrortimer(&s_processcontext_errtimer, ONABORT) ;
   err = initonce_errorcontext(&pcontext->error) ;
   if (err) goto ONABORT ;
   ++ pcontext->initcount ;

   ONERROR_testerrortimer(&s_processcontext_errtimer, ONABORT) ;
   err = initonce_locale() ;
   if (err) goto ONABORT ;
   ++ pcontext->initcount ;

   ONERROR_testerrortimer(&s_processcontext_errtimer, ONABORT) ;
   err = initonce_sysuser(&pcontext->sysuser) ;
   if (err) goto ONABORT ;
   ++ pcontext->initcount ;

   ONERROR_testerrortimer(&s_processcontext_errtimer, ONABORT) ;
   err = initonce_signalconfig() ;
   if (err) goto ONABORT ;
   ++ pcontext->initcount ;

   ONERROR_testerrortimer(&s_processcontext_errtimer, ONABORT) ;
   err = initonce_valuecache(&pcontext->valuecache) ;
   if (err) goto ONABORT ;
   ++ pcontext->initcount ;

   ONERROR_testerrortimer(&s_processcontext_errtimer, ONABORT) ;
   err = initonce_thread() ;
   if (err) goto ONABORT ;
   ++ pcontext->initcount ;

   ONERROR_testerrortimer(&s_processcontext_errtimer, ONABORT) ;
   err = initonce_X11() ;
   if (err) goto ONABORT ;
   ++ pcontext->initcount ;
// TEXTDB:END

   ONERROR_testerrortimer(&s_processcontext_errtimer, ONABORT) ;

   return 0 ;
ONABORT:
   (void) free_processcontext(pcontext) ;
   TRACEABORT_LOG(err) ;
   return err ;
}

int free_processcontext(processcontext_t * pcontext)
{
   int err ;
   int err2 ;

   err = 0 ;

   switch (pcontext->initcount) {
   default: assert(false && "initcount out of bounds")  ;
            break ;
// TEXTDB:SELECT(\n"   case "row-id":  err2 = freeonce_"module"("(if (parameter!="") "&pcontext->" else "")parameter") ;"\n"            if (err2) err = err2 ;")FROM("C-kern/resource/config/initprocess")DESCENDING

   case 7:  err2 = freeonce_X11() ;
            if (err2) err = err2 ;

   case 6:  err2 = freeonce_thread() ;
            if (err2) err = err2 ;

   case 5:  err2 = freeonce_valuecache(&pcontext->valuecache) ;
            if (err2) err = err2 ;

   case 4:  err2 = freeonce_signalconfig() ;
            if (err2) err = err2 ;

   case 3:  err2 = freeonce_sysuser(&pcontext->sysuser) ;
            if (err2) err = err2 ;

   case 2:  err2 = freeonce_locale() ;
            if (err2) err = err2 ;

   case 1:  err2 = freeonce_errorcontext(&pcontext->error) ;
            if (err2) err = err2 ;
// TEXTDB:END
   case 0:  break ;
   }

   pcontext->initcount = 0 ;

   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   processcontext_t  pcontext = processcontext_INIT_STATIC ;
   const uint16_t    I        = 7 ;

   // TEST processcontext_INIT_STATIC
   TEST(0 == pcontext.valuecache) ;
   TEST(0 == pcontext.sysuser) ;
   TEST(0 != pcontext.error.stroffset) ;
   TEST(0 != pcontext.error.strdata) ;
   TEST(0 == pcontext.initcount) ;

   // TEST init_processcontext
   TEST(0 == init_processcontext(&pcontext)) ;
   TEST(0 != pcontext.valuecache) ;
   TEST(0 != pcontext.sysuser) ;
   TEST(0 != pcontext.error.stroffset) ;
   TEST(0 != pcontext.error.strdata) ;
   TEST(I == pcontext.initcount) ;
   TEST(sizestatic_maincontext() == 2*processcontext_STATICSIZE) ;

   // TEST free_processcontext
   TEST(0 == free_processcontext(&pcontext)) ;
   TEST(0 == pcontext.valuecache) ;
   TEST(0 == pcontext.sysuser) ;
   TEST(0 != pcontext.error.stroffset) ;
   TEST(0 != pcontext.error.strdata) ;
   TEST(0 == pcontext.initcount) ;
   TEST(sizestatic_maincontext() == processcontext_STATICSIZE) ;

   // TEST free_processcontext: initcount == 0
   pcontext.initcount = 0 ;
   pcontext.valuecache = (struct valuecache_t*) 1 ;
   pcontext.sysuser    = (struct sysuser_t*) 2 ;
   pcontext.error.stroffset = (uint16_t*)3 ;
   pcontext.error.strdata   = (uint8_t*)4 ;
   TEST(0 == free_processcontext(&pcontext)) ;
   TEST(pcontext.valuecache == (struct valuecache_t*) 1) ;
   TEST(pcontext.sysuser    == (struct sysuser_t*) 2) ;
   TEST(pcontext.error.stroffset == (uint16_t*)3) ;
   TEST(pcontext.error.strdata   == (uint8_t*)4) ;
   TEST(sizestatic_maincontext() == processcontext_STATICSIZE) ;

   // TEST init_processcontext: ERROR
   pcontext = (processcontext_t) processcontext_INIT_STATIC ;
   for(int i = 1; i; ++i) {
      init_testerrortimer(&s_processcontext_errtimer, (unsigned)i, i) ;
      int err = init_processcontext(&pcontext) ;
      if (err == 0) {
         TEST(0 == free_processcontext(&pcontext)) ;
         TEST(i > I) ;
         break ;
      }
      TEST(i == err) ;
      TEST(0 == pcontext.valuecache) ;
      TEST(0 == pcontext.sysuser) ;
      TEST(0 != pcontext.error.stroffset) ;
      TEST(0 != pcontext.error.strdata) ;
      TEST(0 == pcontext.initcount) ;
      TEST(sizestatic_maincontext() == processcontext_STATICSIZE) ;
   }
   init_testerrortimer(&s_processcontext_errtimer, 0, 0) ;

   // TEST init_processcontext: restore default environment
   pcontext = (processcontext_t) processcontext_INIT_STATIC ;
   TEST(0 == init_processcontext(&pcontext)) ;
   TEST(I == pcontext.initcount) ;
   TEST(sizestatic_maincontext() == 2*processcontext_STATICSIZE) ;
   freeonce_valuecache(&pcontext.valuecache) ;
   freeonce_sysuser(&pcontext.sysuser) ;
   // restore (if setuid)
   switchtoreal_sysuser(sysuser_maincontext()) ;
   TEST(sizestatic_maincontext() == processcontext_STATICSIZE) ;

   return 0 ;
ONABORT:
   (void) free_processcontext(&pcontext) ;
   return EINVAL ;
}

int unittest_context_processcontext()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   if (test_initfree())    goto ONABORT ;
   CLEARBUFFER_LOG() ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())    goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
