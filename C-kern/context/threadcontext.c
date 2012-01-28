/* title: ThreadContext impl
   Implements the global (thread local) context of a running system thread.
   See <ThreadContext>.

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

   file: C-kern/api/context/threadcontext.h
    Header file <ThreadContext>.

   file: C-kern/context/threadcontext.c
    Implementation file <ThreadContext impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/context/threadcontext.h"
#include "C-kern/api/err.h"
#include "C-kern/api/test/errortimer.h"
#include "C-kern/api/writer/logmain.h"
// TEXTDB:SELECT('#include "'header-name'"')FROM("C-kern/resource/text.db/initthread")
#include "C-kern/api/cache/objectcache.h"
#include "C-kern/api/writer/logwriter.h"
// TEXTDB:END
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


// section: threadcontext_t

// group: variables

#ifdef KONFIG_UNITTEST
/* variable: s_error_init
 * Simulates an error in <init_thread_resources>. */
static test_errortimer_t   s_error_init = test_errortimer_INIT_FREEABLE ;
#endif

// group: lifetime

int free_threadcontext(threadcontext_t * tcontext)
{
   int err = 0 ;
   int err2 ;

   uint16_t  initcount = tcontext->initcount ;
   tcontext->initcount = 0 ;

   switch(initcount) {
   default:    assert(0 != tcontext->initcount && "out of bounds") ;
               break ;
// TEXTDB:SELECT("   case "row-id":     err2 = freethread_"module"("(if (parameter!="") "&tcontext->" else "")parameter") ;"\n"               if (err2) err = err2 ;")FROM(C-kern/resource/text.db/initthread)DESCENDING
   case 2:     err2 = freethread_logwriter(&tcontext->ilog) ;
               if (err2) err = err2 ;
   case 1:     err2 = freethread_objectcache(&tcontext->objectcache) ;
               if (err2) err = err2 ;
// TEXTDB:END
   case 0:     break ;
   }

   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT_FREE(err) ;
   return err ;
}

int init_threadcontext(/*out*/threadcontext_t * tcontext)
{
   int err ;

   *tcontext = (threadcontext_t) threadcontext_INIT_STATIC ;

   VALIDATE_STATE_TEST(context_STATIC != type_context(), ABBRUCH, ) ;

// TEXTDB:SELECT(\n"   ONERROR_testerrortimer(&s_error_init, ABBRUCH) ;"\n"   err = initthread_"module"("(if (parameter!="") "&tcontext->")parameter") ;"\n"   if (err) goto ABBRUCH ;"\n"   ++tcontext->initcount ;")FROM(C-kern/resource/text.db/initthread)

   ONERROR_testerrortimer(&s_error_init, ABBRUCH) ;
   err = initthread_objectcache(&tcontext->objectcache) ;
   if (err) goto ABBRUCH ;
   ++tcontext->initcount ;

   ONERROR_testerrortimer(&s_error_init, ABBRUCH) ;
   err = initthread_logwriter(&tcontext->ilog) ;
   if (err) goto ABBRUCH ;
   ++tcontext->initcount ;
// TEXTDB:END

   ONERROR_testerrortimer(&s_error_init, ABBRUCH) ;

   return 0 ;
ABBRUCH:
   (void) free_threadcontext(tcontext) ;
   LOG_ABORT(err) ;
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

#define TEST(CONDITION) TEST_ONERROR_GOTO(CONDITION, ABBRUCH)

static int test_initfree(void)
{
   threadcontext_t  tcontext = threadcontext_INIT_STATIC ;

   // TEST threadcontext_INIT_STATIC
   TEST(0 == tcontext.initcount) ;
   TEST(&g_logmain == tcontext.ilog.object) ;
   TEST(&g_logmain_interface == tcontext.ilog.functable) ;
   TEST(0 == tcontext.objectcache.object) ;
   TEST(0 == tcontext.objectcache.functable) ;

   // TEST init, double free
   TEST(0 == init_threadcontext(&tcontext)) ;
   TEST(tcontext.initcount      == 2) ;
   TEST(tcontext.ilog.object    != 0) ;
   TEST(tcontext.ilog.object    != &g_logmain) ;
   TEST(tcontext.ilog.functable != &g_logmain_interface) ;
   TEST(tcontext.objectcache.object    != 0) ;
   TEST(tcontext.objectcache.functable != 0) ;
   TEST(0 == free_threadcontext(&tcontext)) ;
   TEST(0 == tcontext.initcount) ;
   TEST(&g_logmain == tcontext.ilog.object) ;
   TEST(&g_logmain_interface == tcontext.ilog.functable) ;
   TEST(0 == tcontext.objectcache.object) ;
   TEST(0 == tcontext.objectcache.functable) ;
   TEST(0 == free_threadcontext(&tcontext)) ;
   TEST(0 == tcontext.initcount) ;
   TEST(&g_logmain == tcontext.ilog.object) ;
   TEST(&g_logmain_interface == tcontext.ilog.functable) ;
   TEST(0 == tcontext.objectcache.object) ;
   TEST(0 == tcontext.objectcache.functable) ;

   // TEST EINVAL init
   for(int i = 0; i < 3; ++i) {
      TEST(0 == init_testerrortimer(&s_error_init, 1u+(unsigned)i, EINVAL+i)) ;
      memset(&tcontext, 0xff, sizeof(tcontext)) ;
      TEST(EINVAL+i == init_threadcontext(&tcontext)) ;
      TEST(0 == tcontext.initcount) ;
      TEST(&g_logmain == tcontext.ilog.object) ;
      TEST(&g_logmain_interface == tcontext.ilog.functable) ;
      TEST(0 == tcontext.objectcache.object) ;
      TEST(0 == tcontext.objectcache.functable) ;
   }

   return 0 ;
ABBRUCH:
   s_error_init = (test_errortimer_t) test_errortimer_INIT_FREEABLE ;
   return EINVAL ;
}

int unittest_context_threadcontext()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;


   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())    goto ABBRUCH ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ABBRUCH:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
