/* title: ThreadContext impl
   Implements the global (thread local) context of a running system thread.
   See <ThreadContext>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2012 Jörg Seebohn

   file: C-kern/api/task/threadcontext.h
    Header file <ThreadContext>.

   file: C-kern/task/threadcontext.c
    Implementation file <ThreadContext impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/task/threadcontext.h"
#include "C-kern/api/err.h"
#include "C-kern/api/io/writer/log/logmain.h"
#include "C-kern/api/memory/atomic.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/pagecache_macros.h"
#include "C-kern/api/memory/mm/mm.h"
#include "C-kern/api/platform/task/thread.h"
#include "C-kern/api/platform/task/thread_localstore.h"
#include "C-kern/api/test/errortimer.h"
// TEXTDB:SELECT('#include "'header-name'"')FROM(C-kern/resource/config/initthread)WHERE(header-name!="")
#include "C-kern/api/memory/pagecache_impl.h"
#include "C-kern/api/memory/mm/mm_impl.h"
#include "C-kern/api/task/syncrunner.h"
#include "C-kern/api/cache/objectcache_impl.h"
#include "C-kern/api/io/writer/log/logwriter.h"
// TEXTDB:END
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#endif


// section: threadcontext_t

// group: static variables

#ifdef KONFIG_UNITTEST
/* variable: s_threadcontext_errtimer
 * Simulates an error in <threadcontext_t.init_threadcontext>. */
static test_errortimer_t   s_threadcontext_errtimer = test_errortimer_FREE;
#endif

/* variable: s_threadcontext_nextid
 * The next id which is assigned to <threadcontext_t.thread_id>. */
static size_t              s_threadcontext_nextid = 0;

// group: lifetime-helper

static inline size_t static_memory_size(void)
{
   size_t memorysize = 0
// TEXTDB:SELECT("         + sizeof("objtype")")FROM(C-kern/resource/config/initthread)WHERE(inittype=="interface"||inittype=="object")
         + sizeof(pagecache_impl_t)
         + sizeof(mm_impl_t)
         + sizeof(syncrunner_t)
         + sizeof(objectcache_impl_t)
         + sizeof(logwriter_t)
// TEXTDB:END
      ;
   return memorysize;
}

/* function: alloc_static_memory
 * Allokiert statischen Speicher für alle noch für threadcontext_t zu initialisierten Objekte.
 *
 * Unchecked Precondition:
 * - is_initialized(tcontext->pagecache) */
static inline int alloc_static_memory(threadcontext_t* tcontext, thread_localstore_t* tls, /*out*/memblock_t* mblock)
{
   int err;
   const size_t size = static_memory_size();

   ONERROR_testerrortimer(&s_threadcontext_errtimer, &err, ONERR);
   err = allocstatic_threadlocalstore(tls, size, mblock);
   if (err) goto ONERR;

   tcontext->staticmemblock = mblock->addr;

   return 0;
ONERR:
   return err;
}

/* function: free_static_memory
 * Gibt statischen Speicher für alle im threadcontext_t initialisierten Objekt frei.
 *
 * Unchecked Precondition:
 * - is_initialized(tcontext->pagecache) */
static inline int free_static_memory(threadcontext_t* tcontext, thread_localstore_t* tls)
{
   int err;

   if (tcontext->staticmemblock) {
      const size_t size = static_memory_size();
      memblock_t mblock = memblock_INIT(size, tcontext->staticmemblock);

      tcontext->staticmemblock = 0;

      err = freestatic_threadlocalstore(tls, &mblock);
      SETONERROR_testerrortimer(&s_threadcontext_errtimer, &err);
      if (err) goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

/* function: config_threadcontext
 * Adapts tcontext to chosen <maincontext_e> type. */
static inline int config_threadcontext(threadcontext_t * tcontext, maincontext_e context_type)
{
   log_t * ilog;

   switch (context_type) {
   case maincontext_STATIC:
      return 0;
   case maincontext_DEFAULT:
      return 0;
   case maincontext_CONSOLE:
      ilog = cast_log(&tcontext->log);
      ilog->iimpl->setstate(ilog->object, log_channel_USERERR, log_state_UNBUFFERED);
      ilog->iimpl->setstate(ilog->object, log_channel_ERR, log_state_IGNORED);
      return 0;
   }

   return EINVAL;
}

/* function: flushlog_threadcontext
 * Writes content of log cache to corresponding I/O channels. */
static inline void flushlog_threadcontext(threadcontext_t* tcontext)
{
   tcontext->log.iimpl->flushbuffer(tcontext->log.object, log_channel_ERR);
}

// group: lifetime

int free_threadcontext(threadcontext_t* tcontext)
{
   int err = 0;
   int err2;
   threadcontext_t statictcontext = threadcontext_INIT_STATIC;
   threadcontext_t staticcontext = threadcontext_INIT_STATIC;

   // TODO: add flush caches (log + database caches) to free_threadcontext
   //       another (better?) option is to add flush caches to end of a single
   //       transaction (or end of group transaction) and commit the default transaction

   flushlog_threadcontext(tcontext);

   size_t    initcount = tcontext->initcount;
   tcontext->initcount = 0;

   switch (initcount) {
   default:
      assert(false && "initcount out of bounds");
      break;
// TEXTDB:SELECT("   case ("row-id"+1): {"\n(if (inittype=='interface') ("      "objtype"* delobj = ("objtype"*) tcontext->"parameter".object;"\n"      assert(tcontext->"parameter".iimpl == interface_"module"());"\n"      tcontext->"parameter" = staticcontext."parameter";"\n"      err2 = free_"module"(delobj);"\n"      SETONERROR_testerrortimer(&s_threadcontext_errtimer, &err2);"\n"      if (err2) err = err2;"\n)) (if (inittype=='object') ("      "objtype"* delobj = tcontext->"parameter";"\n"      tcontext->"parameter" = staticcontext."parameter";"\n"      err2 = free_"module"(delobj);"\n"      SETONERROR_testerrortimer(&s_threadcontext_errtimer, &err2);"\n"      if (err2) err = err2;"\n)) (if (inittype=='initthread') ("      err2 = freethread_"module"("(if (parameter!="") ("&tcontext->"parameter))");"\n"      SETONERROR_testerrortimer(&s_threadcontext_errtimer, &err2);"\n"      if (err2) err = err2;"\n))"   }")FROM(C-kern/resource/config/initthread)DESCENDING
   case (5+1): {
      logwriter_t* delobj = (logwriter_t*) tcontext->log.object;
      assert(tcontext->log.iimpl == interface_logwriter());
      tcontext->log = staticcontext.log;
      err2 = free_logwriter(delobj);
      SETONERROR_testerrortimer(&s_threadcontext_errtimer, &err2);
      if (err2) err = err2;
   }
   case (4+1): {
      objectcache_impl_t* delobj = (objectcache_impl_t*) tcontext->objectcache.object;
      assert(tcontext->objectcache.iimpl == interface_objectcacheimpl());
      tcontext->objectcache = staticcontext.objectcache;
      err2 = free_objectcacheimpl(delobj);
      SETONERROR_testerrortimer(&s_threadcontext_errtimer, &err2);
      if (err2) err = err2;
   }
   case (3+1): {
      syncrunner_t* delobj = tcontext->syncrunner;
      tcontext->syncrunner = staticcontext.syncrunner;
      err2 = free_syncrunner(delobj);
      SETONERROR_testerrortimer(&s_threadcontext_errtimer, &err2);
      if (err2) err = err2;
   }
   case (2+1): {
      mm_impl_t* delobj = (mm_impl_t*) tcontext->mm.object;
      assert(tcontext->mm.iimpl == interface_mmimpl());
      tcontext->mm = staticcontext.mm;
      err2 = free_mmimpl(delobj);
      SETONERROR_testerrortimer(&s_threadcontext_errtimer, &err2);
      if (err2) err = err2;
   }
   case (1+1): {
      pagecache_impl_t* delobj = (pagecache_impl_t*) tcontext->pagecache.object;
      assert(tcontext->pagecache.iimpl == interface_pagecacheimpl());
      tcontext->pagecache = staticcontext.pagecache;
      err2 = free_pagecacheimpl(delobj);
      SETONERROR_testerrortimer(&s_threadcontext_errtimer, &err2);
      if (err2) err = err2;
   }
// TEXTDB:END
   case 1:
      err2 = free_static_memory(tcontext, self_threadlocalstore());
      SETONERROR_testerrortimer(&s_threadcontext_errtimer, &err2);
      if (err2) err = err2;
   case 0:
      break;
   }

   if (1 == tcontext->thread_id) {
      // end main thread => reset s_threadcontext_nextid
      s_threadcontext_nextid = 0;
   }

   tcontext->pcontext  = statictcontext.pcontext;
   tcontext->thread_id = statictcontext.thread_id;

   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

int init_threadcontext(/*out*/threadcontext_t * tcontext, processcontext_t * pcontext, uint8_t context_type)
{
   int err;

   VALIDATE_STATE_TEST(maincontext_STATIC != type_maincontext(), ONERR_NOFREE, );

   *(volatile threadcontext_t *)tcontext = (threadcontext_t) threadcontext_INIT_STATIC;
   ((volatile threadcontext_t *)tcontext)->pcontext = pcontext;

   if (ismain_thread(self_thread())) {
      tcontext->thread_id = 1;
      s_threadcontext_nextid = 2;

   } else {
      do {
         tcontext->thread_id = add_atomicint(&s_threadcontext_nextid, 1);
      } while (tcontext->thread_id <= 1);  // if wrapped around => repeat
   }

   thread_localstore_t* tls = self_threadlocalstore();
   memblock_t mblock;
   err = alloc_static_memory(tcontext, tls, &mblock);
   if (err) goto ONERR;
   ++tcontext->initcount;

// TEXTDB:SELECT(\n"   ONERROR_testerrortimer(&s_threadcontext_errtimer, &err, ONERR);"\n (if (inittype=='interface') ("   assert( interface_"module"());"\n"   assert( sizeof("objtype") <= mblock.size);"\n"   err = init_"module"(("objtype"*) mblock.addr);"\n"   if (err) goto ONERR;"\n"   init_iobj( &tcontext->"parameter", (void*) mblock.addr, interface_"module"());"\n"   mblock.addr += sizeof("objtype");"\n"   mblock.size -= sizeof("objtype");")) (if (inittype=='object') ("   assert(sizeof("objtype") <= mblock.size);"\n"   err = init_"module"(("objtype"*) mblock.addr);"\n"   if (err) goto ONERR;"\n"   tcontext->"parameter" = ("objtype"*) mblock.addr;"\n"   mblock.addr += sizeof("objtype");"\n"   mblock.size -= sizeof("objtype");")) (if (inittype=='initthread') ("   err = initthread_"module"("(if (parameter!="") ("&tcontext->"parameter))");"\n"   if (err) goto ONERR;")) \n"   ++tcontext->initcount;")FROM(C-kern/resource/config/initthread)

   ONERROR_testerrortimer(&s_threadcontext_errtimer, &err, ONERR);
   assert( interface_pagecacheimpl());
   assert( sizeof(pagecache_impl_t) <= mblock.size);
   err = init_pagecacheimpl((pagecache_impl_t*) mblock.addr);
   if (err) goto ONERR;
   init_iobj( &tcontext->pagecache, (void*) mblock.addr, interface_pagecacheimpl());
   mblock.addr += sizeof(pagecache_impl_t);
   mblock.size -= sizeof(pagecache_impl_t);
   ++tcontext->initcount;

   ONERROR_testerrortimer(&s_threadcontext_errtimer, &err, ONERR);
   assert( interface_mmimpl());
   assert( sizeof(mm_impl_t) <= mblock.size);
   err = init_mmimpl((mm_impl_t*) mblock.addr);
   if (err) goto ONERR;
   init_iobj( &tcontext->mm, (void*) mblock.addr, interface_mmimpl());
   mblock.addr += sizeof(mm_impl_t);
   mblock.size -= sizeof(mm_impl_t);
   ++tcontext->initcount;

   ONERROR_testerrortimer(&s_threadcontext_errtimer, &err, ONERR);
   assert(sizeof(syncrunner_t) <= mblock.size);
   err = init_syncrunner((syncrunner_t*) mblock.addr);
   if (err) goto ONERR;
   tcontext->syncrunner = (syncrunner_t*) mblock.addr;
   mblock.addr += sizeof(syncrunner_t);
   mblock.size -= sizeof(syncrunner_t);
   ++tcontext->initcount;

   ONERROR_testerrortimer(&s_threadcontext_errtimer, &err, ONERR);
   assert( interface_objectcacheimpl());
   assert( sizeof(objectcache_impl_t) <= mblock.size);
   err = init_objectcacheimpl((objectcache_impl_t*) mblock.addr);
   if (err) goto ONERR;
   init_iobj( &tcontext->objectcache, (void*) mblock.addr, interface_objectcacheimpl());
   mblock.addr += sizeof(objectcache_impl_t);
   mblock.size -= sizeof(objectcache_impl_t);
   ++tcontext->initcount;

   ONERROR_testerrortimer(&s_threadcontext_errtimer, &err, ONERR);
   assert( interface_logwriter());
   assert( sizeof(logwriter_t) <= mblock.size);
   err = init_logwriter((logwriter_t*) mblock.addr);
   if (err) goto ONERR;
   init_iobj( &tcontext->log, (void*) mblock.addr, interface_logwriter());
   mblock.addr += sizeof(logwriter_t);
   mblock.size -= sizeof(logwriter_t);
   ++tcontext->initcount;
// TEXTDB:END

   ONERROR_testerrortimer(&s_threadcontext_errtimer, &err, ONERR);
   err = config_threadcontext(tcontext, context_type);
   if (err) goto ONERR;

   ONERROR_testerrortimer(&s_threadcontext_errtimer, &err, ONERR);

   return 0;
ONERR:
   (void) free_threadcontext(tcontext);
ONERR_NOFREE:
   TRACEEXIT_ERRLOG(err);
   return err;
}

// group: query

bool isstatic_threadcontext(const threadcontext_t * tcontext)
{
   return   &g_maincontext.pcontext == tcontext->pcontext
            && 0 == tcontext->pagecache.object   && 0 == tcontext->pagecache.iimpl
            && 0 == tcontext->mm.object          && 0 == tcontext->mm.iimpl
            && 0 == tcontext->syncrunner
            && 0 == tcontext->objectcache.object && 0 == tcontext->objectcache.iimpl
            && 0 == tcontext->log.object && &g_logmain_interface == tcontext->log.iimpl
            && 0 == tcontext->thread_id
            && 0 == tcontext->initcount
            && 0 == tcontext->staticmemblock;
}

size_t extsize_threadcontext(void)
{
   return static_memory_size();
}

// group: change

void resetthreadid_threadcontext()
{
   write_atomicint(&s_threadcontext_nextid, 0);
}

void setmm_threadcontext(threadcontext_t* tcontext, const threadcontext_mm_t* new_mm)
{
   initcopy_iobj(&tcontext->mm, new_mm);
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_lifehelper(void)
{
   threadcontext_t  tcontext = threadcontext_FREE;
   thread_localstore_t * tls = 0;
   const size_t   staticsize = static_memory_size();
   memblock_t         mblock = memblock_FREE;

   // prepare0
   TEST(0 == new_threadlocalstore(&tls, 0, 0));

   // TEST static_memory_size
   TEST(staticsize == static_memory_size());
   TEST(0   < staticsize);
   TEST(512 > staticsize);
   TEST(0  == staticsize % sizeof(size_t));

   // TEST alloc_static_memory
   TEST(0 == alloc_static_memory(&tcontext, tls, &mblock))
   // check params
   TEST(mblock.addr == tcontext.staticmemblock);
   TEST(mblock.size == sizestatic_threadlocalstore(tls));
   TEST(mblock.addr >  (uint8_t*) tls);
   TEST(mblock.size == staticsize);

   // TEST free_static_memory
   TEST(0 == free_static_memory(&tcontext, tls))
   // check params
   TEST(0 == tcontext.staticmemblock);
   TEST(0 == sizestatic_threadlocalstore(tls));

   // TEST free_static_memory: already freed
   TEST(0 == free_static_memory(&tcontext, tls))
   // check params
   TEST(0 == tcontext.staticmemblock);
   TEST(0 == sizestatic_threadlocalstore(tls));

   // TEST alloc_static_memory: simulated ERROR
   mblock = (memblock_t) memblock_FREE;
   init_testerrortimer(&s_threadcontext_errtimer, 1, 4);
   TEST(4 == alloc_static_memory(&tcontext, tls, &mblock))
   // check params
   TEST(0 == tcontext.staticmemblock);
   TEST(0 == sizestatic_threadlocalstore(tls));
   TEST( isfree_memblock(&mblock));

   // TEST free_static_memory: simulated ERROR
   TEST(0 == alloc_static_memory(&tcontext, tls, &mblock))
   init_testerrortimer(&s_threadcontext_errtimer, 1, 4);
   TEST(4 == free_static_memory(&tcontext, tls))
   // check params
   TEST(0 == tcontext.staticmemblock);
   TEST(0 == sizestatic_threadlocalstore(tls));

   // prepare0
   TEST(0 == delete_threadlocalstore(&tls));

   return 0;
ONERR:
   delete_threadlocalstore(&tls);
   return EINVAL;
}

static int thread_testid(void* expect_id)
{
   TEST((size_t) expect_id == tcontext_maincontext()->thread_id);

   return 0;
ONERR:
   return EINVAL;
}

static int test_initfree(void)
{
   threadcontext_t   tcontext = threadcontext_FREE;
   thread_t*         thread   = 0;
   processcontext_t* P        = pcontext_maincontext();
   const size_t      nrsvc    = 6;
   const size_t      S        = sizestatic_threadlocalstore(self_threadlocalstore());

   // prepare
   TEST(P != 0);
   memblock_t _mb = memblock_FREE;
   TEST(0 == allocstatic_threadlocalstore(self_threadlocalstore(), 0, &_mb));
   const uint8_t* M = _mb.addr;
   TEST(0 != M);

   // TEST threadcontext_FREE
   TEST(0 == tcontext.pcontext);
   TEST(0 == tcontext.pagecache.object);
   TEST(0 == tcontext.pagecache.iimpl);
   TEST(0 == tcontext.mm.object);
   TEST(0 == tcontext.mm.iimpl);
   TEST(0 == tcontext.syncrunner);
   TEST(0 == tcontext.objectcache.object);
   TEST(0 == tcontext.objectcache.iimpl);
   TEST(0 == tcontext.log.object);
   TEST(0 == tcontext.log.iimpl);
   TEST(0 == tcontext.thread_id);
   TEST(0 == tcontext.initcount);
   TEST(0 == tcontext.staticmemblock);

   // TEST threadcontext_INIT_STATIC
   tcontext = (threadcontext_t) threadcontext_INIT_STATIC;
   TEST(1 == isstatic_threadcontext(&tcontext));

   maincontext_e contexttype[] = { maincontext_DEFAULT, maincontext_CONSOLE };
   for (unsigned i = 0; i < lengthof(contexttype); ++i) {

      // TEST init_threadcontext
      TEST(0 == init_threadcontext(&tcontext, P, contexttype[i]));
      // check self_threadlocalstore
      TEST(S == sizestatic_threadlocalstore(self_threadlocalstore()) - static_memory_size());
      // check tcontext
      TEST(P == tcontext.pcontext);
      TEST(0 != tcontext.thread_id);
      TEST(nrsvc == tcontext.initcount);
      TEST(M == tcontext.staticmemblock);
      // check tcontext object memory address
      uint8_t* nextaddr = tcontext.staticmemblock;
// TEXTDB:SELECT("      TEST((void*)nextaddr == tcontext."parameter(if (inittype=='interface') ".object" else "")");"\n"      nextaddr += sizeof("objtype");")FROM(C-kern/resource/config/initthread)WHERE(inittype=="interface"||inittype=="object")
      TEST((void*)nextaddr == tcontext.pagecache.object);
      nextaddr += sizeof(pagecache_impl_t);
      TEST((void*)nextaddr == tcontext.mm.object);
      nextaddr += sizeof(mm_impl_t);
      TEST((void*)nextaddr == tcontext.syncrunner);
      nextaddr += sizeof(syncrunner_t);
      TEST((void*)nextaddr == tcontext.objectcache.object);
      nextaddr += sizeof(objectcache_impl_t);
      TEST((void*)nextaddr == tcontext.log.object);
      nextaddr += sizeof(logwriter_t);
// TEXTDB:END
      TEST(nextaddr == (uint8_t*) tcontext.staticmemblock + static_memory_size());
      // check tcontext interface address
// TEXTDB:SELECT("      TEST(tcontext."parameter".iimpl == interface_"module"());")FROM(C-kern/resource/config/initthread)WHERE(inittype=="interface")
      TEST(tcontext.pagecache.iimpl == interface_pagecacheimpl());
      TEST(tcontext.mm.iimpl == interface_mmimpl());
      TEST(tcontext.objectcache.iimpl == interface_objectcacheimpl());
      TEST(tcontext.log.iimpl == interface_logwriter());
// TEXTDB:END
      switch (contexttype[i]) {
      case maincontext_STATIC:
         break;
      case maincontext_DEFAULT:
         TEST(log_state_IGNORED  == tcontext.log.iimpl->getstate(tcontext.log.object, log_channel_USERERR));
         TEST(log_state_BUFFERED == tcontext.log.iimpl->getstate(tcontext.log.object, log_channel_ERR));
         break;
      case maincontext_CONSOLE:
         TEST(log_state_UNBUFFERED == tcontext.log.iimpl->getstate(tcontext.log.object, log_channel_USERERR));
         TEST(log_state_IGNORED    == tcontext.log.iimpl->getstate(tcontext.log.object, log_channel_ERR));
         break;
      }

      // TEST free_threadcontext
      TEST(0 == free_threadcontext(&tcontext));
      TEST(1 == isstatic_threadcontext(&tcontext));

      // TEST free_threadcontext: already freed
      TEST(0 == free_threadcontext(&tcontext));
      TEST(1 == isstatic_threadcontext(&tcontext));
   }

   // TEST init_threadcontext: main thread ==> thread_id == 1 && s_threadcontext_nextid == 2
   TEST( ismain_thread(self_thread()));
   s_threadcontext_nextid = 100;
   TEST(0 == init_threadcontext(&tcontext, P, maincontext_DEFAULT));
   TEST(1 == tcontext.thread_id);
   TEST(2 == s_threadcontext_nextid);

   // TEST free_threadcontext: main thread ==> reset s_threadcontext_nextid
   TEST(0 == free_threadcontext(&tcontext));
   TEST(0 == s_threadcontext_nextid);

   // TEST init_threadcontext: s_threadcontext_nextid incremented && s_threadcontext_nextid > 1
   s_threadcontext_nextid = 0;
   for (unsigned i = 2; i < 10; ++i) {
      TEST(0 == new_thread(&thread, thread_testid, (void*)i));
      TEST(0 == join_thread(thread));
      // check id was valid
      TEST(0 == returncode_thread(thread));
      TEST(0 == delete_thread(&thread));
      // check s_threadcontext_nextid incremented
      TEST(i+1 == s_threadcontext_nextid);
   }

   // TEST init_threadcontext: EPROTO
   // prepare
   maincontext_e oldtype = type_maincontext();
   g_maincontext.type = maincontext_STATIC;
   // test
   TEST(EPROTO == init_threadcontext(&tcontext, P, maincontext_DEFAULT));
   TEST(1 == isstatic_threadcontext(&tcontext));
   // reset
   g_maincontext.type = oldtype;

   // TEST init_threadcontext: EINVAL
   TEST(EINVAL == init_threadcontext(&tcontext, P, maincontext_CONSOLE+1));
   TEST(1 == isstatic_threadcontext(&tcontext));

   // TEST init_threadcontext: sumulated ERROR
   s_threadcontext_nextid = 0;
   for(unsigned i = 1; i; ++i) {
      init_testerrortimer(&s_threadcontext_errtimer, i, (int)i);
      memset(&tcontext, 0xff, sizeof(tcontext));
      int err = init_threadcontext(&tcontext, P, maincontext_DEFAULT);
      free_testerrortimer(&s_threadcontext_errtimer);
      if (!err) {
         TEST(2 == s_threadcontext_nextid);
         TEST(0 == free_threadcontext(&tcontext));
         TEST(i > nrsvc);
      }
      TEST(0 == s_threadcontext_nextid); // reset
      TEST(1 == isstatic_threadcontext(&tcontext));
      if (!err) break;
      TEST(i == (unsigned)err);
   }

   // TEST free_threadcontext: simulated ERROR
   for(unsigned i = 1; i; ++i) {
      TEST(0 == init_threadcontext(&tcontext, P, maincontext_DEFAULT));
      init_testerrortimer(&s_threadcontext_errtimer, i, (int)i);
      int err = free_threadcontext(&tcontext);
      free_testerrortimer(&s_threadcontext_errtimer);
      TEST(0 == s_threadcontext_nextid); // reset
      TEST(1 == isstatic_threadcontext(&tcontext));
      if (!err) {
         TEST(i > nrsvc);
         break;
      }
      TEST(i == (unsigned)err);
   }

   return 0;
ONERR:
   free_testerrortimer(&s_threadcontext_errtimer);
   delete_thread(&thread);
   return EINVAL;
}

static int test_query(void)
{
   threadcontext_t tcontext = threadcontext_INIT_STATIC;

   // TEST isstatic_threadcontext
   TEST(1 == isstatic_threadcontext(&tcontext));
   tcontext.pcontext = 0;
   TEST(0 == isstatic_threadcontext(&tcontext));
   tcontext.pcontext = &g_maincontext.pcontext;
   tcontext.pagecache.object = (void*)1;
   TEST(0 == isstatic_threadcontext(&tcontext));
   tcontext.pagecache.object = 0;
   tcontext.pagecache.iimpl = (void*)1;
   TEST(0 == isstatic_threadcontext(&tcontext));
   tcontext.pagecache.iimpl = 0;
   tcontext.mm.object = (void*)1;
   TEST(0 == isstatic_threadcontext(&tcontext));
   tcontext.mm.object = 0;
   tcontext.mm.iimpl = (void*)1;
   TEST(0 == isstatic_threadcontext(&tcontext));
   tcontext.mm.iimpl = 0;
   tcontext.syncrunner = (void*)1;
   TEST(0 == isstatic_threadcontext(&tcontext));
   tcontext.syncrunner = 0;
   tcontext.objectcache.object = (void*)1;
   TEST(0 == isstatic_threadcontext(&tcontext));
   tcontext.objectcache.object = 0;
   tcontext.objectcache.iimpl = (void*)1;
   TEST(0 == isstatic_threadcontext(&tcontext));
   tcontext.objectcache.iimpl = 0;
   tcontext.log.object = (struct log_t*)1;
   TEST(0 == isstatic_threadcontext(&tcontext));
   tcontext.log.object = 0;
   tcontext.log.iimpl = 0;
   TEST(0 == isstatic_threadcontext(&tcontext));
   tcontext.log.iimpl = &g_logmain_interface;
   tcontext.thread_id = 1;
   TEST(0 == isstatic_threadcontext(&tcontext));
   tcontext.thread_id = 0;
   tcontext.initcount = 1;
   TEST(0 == isstatic_threadcontext(&tcontext));
   tcontext.initcount = 0;
   tcontext.staticmemblock = (void*)1;
   TEST(0 == isstatic_threadcontext(&tcontext));
   tcontext.staticmemblock = 0;
   TEST(1 == isstatic_threadcontext(&tcontext));

   // TEST extsize_threadcontext
   for (int i = 0; i < 4; ++i) {
      TEST(extsize_threadcontext() == static_memory_size());
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_change(void)
{
   threadcontext_t tcontext = threadcontext_FREE;

   // TEST resetthreadid_threadcontext
   s_threadcontext_nextid = 10;
   TEST(0 != s_threadcontext_nextid);
   resetthreadid_threadcontext();
   TEST(0 == s_threadcontext_nextid);

   // TEST setmm_threadcontext
   setmm_threadcontext(&tcontext, &(threadcontext_mm_t) iobj_INIT((void*)1, (void*)2));
   TEST(tcontext.mm.object == (struct mm_t*)1);
   TEST(tcontext.mm.iimpl  == (struct mm_it*)2);
   setmm_threadcontext(&tcontext, &(threadcontext_mm_t) iobj_INIT(0, 0));
   TEST(tcontext.mm.object == 0);
   TEST(tcontext.mm.iimpl  == 0);

   return 0;
ONERR:
   return EINVAL;
}

int unittest_task_threadcontext()
{
   size_t   oldid = s_threadcontext_nextid;

   if (test_lifehelper())  goto ONERR;
   if (test_initfree())    goto ONERR;
   if (test_query())       goto ONERR;
   if (test_change())      goto ONERR;

   s_threadcontext_nextid = oldid;
   return 0;
ONERR:
   s_threadcontext_nextid = oldid;
   return EINVAL;
}

#endif
