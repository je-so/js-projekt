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
// TEXTDB:SELECT('#include "'header-name'"')FROM("C-kern/resource/text.db/initprocess")
#include "C-kern/api/platform/locale.h"
#include "C-kern/api/presentation/X11/x11.h"
#include "C-kern/api/platform/sync/signal.h"
#include "C-kern/api/cache/valuecache.h"
#include "C-kern/api/platform/thread.h"
// TEXTDB:END
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


// section: processcontext_t

// group: lifetime

int init_processcontext(processcontext_t * pcontext)
{
   int err ;

   pcontext->initcount = 0 ;

// TEXTDB:SELECT(\n"   err = initonce_"module"("(if (parameter!="") "&pcontext->" else "")parameter") ;"\n"   if (err) goto ABBRUCH ;"\n"   ++ pcontext->initcount ;")FROM("C-kern/resource/text.db/initprocess")

   err = initonce_locale() ;
   if (err) goto ABBRUCH ;
   ++ pcontext->initcount ;

   err = initonce_X11() ;
   if (err) goto ABBRUCH ;
   ++ pcontext->initcount ;

   err = initonce_signalconfig() ;
   if (err) goto ABBRUCH ;
   ++ pcontext->initcount ;

   err = initonce_valuecache(&pcontext->valuecache) ;
   if (err) goto ABBRUCH ;
   ++ pcontext->initcount ;

   err = initonce_thread() ;
   if (err) goto ABBRUCH ;
   ++ pcontext->initcount ;
// TEXTDB:END

   return 0 ;
ABBRUCH:
   (void) free_processcontext(pcontext) ;
   LOG_ABORT(err) ;
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
// TEXTDB:SELECT(\n"   case "row-id":  err2 = freeonce_"module"("(if (parameter!="") "&pcontext->" else "")parameter") ;"\n"            if (err2) err = err2 ;")FROM("C-kern/resource/text.db/initprocess")DESCENDING

   case 5:  err2 = freeonce_thread() ;
            if (err2) err = err2 ;

   case 4:  err2 = freeonce_valuecache(&pcontext->valuecache) ;
            if (err2) err = err2 ;

   case 3:  err2 = freeonce_signalconfig() ;
            if (err2) err = err2 ;

   case 2:  err2 = freeonce_X11() ;
            if (err2) err = err2 ;

   case 1:  err2 = freeonce_locale() ;
            if (err2) err = err2 ;
// TEXTDB:END
   case 0:  break ;
   }

   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT_FREE(err) ;
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   processcontext_t  pcontext = processcontext_INIT_FREEABLE ;

   // TEST static init
   TEST(0 == pcontext.initcount) ;
   TEST(0 == pcontext.valuecache) ;

   // TEST double free
   pcontext.initcount = process_maincontext().initcount ;
   TEST(0 == free_processcontext(&pcontext)) ;
   TEST(0 == pcontext.initcount) ;
   TEST(0 == pcontext.valuecache) ;
   TEST(0 == free_processcontext(&pcontext)) ;
   TEST(0 == pcontext.initcount) ;
   TEST(0 == pcontext.valuecache) ;

   // free_processcontext does nothing in case init_count == 0 ;
   pcontext.initcount  = 0 ;
   pcontext.valuecache = (struct valuecache_t*) 1 ;
   TEST(0 == free_processcontext(&pcontext)) ;
   TEST(pcontext.valuecache == (struct valuecache_t*) 1) ;
   pcontext.valuecache = 0 ;

   // TEST EINVAL
   pcontext.valuecache = (struct valuecache_t*) 2 ;
   TEST(EINVAL == init_processcontext(&pcontext)) ;
   TEST(pcontext.initcount  == 0) ;
   TEST(pcontext.valuecache == (struct valuecache_t*) 2) ;
   pcontext.valuecache = 0 ;

   // TEST init
   TEST(0 == init_processcontext(&pcontext)) ;
   TEST(0 != pcontext.valuecache) ;
   TEST(5 == pcontext.initcount) ;
   freeonce_valuecache(&pcontext.valuecache) ;
   TEST(0 == pcontext.valuecache) ;

   return 0 ;
ABBRUCH:
   (void) free_processcontext(&pcontext) ;
   return EINVAL ;
}

int unittest_context_processcontext()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   if (test_initfree())            goto ABBRUCH ;
   LOG_CLEARBUFFER() ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())            goto ABBRUCH ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   // make printed system error messages language (English) neutral
   resetmsg_locale() ;

   return 0 ;
ABBRUCH:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
