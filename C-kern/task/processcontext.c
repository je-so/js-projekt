/* title: ProcessContext impl
   Implements the global context of the running system process.
   See <ProcessContext>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2012 Jörg Seebohn

   file: C-kern/api/task/processcontext.h
    Header file <ProcessContext>.

   file: C-kern/task/processcontext.c
    Implementation file <ProcessContext impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/task/processcontext.h"
#include "C-kern/api/err.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/test/errortimer.h"
// TEXTDB:SELECT('#include "'header-name'"')FROM(C-kern/resource/config/initprocess)
#include "C-kern/api/stdtypes/errorcontext.h"
#include "C-kern/api/platform/locale.h"
#include "C-kern/api/platform/sync/signal.h"
#include "C-kern/api/cache/valuecache.h"
#include "C-kern/api/platform/syslogin.h"
#include "C-kern/api/memory/pagecache_impl.h"
#include "C-kern/api/platform/X11/x11.h"
// TEXTDB:END
#include "C-kern/api/platform/task/thread_tls.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#endif


// section: processcontext_t

// group: static variables

#ifdef KONFIG_UNITTEST
/* variable: s_processcontext_errtimer
 * Simulates an error in <init_processcontext>. */
static test_errortimer_t   s_processcontext_errtimer = test_errortimer_FREE;
#endif

// group: helper

// group: lifetime-helper

static inline uint16_t static_memory_size(void)
{
   uint16_t memorysize = 0
// TEXTDB:SELECT("         + sizeof("objtype")")FROM(C-kern/resource/config/initprocess)WHERE(inittype=="object")
         + sizeof(valuecache_t)
         + sizeof(syslogin_t)
         + sizeof(pagecache_blockmap_t)
// TEXTDB:END
     ;
   return memorysize;
}

/* function: alloc_static_memory
 * Allokiert statischen Speicher für alle noch für <processcontext_t> zu initialisierten Objekte.
 * */
static inline int alloc_static_memory(processcontext_t* pcontext, thread_tls_t* tls, /*out*/memblock_t* mblock)
{
   int err;
   const uint16_t size = static_memory_size();

   ONERROR_testerrortimer(&s_processcontext_errtimer, &err, ONERR);
   err = allocstatic_threadtls(tls, size, mblock);
   if (err) goto ONERR;

   pcontext->staticmemblock = mblock->addr;

   return 0;
ONERR:
   return err;
}

/* function: free_static_memory
 * Gibt statischen Speicher für alle im <processcontext_t> initialisierten Objekt frei.
 * */
static inline int free_static_memory(processcontext_t* pcontext, thread_tls_t* tls)
{
   int err;
   const uint16_t size = static_memory_size();

   if (pcontext->staticmemblock) {
      memblock_t mblock = memblock_INIT(size, pcontext->staticmemblock);

      pcontext->staticmemblock = 0;

      err = freestatic_threadtls(tls, &mblock);
      SETONERROR_testerrortimer(&s_processcontext_errtimer, &err);

      if (err) goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

// group: lifetime

int init_processcontext(/*out*/processcontext_t * pcontext)
{
   int err;

   pcontext->initcount = 0;

   memblock_t mblock;
   err = alloc_static_memory(pcontext, self_threadtls(), &mblock);
   if (err) goto ONERR;
   ++ pcontext->initcount;

// TEXTDB:SELECT(\n"   ONERROR_testerrortimer(&s_processcontext_errtimer, &err, ONERR);"\n(if (inittype=='initonce') ("   err = initonce_"module"("(if (parameter!="") ("&pcontext->"parameter))");"\n"   if (err) goto ONERR;")) (if (inittype=='object') ("   assert( sizeof("objtype") <= mblock.size);"\n"   err = init_"module"(("objtype"*) mblock.addr);"\n"   if (err) goto ONERR;"\n"   pcontext->"parameter" = ("objtype"*) mblock.addr;"\n"   mblock.addr += sizeof("objtype");"\n"   mblock.size -= sizeof("objtype");")) \n"   ++ pcontext->initcount;")FROM(C-kern/resource/config/initprocess)

   ONERROR_testerrortimer(&s_processcontext_errtimer, &err, ONERR);
   err = initonce_errorcontext(&pcontext->error);
   if (err) goto ONERR;
   ++ pcontext->initcount;

   ONERROR_testerrortimer(&s_processcontext_errtimer, &err, ONERR);
   err = initonce_locale();
   if (err) goto ONERR;
   ++ pcontext->initcount;

   ONERROR_testerrortimer(&s_processcontext_errtimer, &err, ONERR);
   err = initonce_signalhandler();
   if (err) goto ONERR;
   ++ pcontext->initcount;

   ONERROR_testerrortimer(&s_processcontext_errtimer, &err, ONERR);
   assert( sizeof(valuecache_t) <= mblock.size);
   err = init_valuecache((valuecache_t*) mblock.addr);
   if (err) goto ONERR;
   pcontext->valuecache = (valuecache_t*) mblock.addr;
   mblock.addr += sizeof(valuecache_t);
   mblock.size -= sizeof(valuecache_t);
   ++ pcontext->initcount;

   ONERROR_testerrortimer(&s_processcontext_errtimer, &err, ONERR);
   assert( sizeof(syslogin_t) <= mblock.size);
   err = init_syslogin((syslogin_t*) mblock.addr);
   if (err) goto ONERR;
   pcontext->syslogin = (syslogin_t*) mblock.addr;
   mblock.addr += sizeof(syslogin_t);
   mblock.size -= sizeof(syslogin_t);
   ++ pcontext->initcount;

   ONERROR_testerrortimer(&s_processcontext_errtimer, &err, ONERR);
   assert( sizeof(pagecache_blockmap_t) <= mblock.size);
   err = init_pagecacheblockmap((pagecache_blockmap_t*) mblock.addr);
   if (err) goto ONERR;
   pcontext->blockmap = (pagecache_blockmap_t*) mblock.addr;
   mblock.addr += sizeof(pagecache_blockmap_t);
   mblock.size -= sizeof(pagecache_blockmap_t);
   ++ pcontext->initcount;

   ONERROR_testerrortimer(&s_processcontext_errtimer, &err, ONERR);
   err = initonce_X11();
   if (err) goto ONERR;
   ++ pcontext->initcount;
// TEXTDB:END

   ONERROR_testerrortimer(&s_processcontext_errtimer, &err, ONERR);

   return 0;
ONERR:
   (void) free_processcontext(pcontext);
   TRACEEXIT_ERRLOG(err);
   return err;
}

int free_processcontext(processcontext_t* pcontext)
{
   int err;
   int err2;

   err = 0;

   switch (pcontext->initcount) {
   default: assert(false && "initcount out of bounds");
            break;
// TEXTDB:SELECT(\n"   case ("row-id"+1):"\n(if (inittype=='initonce') ("            err2 = freeonce_"module"("(if (parameter!="") ("&pcontext->"parameter))");"\n"            SETONERROR_testerrortimer(&s_processcontext_errtimer, &err2);"\n"            if (err2) err = err2;")) (if (inittype=='object') ("            err2 = free_"module"( pcontext->"parameter");"\n"            SETONERROR_testerrortimer(&s_processcontext_errtimer, &err2);"\n"            if (err2) err = err2;"\n"            pcontext->"parameter" = 0;")) )FROM(C-kern/resource/config/initprocess)DESCENDING

   case (7+1):
            err2 = freeonce_X11();
            SETONERROR_testerrortimer(&s_processcontext_errtimer, &err2);
            if (err2) err = err2;

   case (6+1):
            err2 = free_pagecacheblockmap( pcontext->blockmap);
            SETONERROR_testerrortimer(&s_processcontext_errtimer, &err2);
            if (err2) err = err2;
            pcontext->blockmap = 0;

   case (5+1):
            err2 = free_syslogin( pcontext->syslogin);
            SETONERROR_testerrortimer(&s_processcontext_errtimer, &err2);
            if (err2) err = err2;
            pcontext->syslogin = 0;

   case (4+1):
            err2 = free_valuecache( pcontext->valuecache);
            SETONERROR_testerrortimer(&s_processcontext_errtimer, &err2);
            if (err2) err = err2;
            pcontext->valuecache = 0;

   case (3+1):
            err2 = freeonce_signalhandler();
            SETONERROR_testerrortimer(&s_processcontext_errtimer, &err2);
            if (err2) err = err2;

   case (2+1):
            err2 = freeonce_locale();
            SETONERROR_testerrortimer(&s_processcontext_errtimer, &err2);
            if (err2) err = err2;

   case (1+1):
            err2 = freeonce_errorcontext(&pcontext->error);
            SETONERROR_testerrortimer(&s_processcontext_errtimer, &err2);
            if (err2) err = err2;
// TEXTDB:END
   case 1:  err2 = free_static_memory(pcontext, self_threadtls());
            SETONERROR_testerrortimer(&s_processcontext_errtimer, &err2);
            if (err2) err = err2;
   case 0:  break;
   }

   pcontext->initcount = 0;

   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

// group: query

bool isstatic_processcontext(const processcontext_t * pcontext)
{
   return   0 == pcontext->valuecache
            && 0 == pcontext->syslogin
            && g_errorcontext_stroffset == pcontext->error.stroffset && g_errorcontext_strdata == pcontext->error.strdata
            && 0 == pcontext->blockmap
            && 0 == pcontext->staticmemblock
            && 0 == pcontext->initcount;
}

size_t extsize_processcontext(void)
{
   return static_memory_size();
}

// group: test

#ifdef KONFIG_UNITTEST

static int test_helper(void)
{
   processcontext_t pcontext = processcontext_INIT_STATIC;
   thread_tls_t *   tls      = 0;
   const size_t     S        = static_memory_size();
   memblock_t       mblock   = memblock_FREE;

   // prepare0
   TEST(0 == new_threadtls(&tls, 0, 0));

   // TEST static_memory_size
   TEST(S == static_memory_size());
   TEST(S != 0);
   TEST(S <  1024);
   TEST(0 == S % sizeof(size_t));

   // TEST alloc_static_memory
   TEST(0 == alloc_static_memory(&pcontext, tls, &mblock))
   // check params
   TEST(mblock.addr == pcontext.staticmemblock);
   TEST(mblock.size == sizestatic_threadtls(tls));
   TEST(mblock.addr >  (uint8_t*) tls);
   TEST(mblock.size == S);

   // TEST free_static_memory
   TEST(0 == free_static_memory(&pcontext, tls))
   // check params
   TEST(0 == pcontext.staticmemblock);
   TEST(0 == sizestatic_threadtls(tls));

   // TEST free_static_memory: already freed
   TEST(0 == free_static_memory(&pcontext, tls))
   // check params
   TEST(0 == pcontext.staticmemblock);
   TEST(0 == sizestatic_threadtls(tls));

   // TEST alloc_static_memory: simulated ERROR
   mblock = (memblock_t) memblock_FREE;
   init_testerrortimer(&s_processcontext_errtimer, 1, 4);
   TEST(4 == alloc_static_memory(&pcontext, tls, &mblock))
   // check params
   TEST(0 == pcontext.staticmemblock);
   TEST(0 == sizestatic_threadtls(tls));
   TEST( isfree_memblock(&mblock));

   // TEST free_static_memory: simulated ERROR
   TEST(0 == alloc_static_memory(&pcontext, tls, &mblock))
   init_testerrortimer(&s_processcontext_errtimer, 1, 4);
   TEST(4 == free_static_memory(&pcontext, tls))
   // check params
   TEST(0 == pcontext.staticmemblock);
   TEST(0 == sizestatic_threadtls(tls));

   // prepare0
   TEST(0 == delete_threadtls(&tls));

   return 0;
ONERR:
   delete_threadtls(&tls);
   return EINVAL;
}

static int test_initfree(void)
{
   processcontext_t  pcontext = processcontext_INIT_STATIC;
   const uint16_t    I        = 8;
   const size_t      S        = sizestatic_threadtls(self_threadtls());

   // TEST processcontext_INIT_STATIC
   TEST(1 == isstatic_processcontext(&pcontext));

   // TEST init_processcontext
   TEST(0 == init_processcontext(&pcontext));
   TEST(0 != pcontext.valuecache);
   TEST(0 != pcontext.syslogin);
   TEST(0 != pcontext.error.stroffset);
   TEST(0 != pcontext.error.strdata);
   TEST(0 != pcontext.blockmap);
   TEST(0 != pcontext.staticmemblock);
   TEST(I == pcontext.initcount);
   TEST(S == sizestatic_threadtls(self_threadtls()) - static_memory_size());

   // TEST free_processcontext
   TEST(0 == free_processcontext(&pcontext));
   TEST(1 == isstatic_processcontext(&pcontext));
   TEST(S == sizestatic_threadtls(self_threadtls()));

   // TEST free_processcontext: initcount == 0
   processcontext_t pcontext2;
   memset(&pcontext, 255, sizeof(pcontext));
   memset(&pcontext2, 255, sizeof(pcontext2));
   pcontext.initcount = 0;
   pcontext2.initcount = 0;
   TEST(0 == free_processcontext(&pcontext));
   TEST(0 == memcmp(&pcontext, &pcontext2, sizeof(pcontext)));
   TEST(S == sizestatic_threadtls(self_threadtls()));

   // TEST init_processcontext: simulated ERROR
   pcontext = (processcontext_t) processcontext_INIT_STATIC;
   for(int i = 1; i; ++i) {
      init_testerrortimer(&s_processcontext_errtimer, (unsigned)i, i);
      int err = init_processcontext(&pcontext);
      if (err == 0) {
         free_testerrortimer(&s_processcontext_errtimer);
         TEST(0 == free_processcontext(&pcontext));
         TEST(i > I);
         break;
      }
      TEST(i == err);
      TEST(1 == isstatic_processcontext(&pcontext));
      TEST(S == sizestatic_threadtls(self_threadtls()));
   }

   // TEST free_processcontext: simulated ERROR
   for(int i = 1; i; ++i) {
      TEST(0 == init_processcontext(&pcontext));
      init_testerrortimer(&s_processcontext_errtimer, (unsigned)i, i);
      int err = free_processcontext(&pcontext);
      TEST(1 == isstatic_processcontext(&pcontext));
      TEST(S == sizestatic_threadtls(self_threadtls()));
      if (err == 0) {
         free_testerrortimer(&s_processcontext_errtimer);
         TEST(i > I);
         break;
      }
      TEST(err == i);
   }

   // TEST init_processcontext: restore default environment
   pcontext = (processcontext_t) processcontext_INIT_STATIC;
   TEST(0 == init_processcontext(&pcontext));
   TEST(I == pcontext.initcount);
   TEST(S == sizestatic_threadtls(self_threadtls()) - static_memory_size());
   TEST(0 == free_valuecache(pcontext.valuecache));
   TEST(0 == free_syslogin(pcontext.syslogin));
   TEST(0 == free_pagecacheblockmap(pcontext.blockmap));
   // restore (if setuid)
   switchtorealuser_syslogin(syslogin_maincontext());
   TEST(0 == free_static_memory(&pcontext, self_threadtls()));
   TEST(S == sizestatic_threadtls(self_threadtls()));

   return 0;
ONERR:
   (void) free_processcontext(&pcontext);
   return EINVAL;
}

static int test_query(void)
{
   processcontext_t  pcontext = processcontext_INIT_STATIC;

   // TEST isstatic_processcontext
   TEST(1 == isstatic_processcontext(&pcontext));
   pcontext.valuecache = (void*)1;
   TEST(0 == isstatic_processcontext(&pcontext));
   pcontext.valuecache = 0;
   pcontext.syslogin = (void*)1;
   TEST(0 == isstatic_processcontext(&pcontext));
   pcontext.syslogin = 0;
   pcontext.error.stroffset = 0;
   TEST(0 == isstatic_processcontext(&pcontext));
   pcontext.error.stroffset = g_errorcontext_stroffset;
   pcontext.error.strdata = 0;
   TEST(0 == isstatic_processcontext(&pcontext));
   pcontext.error.strdata = g_errorcontext_strdata;
   pcontext.blockmap = (void*)1;
   TEST(0 == isstatic_processcontext(&pcontext));
   pcontext.blockmap = 0;
   pcontext.staticmemblock = (void*)1;
   TEST(0 == isstatic_processcontext(&pcontext));
   pcontext.staticmemblock = 0;
   pcontext.initcount = 1;
   TEST(0 == isstatic_processcontext(&pcontext));
   pcontext.initcount = 0;
   TEST(1 == isstatic_processcontext(&pcontext));

   // TEST extsize_processcontext
   TEST(0 != extsize_processcontext());
   for (int i = 0; i < 2; ++i) {
      TEST(extsize_processcontext() == static_memory_size());
   }

   return 0;
ONERR:
   return EINVAL;
}

int unittest_task_processcontext()
{
   if (test_helper())      goto ONERR;
   if (test_initfree())    goto ONERR;
   if (test_query())       goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
