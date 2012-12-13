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
// TEXTDB:SELECT('#include "'header-name'"')FROM("C-kern/resource/config/initprocess")
#include "C-kern/api/platform/locale.h"
#include "C-kern/api/platform/sysuser.h"
#include "C-kern/api/platform/sync/signal.h"
#include "C-kern/api/cache/valuecache.h"
#include "C-kern/api/platform/thread.h"
#include "C-kern/api/presentation/X11/x11.h"
// TEXTDB:END
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


// section: processcontext_t

// group: lifetime

int init_processcontext(/*out*/processcontext_t * pcontext)
{
   int err ;

   pcontext->initcount = 0 ;

// TEXTDB:SELECT(\n"   err = initonce_"module"("(if (parameter!="") "&pcontext->" else "")parameter") ;"\n"   if (err) goto ONABORT ;"\n"   ++ pcontext->initcount ;")FROM("C-kern/resource/config/initprocess")

   err = initonce_locale() ;
   if (err) goto ONABORT ;
   ++ pcontext->initcount ;

   err = initonce_sysuser(&pcontext->sysuser) ;
   if (err) goto ONABORT ;
   ++ pcontext->initcount ;

   err = initonce_signalconfig() ;
   if (err) goto ONABORT ;
   ++ pcontext->initcount ;

   err = initonce_valuecache(&pcontext->valuecache) ;
   if (err) goto ONABORT ;
   ++ pcontext->initcount ;

   err = initonce_thread() ;
   if (err) goto ONABORT ;
   ++ pcontext->initcount ;

   err = initonce_X11() ;
   if (err) goto ONABORT ;
   ++ pcontext->initcount ;
// TEXTDB:END

   return 0 ;
ONABORT:
   (void) free_processcontext(pcontext) ;
   TRACEABORT_LOG(err) ;
   return err ;
}

int free_processcontext(processcontext_t * pcontext)
{
   int err = 0 ;
   int err2 ;

   uint16_t  initcount = pcontext->initcount ;
   pcontext->initcount = 0 ;

   switch(initcount) {
   default: assert(0 != pcontext->initcount  && "out of bounds" )  ;
            break ;
// TEXTDB:SELECT(\n"   case "row-id":  err2 = freeonce_"module"("(if (parameter!="") "&pcontext->" else "")parameter") ;"\n"            if (err2) err = err2 ;")FROM("C-kern/resource/config/initprocess")DESCENDING

   case 6:  err2 = freeonce_X11() ;
            if (err2) err = err2 ;

   case 5:  err2 = freeonce_thread() ;
            if (err2) err = err2 ;

   case 4:  err2 = freeonce_valuecache(&pcontext->valuecache) ;
            if (err2) err = err2 ;

   case 3:  err2 = freeonce_signalconfig() ;
            if (err2) err = err2 ;

   case 2:  err2 = freeonce_sysuser(&pcontext->sysuser) ;
            if (err2) err = err2 ;

   case 1:  err2 = freeonce_locale() ;
            if (err2) err = err2 ;
// TEXTDB:END
   case 0:  break ;
   }

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
   processcontext_t  pcontext = processcontext_INIT_FREEABLE ;
   sysusercontext_t  emptyusr = sysusercontext_INIT_FREEABLE ;

   // TEST processcontext_INIT_FREEABLE
   TEST(0 == pcontext.valuecache) ;
   TEST(1 == isequal_sysusercontext(&pcontext.sysuser, &emptyusr)) ;
   TEST(0 == pcontext.initcount) ;

   // TEST free_processcontext
   pcontext.initcount = process_maincontext().initcount ;
   TEST(0 == free_processcontext(&pcontext)) ;
   TEST(0 == pcontext.initcount) ;
   TEST(1 == isequal_sysusercontext(&pcontext.sysuser, &emptyusr)) ;
   TEST(0 == pcontext.valuecache) ;
   TEST(0 == free_processcontext(&pcontext)) ;
   TEST(0 == pcontext.initcount) ;
   TEST(1 == isequal_sysusercontext(&pcontext.sysuser, &emptyusr)) ;
   TEST(0 == pcontext.valuecache) ;

   // free_processcontext does nothing in case init_count == 0 ;
   pcontext.initcount  = 0 ;
   pcontext.sysuser    = (sysusercontext_t) sysusercontext_INIT(process_maincontext().sysuser.realuser, process_maincontext().sysuser.privilegeduser) ;
   pcontext.valuecache = (struct valuecache_t*) 1 ;
   TEST(0 == free_processcontext(&pcontext)) ;
   TEST(pcontext.valuecache == (struct valuecache_t*) 1) ;
   TEST(1 == isequal_sysusercontext(&pcontext.sysuser, &process_maincontext().sysuser)) ;
   pcontext.valuecache = 0 ;

   // TEST EINVAL
   pcontext.valuecache = (struct valuecache_t*) 2 ;
   TEST(EINVAL == init_processcontext(&pcontext)) ;
   TEST(pcontext.initcount  == 0) ;
   TEST(pcontext.valuecache == (struct valuecache_t*) 2) ;
   pcontext.valuecache = 0 ;

   // TEST init_processcontext
   pcontext = (processcontext_t) processcontext_INIT_FREEABLE ;
   TEST(0 == init_processcontext(&pcontext)) ;
   TEST(0 != pcontext.valuecache) ;
   TEST(1 == isequal_sysusercontext(&pcontext.sysuser, &process_maincontext().sysuser)) ;
   TEST(6 == pcontext.initcount) ;
   freeonce_valuecache(&pcontext.valuecache) ;
   TEST(0 == pcontext.valuecache) ;

   return 0 ;
ONABORT:
   (void) free_processcontext(&pcontext) ;
   return EINVAL ;
}

int unittest_context_processcontext()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   if (test_initfree())            goto ONABORT ;
   CLEARBUFFER_LOG() ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())            goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   // make printed system error messages language (English) neutral
   resetmsg_locale() ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
