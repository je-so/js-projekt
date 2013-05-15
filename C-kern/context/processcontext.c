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

// group: constants

/* define: processcontext_STATICSIZE
 * Defines size for <s_processcontext_staticmem>.
 * This extrasize is needed during unit tests. */
#define processcontext_STATICSIZE         (sizeof(valuecache_t) + sizeof(sysuser_t))

#ifdef KONFIG_UNITTEST
/* define: processcontext_STATICTESTSIZE
 * Defines additonal size for <s_processcontext_staticmem>.
 * This extrasize is needed during unit tests. */
#define processcontext_STATICTESTSIZE     (sizeof(valuecache_t) + sizeof(sysuser_t))
#else
#define processcontext_STATICTESTSIZE     0
#endif

// group: variables

/* variable: s_processcontext_staticmem
 * Static memory block used in in global <processcontext_t>. */
static uint8_t             s_processcontext_staticmem[1 + processcontext_STATICSIZE + processcontext_STATICTESTSIZE] = { 0 } ;

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

   // init static memory service
   pcontext->staticmem.size = sizeof(s_processcontext_staticmem)-1 ;
   pcontext->staticmem.used = 0 ;
   pcontext->initcount      = 0 ;
   if (s_processcontext_staticmem[0]) {
      pcontext->staticmem.addr = malloc(pcontext->staticmem.size) ;
      if (! pcontext->staticmem.addr) return ENOMEM ;
   } else {
      s_processcontext_staticmem[0] = 255 ;
      pcontext->staticmem.addr = &s_processcontext_staticmem[1] ;
   }

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

   if (pcontext->staticmem.used) {
      err = ENOTEMPTY ;
   }

   if (pcontext->staticmem.addr) {
      if (pcontext->staticmem.addr == &s_processcontext_staticmem[1]) {
         s_processcontext_staticmem[0] = 0 ;
      } else {
         free(pcontext->staticmem.addr) ;
      }
      pcontext->staticmem.addr = 0 ;
   }
   pcontext->staticmem.size = 0 ;
   pcontext->staticmem.used = 0 ;
   pcontext->initcount      = 0 ;

   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}

// group: static-memory

void * allocstatic_processcontext(processcontext_t * pcontext, uint8_t size)
{
   int err ;

   if (pcontext->staticmem.size - pcontext->staticmem.used < size) {
      TRACEOUTOFMEM_LOG(size) ;
      err = ENOMEM ;
      goto ONABORT ;
   }

   uint8_t * addr = pcontext->staticmem.addr + pcontext->staticmem.used ;
   pcontext->staticmem.used = (uint16_t)(pcontext->staticmem.used + size) ;

   return addr ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return 0 ;
}

int freestatic_processcontext(processcontext_t * pcontext, uint8_t size)
{
   int err ;

   VALIDATE_INPARAM_TEST(pcontext->staticmem.used >= size, ONABORT,) ;

   pcontext->staticmem.used = (uint16_t)(pcontext->staticmem.used - size) ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
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
   TEST(0 == pcontext.staticmem.addr) ;
   TEST(0 == pcontext.staticmem.size) ;
   TEST(0 == pcontext.staticmem.used) ;
   TEST(0 == pcontext.initcount) ;

   // TEST init_processcontext: s_processcontext_staticmem in use
   TEST(s_processcontext_staticmem[0] != 0) ;
   TEST(process_maincontext().staticmem.addr == &s_processcontext_staticmem[1]) ;
   TEST(process_maincontext().staticmem.size == sizeof(s_processcontext_staticmem)-1) ;
   TEST(process_maincontext().staticmem.used == processcontext_STATICSIZE) ;

   // TEST init_processcontext
   TEST(0 == init_processcontext(&pcontext)) ;
   TEST(0 != pcontext.valuecache) ;
   TEST(0 != pcontext.sysuser) ;
   TEST(0 != pcontext.error.stroffset) ;
   TEST(0 != pcontext.error.strdata) ;
   TEST(0 != pcontext.staticmem.addr) ;
   TEST(pcontext.staticmem.addr != &s_processcontext_staticmem[1]) ;
   TEST(pcontext.staticmem.size == sizeof(s_processcontext_staticmem)-1) ;
   TEST(0 == pcontext.staticmem.used) ;
   TEST(I == pcontext.initcount) ;
   TEST(process_maincontext().staticmem.used == 2*processcontext_STATICSIZE) ;

   // TEST free_processcontext
   TEST(0 == free_processcontext(&pcontext)) ;
   TEST(0 == pcontext.valuecache) ;
   TEST(0 == pcontext.sysuser) ;
   TEST(0 != pcontext.error.stroffset) ;
   TEST(0 != pcontext.error.strdata) ;
   TEST(0 == pcontext.staticmem.addr) ;
   TEST(0 == pcontext.staticmem.size) ;
   TEST(0 == pcontext.staticmem.used) ;
   TEST(0 == pcontext.initcount) ;
   TEST(process_maincontext().staticmem.used == processcontext_STATICSIZE) ;

   // TEST free_processcontext: initcount == 0
   pcontext.initcount = 0 ;
   pcontext.valuecache = (struct valuecache_t*) 1 ;
   pcontext.sysuser    = (struct sysuser_t*) 2 ;
   pcontext.error.stroffset = (uint16_t*)3 ;
   pcontext.error.strdata   = (uint8_t*)4 ;
   pcontext.staticmem.addr = malloc(100) ;
   pcontext.staticmem.size = 100 ;
   pcontext.staticmem.used = 0 ;
   TEST(0 == free_processcontext(&pcontext)) ;
   TEST(pcontext.valuecache == (struct valuecache_t*) 1) ;
   TEST(pcontext.sysuser    == (struct sysuser_t*) 2) ;
   TEST(pcontext.error.stroffset == (uint16_t*)3) ;
   TEST(pcontext.error.strdata   == (uint8_t*)4) ;
   // staticmem is freed
   TEST(0 == pcontext.staticmem.addr) ;
   TEST(0 == pcontext.staticmem.size) ;
   TEST(0 == pcontext.staticmem.used) ;
   TEST(process_maincontext().staticmem.used == processcontext_STATICSIZE) ;

   // TEST free_processcontext: ENOTEMPTY
   pcontext.initcount      = 0 ;
   pcontext.staticmem.addr = malloc(100) ;
   pcontext.staticmem.size = 100 ;
   pcontext.staticmem.used = 1 ;
   TEST(ENOTEMPTY == free_processcontext(&pcontext)) ;
   TEST(0 == pcontext.staticmem.addr) ;
   TEST(0 == pcontext.staticmem.size) ;
   TEST(0 == pcontext.staticmem.used) ;
   TEST(process_maincontext().staticmem.used == processcontext_STATICSIZE) ;

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
      TEST(0 == pcontext.staticmem.addr) ;
      TEST(0 == pcontext.staticmem.size) ;
      TEST(0 == pcontext.staticmem.used) ;
      TEST(0 == pcontext.initcount) ;
      TEST(process_maincontext().staticmem.used == processcontext_STATICSIZE) ;
   }
   init_testerrortimer(&s_processcontext_errtimer, 0, 0) ;

   // TEST init_processcontext: restore default environment
   pcontext = (processcontext_t) processcontext_INIT_STATIC ;
   TEST(0 == init_processcontext(&pcontext)) ;
   TEST(I == pcontext.initcount) ;
   TEST(process_maincontext().staticmem.used == 2*processcontext_STATICSIZE) ;
   freeonce_valuecache(&pcontext.valuecache) ;
   freeonce_sysuser(&pcontext.sysuser) ;
   // restore (if setuid)
   switchtoreal_sysuser(sysuser_maincontext()) ;
   TEST(process_maincontext().staticmem.used == processcontext_STATICSIZE) ;
   free(pcontext.staticmem.addr) ;
   pcontext.staticmem.addr = 0 ;

   return 0 ;
ONABORT:
   (void) free_processcontext(&pcontext) ;
   return EINVAL ;
}

static int test_staticmem(void)
{
   processcontext_t  pcontext = processcontext_INIT_STATIC ;
   const uint16_t    memsize  = 1024;
   uint8_t  *        mem      = malloc(memsize) ;

   // prepare
   TEST(mem != 0) ;
   pcontext.staticmem.addr = mem ;
   pcontext.staticmem.size = memsize ;

   // TEST allocstatic_processcontext: all block sizes
   for (unsigned i = 0; i <= 255; ++i) {
      TEST(mem == allocstatic_processcontext(&pcontext, (uint8_t)i)) ;
      TEST(i   == pcontext.staticmem.used) ;
      pcontext.staticmem.used = 0 ;
   }

   // TEST freestatic_processcontext: all block sizes
   for (unsigned i = 0; i <= 255; ++i) {
      pcontext.staticmem.used = 255 ;
      TEST(0 == freestatic_processcontext(&pcontext, (uint8_t)i)) ;
      TEST(pcontext.staticmem.used == 255-i) ;
   }

   // TEST allocstatic_processcontext: all bytes
   for (unsigned i = 0; i < memsize; ++i) {
      TEST(mem+i == allocstatic_processcontext(&pcontext, 1)) ;
      TEST(i+1   == pcontext.staticmem.used) ;
   }

   // TEST allocstatic_processcontext: ENOMEM
   TEST(mem+memsize == allocstatic_processcontext(&pcontext, 0)) ;
   TEST(0 == allocstatic_processcontext(&pcontext, 1)) ;

   // TEST freestatic_processcontext: all bytes
   for (unsigned i = memsize; i > 0; --i) {
      TEST(0 == freestatic_processcontext(&pcontext, 1)) ;
      TEST(pcontext.staticmem.used == i-1) ;
   }

   // TEST freestatic_processcontext: EINVAL
   TEST(EINVAL == freestatic_processcontext(&pcontext, 1)) ;
   TEST(pcontext.staticmem.addr == mem) ;
   TEST(pcontext.staticmem.size == memsize) ;
   TEST(pcontext.staticmem.used == 0) ;

   // TEST sizestatic_processcontext
   for (uint16_t i = 1; i; i = (uint16_t)(i << 1)) {
      pcontext.staticmem.used = i ;
      TEST(i == sizestatic_processcontext(&pcontext)) ;
   }
   pcontext.staticmem.used = 0 ;
   TEST(0 == sizestatic_processcontext(&pcontext)) ;

   // unprepare
   free(pcontext.staticmem.addr) ;
   pcontext.staticmem.addr = 0 ;

   return 0 ;
ONABORT:
   free(pcontext.staticmem.addr) ;
   return EINVAL ;
}

int unittest_context_processcontext()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   if (test_initfree())          goto ONABORT ;
   CLEARBUFFER_LOG() ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())          goto ONABORT ;
   if (test_staticmem())         goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
