/* title: ThreadContext impl

   Implements <ThreadContext>.

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
#include "C-kern/api/io/writer/log/logbuffer.h" // needed by logwriter.h
#include "C-kern/api/memory/atomic.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/pagecache_macros.h"
#include "C-kern/api/memory/vm.h"
#include "C-kern/api/memory/mm/mm.h"
#include "C-kern/api/platform/task/thread.h"
#include "C-kern/api/platform/task/thread_stack.h"
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
#include "C-kern/api/test/mm/testmm.h"
#include "C-kern/api/io/iochannel.h"
#include "C-kern/api/io/pipe.h"
#endif

// Wird als erste S
typedef struct static_data_t {
   logwriter_t       logwriter;                    // initstatic_threadcontext initializes it with initstatic_logwriter
   uint8_t           logmem[log_config_MINSIZE];   // used for logwriter
} static_data_t;


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
   size_t memorysize = sizeof(static_data_t)
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
 * Allokiert statischen Speicher für alle noch für threadcontext_t zu initialisierenden Objekte.
 *
 * Unchecked Precondition:
 * - is_initialized(castPcontext_threadstack(tcontext)) */
static inline int alloc_static_memory(threadcontext_t* tcontext)
{
   int err;
   memblock_t mblock;
   const size_t size = static_memory_size();

   if (! PROCESS_testerrortimer(&s_threadcontext_errtimer, &err)) {
      err = allocstatic_threadstack(castPcontext_threadstack(tcontext), size, &mblock);
   }
   if (err) goto ONERR;

   /* set out */
   tcontext->staticdata = mblock.addr;

   return 0;
ONERR:
   TRACE_LOG(AUTO, log_channel_ERR, log_flags_LAST, FUNCTION_EXIT_ERRLOG, err);
   return err;
}

/* function: free_static_memory
 * Gibt statischen Speicher für alle im threadcontext_t initialisierten Objekt frei.
 *
 * Unchecked Precondition:
 * - is_initialized(st) */
static inline int free_static_memory(threadcontext_t* tcontext)
{
   int err;

   if (tcontext->staticdata) {
      const size_t size = static_memory_size();
      memblock_t mblock = memblock_INIT(size, tcontext->staticdata);

      tcontext->staticdata = 0;

      err = freestatic_threadstack(castPcontext_threadstack(tcontext), &mblock);
      (void) PROCESS_testerrortimer(&s_threadcontext_errtimer, &err);
      if (err) goto ONERR;
   }

   return 0;
ONERR:
   TRACE_LOG(AUTO, log_channel_ERR, log_flags_LAST, FUNCTION_EXIT_FREE_RESOURCE_ERRLOG, err);
   return err;
}

/* function: config_threadcontext
 * Adapts tcontext to chosen <maincontext_e> type. */
static inline int config_threadcontext(threadcontext_t * tcontext, maincontext_e context_type)
{
   ilog_t* ilog;

   switch (context_type) {
   case maincontext_STATIC:
      return 0;
   case maincontext_DEFAULT:
      return 0;
   case maincontext_CONSOLE:
      ilog = &tcontext->log;
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

static inline static_data_t* get_static_data(const threadcontext_t* tcontext)
{
   return (static_data_t*) tcontext->staticdata;
}

static inline void set_static_data(threadcontext_t* tcontext, maincontext_t* maincontext, static_data_t* sd)
{
   tcontext->maincontext = maincontext;
   tcontext->log = (ilog_t) iobj_INIT((struct log_t*)&sd->logwriter, interface_logwriter());
}

// group: lifetime

int initstatic_threadcontext(threadcontext_t* tcontext)
{
   int err;

   err = alloc_static_memory(tcontext);
   if (err) goto ONERR;

   static_data_t *sd = (static_data_t*) tcontext->staticdata;
   if (! PROCESS_testerrortimer(&s_threadcontext_errtimer, &err)) {
      err = initstatic_logwriter(&sd->logwriter, sizeof(sd->logmem), sd->logmem);
   }
   if (err) goto ONERR;

   set_static_data(tcontext, &g_maincontext, sd);

   return 0;
ONERR:
   free_static_memory(tcontext);
   TRACE_LOG(AUTO, log_channel_ERR, log_flags_LAST, FUNCTION_EXIT_ERRLOG, err);
   return err;
}

int freestatic_threadcontext(threadcontext_t* tcontext)
{
   int err;

   if (tcontext->staticdata) {
      static_data_t *sd = (static_data_t*) tcontext->staticdata;
      tcontext->log = (ilog_t) iobj_FREE;
      freestatic_logwriter(&sd->logwriter);
      err = free_static_memory(tcontext);
      if (err) goto ONERR;
   }

   return 0;
ONERR:
   TRACE_LOG(AUTO, log_channel_ERR, log_flags_LAST, FUNCTION_EXIT_FREE_RESOURCE_ERRLOG, err);
   return err;
}

int free_threadcontext(threadcontext_t* tcontext)
{
   int err = 0;
   int err2;
   threadcontext_t staticcontext = threadcontext_FREE;
   set_static_data(&staticcontext, tcontext->maincontext, get_static_data(tcontext));

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
// TEXTDB:SELECT("   case ("row-id"+0): {"\n(if (inittype=='interface') ("      "objtype"* delobj = ("objtype"*) tcontext->"parameter".object;"\n"      assert(tcontext->"parameter".iimpl == interface_"module"());"\n"      tcontext->"parameter" = staticcontext."parameter";"\n"      err2 = free_"module"(delobj);"\n"      (void) PROCESS_testerrortimer(&s_threadcontext_errtimer, &err2);"\n"      if (err2) err = err2;"\n)) (if (inittype=='object') ("      "objtype"* delobj = tcontext->"parameter";"\n"      tcontext->"parameter" = staticcontext."parameter";"\n"      err2 = free_"module"(delobj);"\n"      (void) PROCESS_testerrortimer(&s_threadcontext_errtimer, &err2);"\n"      if (err2) err = err2;"\n)) (if (inittype=='initthread') ("      err2 = freethread_"module"("(if (parameter!="") ("&tcontext->"parameter))");"\n"      (void) PROCESS_testerrortimer(&s_threadcontext_errtimer, &err2);"\n"      if (err2) err = err2;"\n))"   }")FROM(C-kern/resource/config/initthread)DESCENDING
   case (5+0): {
      logwriter_t* delobj = (logwriter_t*) tcontext->log.object;
      assert(tcontext->log.iimpl == interface_logwriter());
      tcontext->log = staticcontext.log;
      err2 = free_logwriter(delobj);
      (void) PROCESS_testerrortimer(&s_threadcontext_errtimer, &err2);
      if (err2) err = err2;
   }
   case (4+0): {
      objectcache_impl_t* delobj = (objectcache_impl_t*) tcontext->objectcache.object;
      assert(tcontext->objectcache.iimpl == interface_objectcacheimpl());
      tcontext->objectcache = staticcontext.objectcache;
      err2 = free_objectcacheimpl(delobj);
      (void) PROCESS_testerrortimer(&s_threadcontext_errtimer, &err2);
      if (err2) err = err2;
   }
   case (3+0): {
      syncrunner_t* delobj = tcontext->syncrunner;
      tcontext->syncrunner = staticcontext.syncrunner;
      err2 = free_syncrunner(delobj);
      (void) PROCESS_testerrortimer(&s_threadcontext_errtimer, &err2);
      if (err2) err = err2;
   }
   case (2+0): {
      mm_impl_t* delobj = (mm_impl_t*) tcontext->mm.object;
      assert(tcontext->mm.iimpl == interface_mmimpl());
      tcontext->mm = staticcontext.mm;
      err2 = free_mmimpl(delobj);
      (void) PROCESS_testerrortimer(&s_threadcontext_errtimer, &err2);
      if (err2) err = err2;
   }
   case (1+0): {
      pagecache_impl_t* delobj = (pagecache_impl_t*) tcontext->pagecache.object;
      assert(tcontext->pagecache.iimpl == interface_pagecacheimpl());
      tcontext->pagecache = staticcontext.pagecache;
      err2 = free_pagecacheimpl(delobj);
      (void) PROCESS_testerrortimer(&s_threadcontext_errtimer, &err2);
      if (err2) err = err2;
   }
// TEXTDB:END
   case 0:
      break;
   }

   if (1 == tcontext->thread_id) {
      // end main thread => reset s_threadcontext_nextid
      s_threadcontext_nextid = 0;
   }

   // keep tcontext->thread_id
   // keep tcontext->maincontext

   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

int init_threadcontext(/*out*/threadcontext_t * tcontext, uint8_t context_type)
{
   int err;

   VALIDATE_INPARAM_TEST(context_type < maincontext__NROF, ONERR_NOFREE, );

   VALIDATE_STATE_TEST(maincontext_STATIC != type_maincontext(), ONERR_NOFREE, );

   thread_stack_t* st = castPcontext_threadstack(tcontext);

   // assert(isstatic_threadcontext(tcontext));
   // ==> normal log functions are available / no init log needed

   if (ismain_thread(thread_threadstack(st))) {
      tcontext->thread_id = 1;
      s_threadcontext_nextid = 2;

   } else {
      do {
         tcontext->thread_id = add_atomicint(&s_threadcontext_nextid, 1);
      } while (tcontext->thread_id <= 1);  // if wrapped around => repeat
   }

   assert(0 != tcontext->staticdata);
   assert(0 == tcontext->initcount);
   memblock_t mblock = memblock_INIT(static_memory_size()-sizeof(static_data_t), tcontext->staticdata + sizeof(static_data_t));

// TEXTDB:SELECT(\n"   if (! PROCESS_testerrortimer(&s_threadcontext_errtimer, &err)) {"\n (if (inittype=='interface') ("      assert( interface_"module"());"\n"      assert( sizeof("objtype") <= mblock.size);"\n"      err = init_"module"(("objtype"*) mblock.addr);"\n"   }"\n"   if (err) goto ONERR;"\n"   init_iobj( &tcontext->"parameter", (void*) mblock.addr, interface_"module"());"\n"   mblock.addr += sizeof("objtype");"\n"   mblock.size -= sizeof("objtype");")) (if (inittype=='object') ("      assert(sizeof("objtype") <= mblock.size);"\n"      err = init_"module"(("objtype"*) mblock.addr);"\n"   }"\n"   if (err) goto ONERR;"\n"   tcontext->"parameter" = ("objtype"*) mblock.addr;"\n"   mblock.addr += sizeof("objtype");"\n"   mblock.size -= sizeof("objtype");")) (if (inittype=='initthread') ("      err = initthread_"module"("(if (parameter!="") ("&tcontext->"parameter))");"\n"   }"\n"   if (err) goto ONERR;")) \n"   ++tcontext->initcount;")FROM(C-kern/resource/config/initthread)

   if (! PROCESS_testerrortimer(&s_threadcontext_errtimer, &err)) {
      assert( interface_pagecacheimpl());
      assert( sizeof(pagecache_impl_t) <= mblock.size);
      err = init_pagecacheimpl((pagecache_impl_t*) mblock.addr);
   }
   if (err) goto ONERR;
   init_iobj( &tcontext->pagecache, (void*) mblock.addr, interface_pagecacheimpl());
   mblock.addr += sizeof(pagecache_impl_t);
   mblock.size -= sizeof(pagecache_impl_t);
   ++tcontext->initcount;

   if (! PROCESS_testerrortimer(&s_threadcontext_errtimer, &err)) {
      assert( interface_mmimpl());
      assert( sizeof(mm_impl_t) <= mblock.size);
      err = init_mmimpl((mm_impl_t*) mblock.addr);
   }
   if (err) goto ONERR;
   init_iobj( &tcontext->mm, (void*) mblock.addr, interface_mmimpl());
   mblock.addr += sizeof(mm_impl_t);
   mblock.size -= sizeof(mm_impl_t);
   ++tcontext->initcount;

   if (! PROCESS_testerrortimer(&s_threadcontext_errtimer, &err)) {
      assert(sizeof(syncrunner_t) <= mblock.size);
      err = init_syncrunner((syncrunner_t*) mblock.addr);
   }
   if (err) goto ONERR;
   tcontext->syncrunner = (syncrunner_t*) mblock.addr;
   mblock.addr += sizeof(syncrunner_t);
   mblock.size -= sizeof(syncrunner_t);
   ++tcontext->initcount;

   if (! PROCESS_testerrortimer(&s_threadcontext_errtimer, &err)) {
      assert( interface_objectcacheimpl());
      assert( sizeof(objectcache_impl_t) <= mblock.size);
      err = init_objectcacheimpl((objectcache_impl_t*) mblock.addr);
   }
   if (err) goto ONERR;
   init_iobj( &tcontext->objectcache, (void*) mblock.addr, interface_objectcacheimpl());
   mblock.addr += sizeof(objectcache_impl_t);
   mblock.size -= sizeof(objectcache_impl_t);
   ++tcontext->initcount;

   if (! PROCESS_testerrortimer(&s_threadcontext_errtimer, &err)) {
      assert( interface_logwriter());
      assert( sizeof(logwriter_t) <= mblock.size);
      err = init_logwriter((logwriter_t*) mblock.addr);
   }
   if (err) goto ONERR;
   init_iobj( &tcontext->log, (void*) mblock.addr, interface_logwriter());
   mblock.addr += sizeof(logwriter_t);
   mblock.size -= sizeof(logwriter_t);
   ++tcontext->initcount;
// TEXTDB:END

   if (! PROCESS_testerrortimer(&s_threadcontext_errtimer, &err)) {
      err = config_threadcontext(tcontext, context_type);
      (void) PROCESS_testerrortimer(&s_threadcontext_errtimer, &err);
   }
   if (err) goto ONERR;

   return 0;
ONERR:
   (void) free_threadcontext(tcontext);
ONERR_NOFREE:
   TRACEEXIT_ERRLOG(err);
   return err;
}


// group: query

bool isstatic_threadcontext(const threadcontext_t* tcontext)
{
   struct log_t *log = (struct log_t*) &get_static_data(tcontext)->logwriter;
   return   &g_maincontext == tcontext->maincontext
            && isfree_iobj(&tcontext->pagecache)
            && isfree_iobj(&tcontext->mm)
            && 0 == tcontext->syncrunner
            && isfree_iobj(&tcontext->objectcache)
            && log == tcontext->log.object
            && interface_logwriter() == tcontext->log.iimpl
            && 0 == tcontext->initcount
            && (const uint8_t*)tcontext < tcontext->staticdata;
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


// group: test

#ifdef KONFIG_UNITTEST

static int test_lifehelper(void)
{
   threadcontext_t *tcontext = 0;
   thread_stack_t  *st       = 0;
   const size_t   staticsize = static_memory_size();

   // prepare0
   TEST(0 == new_threadstack(&st, extsize_threadcontext(), 0, 0));
   tcontext = context_threadstack(st);

   // TEST static_data_t
   TEST( sizeof(static_data_t) == (sizeof(logwriter_t)+log_config_MINSIZE));

   // TEST static_memory_size
   TEST(staticsize == static_memory_size());
   TEST(sizeof(static_data_t)     < staticsize);
   TEST(sizeof(static_data_t)+640 > staticsize);
   TEST(0  == staticsize % sizeof(size_t));

   // TEST alloc_static_memory
   TEST(0 == alloc_static_memory(tcontext))
   // check params
   TEST((uint8_t*)st < tcontext->staticdata);
   TEST(staticsize == sizestatic_threadstack(st));

   // TEST free_static_memory
   TEST(0 == free_static_memory(tcontext))
   // check params
   TEST(0 == tcontext->staticdata);
   TEST(0 == sizestatic_threadstack(st));

   // TEST free_static_memory: already freed
   TEST(0 == free_static_memory(tcontext))
   // check params
   TEST(0 == tcontext->staticdata);
   TEST(0 == sizestatic_threadstack(st));

   // TEST alloc_static_memory: simulated ERROR
   init_testerrortimer(&s_threadcontext_errtimer, 1, 4);
   TEST(4 == alloc_static_memory(tcontext))
   // check params
   TEST(0 == tcontext->staticdata);
   TEST(0 == sizestatic_threadstack(st));

   // TEST free_static_memory: simulated ERROR
   TEST(0 == alloc_static_memory(tcontext))
   init_testerrortimer(&s_threadcontext_errtimer, 1, 4);
   TEST(4 == free_static_memory(tcontext))
   // check params
   TEST(0 == tcontext->staticdata);
   TEST(0 == sizestatic_threadstack(st));

   // reset0
   TEST(0 == delete_threadstack(&st));

   return 0;
ONERR:
   delete_threadstack(&st);
   return EINVAL;
}

static int test_initfree_static(void)
{
   threadcontext_t *tcontext = 0;
   thread_stack_t  *st       = 0;
   pipe_t         pipe = pipe_FREE;
   iochannel_t    olderr = iochannel_FREE;
   static_data_t *sd;
   char           buffer[15];

   // prepare0
   TEST(0 == new_threadstack(&st, extsize_threadcontext(), 0, 0));
   tcontext = context_threadstack(st);
   TEST(0 == init_pipe(&pipe))
   olderr = dup(STDERR_FILENO);
   TEST(olderr > 0);
   TEST(STDERR_FILENO == dup2(pipe.write, STDERR_FILENO));

   // TEST initstatic_threadcontext
   *tcontext = (threadcontext_t) threadcontext_FREE;
   TEST( 0 == initstatic_threadcontext(tcontext));
   // check tcontext
   TEST( tcontext->maincontext == &g_maincontext);
   TEST( tcontext->staticdata  != 0);
   TEST( tcontext->log.object  == (struct log_t*)tcontext->staticdata);
   TEST( tcontext->log.iimpl   == interface_logwriter());
   // check memory
   TEST( (uint8_t*)st < tcontext->staticdata);
   TEST( static_memory_size() == sizestatic_threadstack(st));
   // check logwriter
   sd = (static_data_t*) tcontext->staticdata;
   TEST( sd->logwriter.addr == sd->logmem);
   TEST( sd->logwriter.size == sizeof(sd->logmem));
   printf_logwriter(&sd->logwriter, log_channel_ERR, log_flags_LAST, 0, "%s", "hello log");
   TEST( 9 == read(pipe.read, buffer, sizeof(buffer)));
   TEST( 0 == strncmp(buffer, "hello log", 9));
   // check isstatic_threadcontext
   TEST( 1 == isstatic_threadcontext(tcontext));

   // TEST freestatic_threadcontext
   TEST( 0 == freestatic_threadcontext(tcontext));
   // check memory
   TEST( 0 == tcontext->staticdata);
   // check tcontext
   TEST( tcontext->log.object == 0);
   TEST( tcontext->log.iimpl  == 0);

   // TEST initstatic_threadcontext: simulated error
   for (unsigned i=1; ; ++i) {
      init_testerrortimer(&s_threadcontext_errtimer, i, (int)i);
      *tcontext = (threadcontext_t) threadcontext_FREE;
      int err = initstatic_threadcontext(tcontext);
      if (!err) {
         TEST( 3 == i);
         free_testerrortimer(&s_threadcontext_errtimer);
         break;
      }
      TEST( (int)i == err);
      // check nothing changed
      TEST( 0 == memcmp(tcontext, &(threadcontext_t)threadcontext_FREE, sizeof(threadcontext_t)));
      TEST( 0 == sizestatic_threadstack(st));
   }
   TEST( 0 == freestatic_threadcontext(tcontext));

   // reset0
   TEST(0 == delete_threadstack(&st));
   TEST(STDERR_FILENO == dup2(olderr, STDERR_FILENO));
   TEST(0 == free_iochannel(&olderr));
   TEST(0 == free_pipe(&pipe));

   return 0;
ONERR:
   delete_threadstack(&st);
   if (olderr > 0) {
      dup2(olderr, STDERR_FILENO);
      free_iochannel(&olderr);
   }
   free_pipe(&pipe);
   return EINVAL;
}

static int test_initfree_main(void)
{
   threadcontext_t* tc = tcontext_maincontext();

   // prepare
   TEST( ismain_thread(self_thread()));
   TEST(0 == switchoff_testmm());

   // TEST free_threadcontext: main thread ==> reset s_threadcontext_nextid
   s_threadcontext_nextid = 100;
   TEST(0 == free_threadcontext(tc));
   TEST(0 == s_threadcontext_nextid);

   // TEST init_threadcontext: main thread ==> thread_id == 1 && s_threadcontext_nextid == 2
   s_threadcontext_nextid = 100;
   TEST(0 == init_threadcontext(tc, maincontext_DEFAULT));
   TEST(1 == tc->thread_id);
   TEST(2 == s_threadcontext_nextid);

   return 0;
ONERR:
   return EINVAL;
}

static int test_initfree(void)
{
   thread_stack_t*   st = 0;
   threadcontext_t*  tc = 0;
   thread_t*         thread = 0;
   maincontext_t *   M  = self_maincontext();
   memblock_t        SM = memblock_FREE; // addr of allocated static memory block (staticdata) in threadcontext
   const size_t      nrsvc   = 5;
   maincontext_e     oldtype = maincontext_STATIC;

   // prepare
   TEST(0 == new_threadstack(&st, extsize_threadcontext(), 0, 0));
   TEST(0 == allocstatic_threadstack(st, 0, &SM));
   tc = context_threadstack(st);
   TEST(0 != tc);
   TEST(0 != M);
   TEST(0 != SM.addr);

   // TEST threadcontext_FREE
   *tc = (threadcontext_t) threadcontext_FREE;
   TEST(0 == tc->maincontext);
   TEST(0 == tc->pagecache.object);
   TEST(0 == tc->pagecache.iimpl);
   TEST(0 == tc->mm.object);
   TEST(0 == tc->mm.iimpl);
   TEST(0 == tc->syncrunner);
   TEST(0 == tc->objectcache.object);
   TEST(0 == tc->objectcache.iimpl);
   TEST(0 == tc->log.object);
   TEST(0 == tc->log.iimpl);
   TEST(0 == tc->thread_id);
   TEST(0 == tc->initcount);
   TEST(0 == tc->staticdata);

   // prepare
   TEST(0 == initstatic_threadcontext(tc));
   TEST(static_memory_size() == sizestatic_threadstack(st));

   maincontext_e contexttype[] = { maincontext_DEFAULT, maincontext_CONSOLE };
   s_threadcontext_nextid = 2;
   for (size_t i = 0, ID = s_threadcontext_nextid; i < lengthof(contexttype); ++i, ++ID) {

      // TEST init_threadcontext
      TEST(0 == init_threadcontext(tc, contexttype[i]));
      // check st
      TEST(static_memory_size() == sizestatic_threadstack(st));
      // check s_threadcontext_nextid
      TEST(ID+1 == s_threadcontext_nextid);
      // check tcontext
      TEST(tc->maincontext == M);
      TEST(tc->thread_id   == ID);
      TEST(tc->initcount   == 5);
      TEST(tc->staticdata == SM.addr);
      // check tcontext object memory address
      uint8_t* nextaddr = tc->staticdata+sizeof(static_data_t);
// TEXTDB:SELECT("      TEST((void*)nextaddr == tc->"parameter(if (inittype=='interface') ".object" else "")");"\n"      nextaddr += sizeof("objtype");")FROM(C-kern/resource/config/initthread)WHERE(inittype=="interface"||inittype=="object")
      TEST((void*)nextaddr == tc->pagecache.object);
      nextaddr += sizeof(pagecache_impl_t);
      TEST((void*)nextaddr == tc->mm.object);
      nextaddr += sizeof(mm_impl_t);
      TEST((void*)nextaddr == tc->syncrunner);
      nextaddr += sizeof(syncrunner_t);
      TEST((void*)nextaddr == tc->objectcache.object);
      nextaddr += sizeof(objectcache_impl_t);
      TEST((void*)nextaddr == tc->log.object);
      nextaddr += sizeof(logwriter_t);
// TEXTDB:END
      TEST(nextaddr == tc->staticdata + static_memory_size());
      // check tcontext interface address
// TEXTDB:SELECT("      TEST(tc->"parameter".iimpl == interface_"module"());")FROM(C-kern/resource/config/initthread)WHERE(inittype=="interface")
      TEST(tc->pagecache.iimpl == interface_pagecacheimpl());
      TEST(tc->mm.iimpl == interface_mmimpl());
      TEST(tc->objectcache.iimpl == interface_objectcacheimpl());
      TEST(tc->log.iimpl == interface_logwriter());
// TEXTDB:END
      // check main type differences
      switch (contexttype[i]) {
      case maincontext_STATIC:
         break;
      case maincontext_DEFAULT:
         TEST(log_state_IGNORED  == tc->log.iimpl->getstate(tc->log.object, log_channel_USERERR));
         TEST(log_state_BUFFERED == tc->log.iimpl->getstate(tc->log.object, log_channel_ERR));
         break;
      case maincontext_CONSOLE:
         TEST(log_state_UNBUFFERED == tc->log.iimpl->getstate(tc->log.object, log_channel_USERERR));
         TEST(log_state_IGNORED    == tc->log.iimpl->getstate(tc->log.object, log_channel_ERR));
         break;
      }

      // TEST free_threadcontext: double free
      for (unsigned f=0; f<2; ++f) {
         TEST(0 == free_threadcontext(tc));
         TEST(1 == isstatic_threadcontext(tc));
      }
   }

   // TEST init_threadcontext: s_threadcontext_nextid == 0 ==> next value is 2
   for (size_t i = 0; i < lengthof(contexttype); ++i) {
      s_threadcontext_nextid = 0;
      TEST(0 == init_threadcontext(tc, contexttype[i]));
      // check tc
      TEST(2 == tc->thread_id);
      // check s_threadcontext_nextid
      TEST(3 == s_threadcontext_nextid);
      // reset
      TEST(0 == free_threadcontext(tc));
      TEST(1 == isstatic_threadcontext(tc));
   }

   // TEST init_threadcontext: EPROTO
   // prepare
   oldtype = g_maincontext.type;
   g_maincontext.type = maincontext_STATIC;
   // test
   TEST(EPROTO == init_threadcontext(tc, maincontext_DEFAULT));
   TEST(1 == isstatic_threadcontext(tc));
   // reset
   g_maincontext.type = oldtype;
   oldtype = maincontext_STATIC;

   // TEST init_threadcontext: EINVAL
   TEST(EINVAL == init_threadcontext(tc, maincontext_CONSOLE+1));
   TEST(1 == isstatic_threadcontext(tc));

   // TEST init_threadcontext: simulated ERROR
   s_threadcontext_nextid = 2;
   for(unsigned i = 1; i; ++i) {
      init_testerrortimer(&s_threadcontext_errtimer, i, (int)i);
      int err = init_threadcontext(tc, maincontext_DEFAULT);
      TEST(i+2 == s_threadcontext_nextid);
      if (!err) {
         free_testerrortimer(&s_threadcontext_errtimer);
         TEST(0 == free_threadcontext(tc));
         TEST(i > nrsvc);
      }
      TEST(1 == isstatic_threadcontext(tc));
      if (!err) break;
      TEST(i == (unsigned)err);
   }

   // TEST free_threadcontext: simulated ERROR
   s_threadcontext_nextid = 2;
   for(unsigned i = 1; i; ++i) {
      TEST(0 == init_threadcontext(tc, maincontext_DEFAULT));
      init_testerrortimer(&s_threadcontext_errtimer, i, (int)i);
      int err = free_threadcontext(tc);
      TEST(i+2 == s_threadcontext_nextid);
      TEST(1 == isstatic_threadcontext(tc));
      if (!err) {
         free_testerrortimer(&s_threadcontext_errtimer);
         TEST(i > nrsvc);
         break;
      }
      TEST(i == (unsigned)err);
   }

   // unprepare
   TEST(0 == freestatic_threadcontext(tc));
   TEST(0 == delete_threadstack(&st));

   return 0;
ONERR:
   if (oldtype != maincontext_STATIC) {
      g_maincontext.type = oldtype;
   }
   free_testerrortimer(&s_threadcontext_errtimer);
   delete_thread(&thread);
   free_threadcontext(tc);
   delete_threadstack(&st);
   return EINVAL;
}

static int test_query(void)
{
   threadcontext_t tcontext = threadcontext_FREE;
   static_data_t   sd;

   // TEST maincontext_threadcontext
   TEST( &tcontext.maincontext == &maincontext_threadcontext(&tcontext));

   // TEST isstatic_threadcontext: threadcontext_FREE;
   TEST(0 == isstatic_threadcontext(&tcontext));

   // TEST isstatic_threadcontext: after simulated initstatic
   tcontext.maincontext = &g_maincontext;
   tcontext.staticdata  = (void*)&sd;
   tcontext.log         = (ilog_t)iobj_INIT((void*)&sd, interface_logwriter());
   TEST(1 == isstatic_threadcontext(&tcontext));

   // TEST isstatic_threadcontext
   TEST(1 == isstatic_threadcontext(&tcontext));
   tcontext.maincontext = 0;
   TEST(0 == isstatic_threadcontext(&tcontext));
   tcontext.maincontext = &g_maincontext;
   tcontext.pagecache.object = (void*)1;
   TEST(0 == isstatic_threadcontext(&tcontext));
   tcontext.pagecache.object = 0;
   tcontext.mm.object = (void*)1;
   TEST(0 == isstatic_threadcontext(&tcontext));
   tcontext.mm.object = 0;
   tcontext.syncrunner = (void*)1;
   TEST(0 == isstatic_threadcontext(&tcontext));
   tcontext.syncrunner = 0;
   tcontext.objectcache.object = (void*)1;
   TEST(0 == isstatic_threadcontext(&tcontext));
   tcontext.objectcache.object = 0;
   tcontext.log.object = 0;
   TEST(0 == isstatic_threadcontext(&tcontext));
   tcontext.log.object = (void*)&sd;
   tcontext.log.iimpl = 0;
   TEST(0 == isstatic_threadcontext(&tcontext));
   tcontext.log.iimpl = interface_logwriter();
   tcontext.thread_id = 1; // does not matter !
   TEST(1 == isstatic_threadcontext(&tcontext));
   tcontext.thread_id = 0;
   tcontext.initcount = 1;
   TEST(0 == isstatic_threadcontext(&tcontext));
   tcontext.initcount = 0;
   tcontext.staticdata = 0;
   TEST(0 == isstatic_threadcontext(&tcontext));
   tcontext.staticdata = (uint8_t*)&sd;
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
   // TEST resetthreadid_threadcontext
   s_threadcontext_nextid = 10;
   TEST(0 != s_threadcontext_nextid);
   resetthreadid_threadcontext();
   TEST(0 == s_threadcontext_nextid);

   return 0;
ONERR:
   return EINVAL;
}

int unittest_task_threadcontext()
{
   int err;
   size_t oldid = s_threadcontext_nextid;

   if (test_lifehelper())  goto ONERR;
   if (execasprocess_unittest(&test_initfree_main, &err)) goto ONERR;
   if (err) goto ONERR;
   if (test_initfree_static())   goto ONERR;
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
