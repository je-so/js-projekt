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
#include "C-kern/api/io/writer/log/logmain.h"
// TEXTDB:SELECT('#include "'header-name'"')FROM("C-kern/resource/config/initthread")
#include "C-kern/api/memory/mm/mmtransient.h"
#include "C-kern/api/memory/pagecache_impl.h"
#include "C-kern/api/cache/objectcache_impl.h"
#include "C-kern/api/io/writer/log/logwriter.h"
// TEXTDB:END
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/memory/pagecache_macros.h"
#endif


// section: threadcontext_t

// group: variables

#ifdef KONFIG_UNITTEST
/* variable: s_threadcontext_errtimer
 * Simulates an error in <init_thread_resources>. */
static test_errortimer_t   s_threadcontext_errtimer = test_errortimer_INIT_FREEABLE ;
#endif

// group: lifetime

int free_threadcontext(threadcontext_t * tcontext)
{
   int err = 0 ;
   int err2 ;

   uint16_t  initcount = tcontext->initcount ;
   tcontext->initcount = 0 ;

   switch (initcount) {
   default:    assert(false && "initcount out of bounds") ;
               break ;
// TEXTDB:SELECT("   case "row-id":     err2 = freethread_"module"("(if (parameter!="") "&tcontext->" else "")parameter") ;"\n"               if (err2) err = err2 ;")FROM(C-kern/resource/config/initthread)DESCENDING
   case 4:     err2 = freethread_logwriter(&tcontext->log) ;
               if (err2) err = err2 ;
   case 3:     err2 = freethread_objectcacheimpl(&tcontext->objectcache) ;
               if (err2) err = err2 ;
   case 2:     err2 = freethread_pagecacheimpl(&tcontext->pgcache) ;
               if (err2) err = err2 ;
   case 1:     err2 = freethread_mmtransient(&tcontext->mm_transient) ;
               if (err2) err = err2 ;
// TEXTDB:END
   case 0:     break ;
   }

   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}

int init_threadcontext(/*out*/threadcontext_t * tcontext)
{
   int err ;

   *tcontext = (threadcontext_t) threadcontext_INIT_STATIC ;

   VALIDATE_STATE_TEST(maincontext_STATIC != type_maincontext(), ONABORT, ) ;

// TEXTDB:SELECT(\n"   ONERROR_testerrortimer(&s_threadcontext_errtimer, ONABORT) ;"\n"   err = initthread_"module"("(if (parameter!="") "&tcontext->")parameter") ;"\n"   if (err) goto ONABORT ;"\n"   ++tcontext->initcount ;")FROM(C-kern/resource/config/initthread)

   ONERROR_testerrortimer(&s_threadcontext_errtimer, ONABORT) ;
   err = initthread_mmtransient(&tcontext->mm_transient) ;
   if (err) goto ONABORT ;
   ++tcontext->initcount ;

   ONERROR_testerrortimer(&s_threadcontext_errtimer, ONABORT) ;
   err = initthread_pagecacheimpl(&tcontext->pgcache) ;
   if (err) goto ONABORT ;
   ++tcontext->initcount ;

   ONERROR_testerrortimer(&s_threadcontext_errtimer, ONABORT) ;
   err = initthread_objectcacheimpl(&tcontext->objectcache) ;
   if (err) goto ONABORT ;
   ++tcontext->initcount ;

   ONERROR_testerrortimer(&s_threadcontext_errtimer, ONABORT) ;
   err = initthread_logwriter(&tcontext->log) ;
   if (err) goto ONABORT ;
   ++tcontext->initcount ;
// TEXTDB:END

   ONERROR_testerrortimer(&s_threadcontext_errtimer, ONABORT) ;

   return 0 ;
ONABORT:
   (void) free_threadcontext(tcontext) ;
   TRACEABORT_LOG(err) ;
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   threadcontext_t   tcontext   = threadcontext_INIT_STATIC ;
   const int         nrsvc      = 4 ;
   size_t            sizestatic ;

   // TEST threadcontext_INIT_STATIC
   TEST(0 == tcontext.initcount) ;
   TEST(0 == tcontext.pgcache.object) ;
   TEST(0 == tcontext.pgcache.iimpl) ;
   TEST(0 == tcontext.mm_transient.object) ;
   TEST(0 == tcontext.mm_transient.iimpl) ;
   TEST(0 == tcontext.objectcache.object) ;
   TEST(0 == tcontext.objectcache.iimpl) ;
   TEST(&g_logmain           == tcontext.log.object) ;
   TEST(&g_logmain_interface == tcontext.log.iimpl) ;

   // TEST init_threadcontext, free_threadcontext
   sizestatic = SIZESTATIC_PAGECACHE() ;
   TEST(0 == init_threadcontext(&tcontext)) ;
   TEST(nrsvc == tcontext.initcount) ;
   TEST(0 != tcontext.pgcache.object) ;
   TEST(0 != tcontext.pgcache.iimpl) ;
   TEST(0 != tcontext.mm_transient.object) ;
   TEST(0 != tcontext.mm_transient.iimpl) ;
   TEST(0 != tcontext.objectcache.object) ;
   TEST(0 != tcontext.objectcache.iimpl) ;
   TEST(0 != tcontext.log.object) ;
   TEST(0 != tcontext.log.iimpl) ;
   TEST(&g_logmain           != tcontext.log.object) ;
   TEST(&g_logmain_interface != tcontext.log.iimpl) ;
   TEST(SIZESTATIC_PAGECACHE() > sizestatic) ;
   TEST(0 == free_threadcontext(&tcontext)) ;
   TEST(0 == tcontext.initcount) ;
   TEST(0 == tcontext.pgcache.object) ;
   TEST(0 == tcontext.pgcache.iimpl) ;
   TEST(0 == tcontext.mm_transient.object) ;
   TEST(0 == tcontext.mm_transient.iimpl) ;
   TEST(0 == tcontext.objectcache.object) ;
   TEST(0 == tcontext.objectcache.iimpl) ;
   TEST(tcontext.log.object == &g_logmain) ;
   TEST(tcontext.log.iimpl  == &g_logmain_interface) ;
   TEST(SIZESTATIC_PAGECACHE() == sizestatic) ;
   TEST(0 == free_threadcontext(&tcontext)) ;
   TEST(0 == tcontext.initcount) ;
   TEST(0 == tcontext.pgcache.object) ;
   TEST(0 == tcontext.pgcache.iimpl) ;
   TEST(0 == tcontext.mm_transient.object) ;
   TEST(0 == tcontext.mm_transient.iimpl) ;
   TEST(0 == tcontext.objectcache.object) ;
   TEST(0 == tcontext.objectcache.iimpl) ;
   TEST(&g_logmain           == tcontext.log.object) ;
   TEST(&g_logmain_interface == tcontext.log.iimpl) ;
   TEST(SIZESTATIC_PAGECACHE() == sizestatic) ;

   // TEST init_threadcontext: EINVAL
   for(int i = 0; i <= nrsvc; ++i) {
      init_testerrortimer(&s_threadcontext_errtimer, 1u+(unsigned)i, EINVAL+i) ;
      memset(&tcontext, 0xff, sizeof(tcontext)) ;
      TEST(EINVAL+i == init_threadcontext(&tcontext)) ;
      TEST(0 == tcontext.initcount) ;
      TEST(0 == tcontext.pgcache.object) ;
      TEST(0 == tcontext.pgcache.iimpl) ;
      TEST(0 == tcontext.mm_transient.object) ;
      TEST(0 == tcontext.mm_transient.iimpl) ;
      TEST(0 == tcontext.objectcache.object) ;
      TEST(0 == tcontext.objectcache.iimpl) ;
      TEST(&g_logmain           == tcontext.log.object) ;
      TEST(&g_logmain_interface == tcontext.log.iimpl) ;
      TEST(SIZESTATIC_PAGECACHE() == sizestatic) ;
   }

   return 0 ;
ONABORT:
   s_threadcontext_errtimer = (test_errortimer_t) test_errortimer_INIT_FREEABLE ;
   return EINVAL ;
}

int unittest_context_threadcontext()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

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
