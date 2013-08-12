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
#include "C-kern/api/io/writer/log/logmain.h"
#include "C-kern/api/math/int/atomic.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/pagecache_macros.h"
#include "C-kern/api/memory/mm/mm.h"
#include "C-kern/api/platform/task/thread.h"
#include "C-kern/api/test/errortimer.h"
// TEXTDB:SELECT('#include "'header-name'"')FROM("C-kern/resource/config/initthread")
#include "C-kern/api/memory/pagecache_impl.h"
#include "C-kern/api/memory/mm/mm_impl.h"
#include "C-kern/api/task/syncrun.h"
#include "C-kern/api/cache/objectcache_impl.h"
#include "C-kern/api/io/writer/log/logwriter.h"
// TEXTDB:END
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


// section: threadcontext_t

// group: variables

#ifdef KONFIG_UNITTEST
/* variable: s_threadcontext_errtimer
 * Simulates an error in <init_threadcontext>. */
static test_errortimer_t   s_threadcontext_errtimer = test_errortimer_INIT_FREEABLE ;
#endif

/* variable: s_threadcontext_nextid
 * The next id which is assigned to <threadcontext_t.thread_id>. */
static size_t              s_threadcontext_nextid = 0 ;

// group: helper

/* define: INITIOBJ
 * Initializes iobj. Calls ALLOCSTATIC_PAGECACHE to allocate memory.
 * This memory is initialized with call to init_##module() and the address
 * is assigned to iobj.object. The return value of interface_##module()
 * is assigned to iobj.iimpl. */
#define INITIOBJ(module, objtype_t, iobj)                   \
         int err ;                                          \
         memblock_t memobject = memblock_INIT_FREEABLE ;    \
                                                            \
         if (! interface_##module()) {                      \
            /* keep static object */                        \
            return 0 ;                                      \
         }                                                  \
         ONERROR_testerrortimer(                            \
               &s_threadcontext_errtimer, ONABORT) ;        \
         err = ALLOCSTATIC_PAGECACHE(                       \
                  sizeof(objtype_t), &memobject) ;          \
         if (err) goto ONABORT ;                            \
                                                            \
         objtype_t * newobj ;                               \
         newobj = (objtype_t*) memobject.addr ;             \
                                                            \
         ONERROR_testerrortimer(                            \
               &s_threadcontext_errtimer, ONABORT) ;        \
         err = init_##module(newobj) ;                      \
         if (err) goto ONABORT ;                            \
                                                            \
         init_iobj(&(iobj),                                 \
               (void*)newobj, interface_##module()) ;       \
                                                            \
         return 0 ;                                         \
      ONABORT:                                              \
         FREESTATIC_PAGECACHE(&memobject) ;                 \
         return err ;

/* define: FREEIOBJ
 * Frees iobj. Calls FREESTATIC_PAGECACHE to free memory as last operation.
 * Calls free_##module() to free the object iobj.object.
 * The return value of interface_##module() is checked against iobj.iimpl.
 * iobj is cleared. Calling it twice has no effect. */
#define FREEIOBJ(module, objtype_t, iobj, staticiobj)       \
         int err ;                                          \
         int err2 ;                                         \
         objtype_t * delobj = (objtype_t*) (iobj).object ;  \
                                                            \
         if ((iobj).object != (staticiobj).object) {        \
            assert(  interface_##module()                   \
                     == (iobj).iimpl) ;                     \
                                                            \
            (iobj) = (staticiobj) ;                         \
                                                            \
            err = free_##module(delobj) ;                   \
                                                            \
            memblock_t memobject =                          \
                        memblock_INIT(                      \
                           sizeof(objtype_t),               \
                           (uint8_t*)delobj) ;              \
            err2 = FREESTATIC_PAGECACHE(&memobject) ;       \
            if (err2) err = err2 ;                          \
                                                            \
            return err ;                                    \
         }                                                  \
         return 0 ;

/* define: INITOBJECT
 * Initializes object. Calls ALLOCSTATIC_PAGECACHE to allocate memory.
 * This memory is initialized with call to init_##module() and the address
 * is assigned to object. */
#define INITOBJECT(module, objtype_t, object)               \
         int err ;                                          \
         memblock_t memobject = memblock_INIT_FREEABLE ;    \
                                                            \
         ONERROR_testerrortimer(                            \
               &s_threadcontext_errtimer, ONABORT) ;        \
         err = ALLOCSTATIC_PAGECACHE(                       \
                  sizeof(objtype_t), &memobject) ;          \
         if (err) goto ONABORT ;                            \
                                                            \
         objtype_t * newobj ;                               \
         newobj = (objtype_t*) memobject.addr ;             \
                                                            \
         ONERROR_testerrortimer(                            \
               &s_threadcontext_errtimer, ONABORT) ;        \
         err = init_##module(newobj) ;                      \
         if (err) goto ONABORT ;                            \
                                                            \
         (object) = newobj ;                                \
                                                            \
         return 0 ;                                         \
      ONABORT:                                              \
         FREESTATIC_PAGECACHE(&memobject) ;                 \
         return err ;

/* define: FREEOBJECT
 * Frees object. Calls FREESTATIC_PAGECACHE to free memory as last operation.
 * Calls free_##module() to free object and object is cleared.
 * Calling it twice has no effect. */
#define FREEOBJECT(module, objtype_t, object, staticobj)    \
         int err ;                                          \
         int err2 ;                                         \
         objtype_t * delobj = (object) ;                    \
                                                            \
         if (delobj != (staticobj)) {                       \
                                                            \
            (object) = (staticobj) ;                        \
                                                            \
            err = free_##module(delobj) ;                   \
                                                            \
            memblock_t memobject =                          \
                        memblock_INIT(                      \
                           sizeof(objtype_t),               \
                           (uint8_t*)delobj) ;              \
            err2 = FREESTATIC_PAGECACHE(&memobject) ;       \
            if (err2) err = err2 ;                          \
                                                            \
            return err ;                                    \
         }                                                  \
         return 0 ;

/* about: freehelperX_threadcontext
 * o Generated freehelper functions to free modules with calls to freethread_module.
 * o Generated freehelper functions to free interfaceable objects with calls free_module.
 * o Generated freehelper functions to free objects with calls free_module.
 * */

// TEXTDB:SELECT("static int freehelper"row-id"_threadcontext(threadcontext_t * tcontext, threadcontext_t * statictcontext)"\n"{"\n"   FREEOBJECT("module", "objtype", tcontext->"parameter", statictcontext->"parameter") ;"\n"}"\n)FROM(C-kern/resource/config/initthread)WHERE(inittype=="object")
static int freehelper3_threadcontext(threadcontext_t * tcontext, threadcontext_t * statictcontext)
{
   FREEOBJECT(syncrun, syncrun_t, tcontext->syncrun, statictcontext->syncrun) ;
}

// TEXTDB:END

// TEXTDB:SELECT("static int freehelper"row-id"_threadcontext(threadcontext_t * tcontext, threadcontext_t * statictcontext)"\n"{"\n"   FREEIOBJ("module", "objtype", tcontext->"parameter", statictcontext->"parameter") ;"\n"}"\n)FROM(C-kern/resource/config/initthread)WHERE(inittype=="interface")
static int freehelper2_threadcontext(threadcontext_t * tcontext, threadcontext_t * statictcontext)
{
   FREEIOBJ(mmimpl, mm_impl_t, tcontext->mm, statictcontext->mm) ;
}

static int freehelper4_threadcontext(threadcontext_t * tcontext, threadcontext_t * statictcontext)
{
   FREEIOBJ(objectcacheimpl, objectcache_impl_t, tcontext->objectcache, statictcontext->objectcache) ;
}

static int freehelper5_threadcontext(threadcontext_t * tcontext, threadcontext_t * statictcontext)
{
   FREEIOBJ(logwriter, logwriter_t, tcontext->log, statictcontext->log) ;
}

// TEXTDB:END

// TEXTDB:SELECT("static int freehelper"row-id"_threadcontext(threadcontext_t * tcontext, threadcontext_t * statictcontext)"\n"{"\n"   (void) tcontext;"\n"   (void) statictcontext;"\n"   return freethread_"module"("(if (parameter!="") "genericcast_iobj(&tcontext->" else "(")parameter(if (parameter!="") ", " else "")parameter")) ;"\n"}"\n)FROM(C-kern/resource/config/initthread)WHERE(inittype=="initthread")
static int freehelper1_threadcontext(threadcontext_t * tcontext, threadcontext_t * statictcontext)
{
   (void) tcontext;
   (void) statictcontext;
   return freethread_pagecacheimpl(genericcast_iobj(&tcontext->pagecache, pagecache)) ;
}

// TEXTDB:END

/* about: inithelperX_threadcontext
 * o Generated inithelper functions to init modules with calls to initthread_module.
 * o Generated inithelper functions to init interfaceable objects with calls init_module.
 * o Generated inithelper functions to init objects with calls init_module.
 * */

// TEXTDB:SELECT("static int inithelper"row-id"_threadcontext(threadcontext_t * tcontext)"\n"{"\n"   INITOBJECT("module", "objtype", tcontext->"parameter") ;"\n"}"\n)FROM(C-kern/resource/config/initthread)WHERE(inittype=="object")
static int inithelper3_threadcontext(threadcontext_t * tcontext)
{
   INITOBJECT(syncrun, syncrun_t, tcontext->syncrun) ;
}

// TEXTDB:END

// TEXTDB:SELECT("static int inithelper"row-id"_threadcontext(threadcontext_t * tcontext)"\n"{"\n"   INITIOBJ("module", "objtype", tcontext->"parameter") ;"\n"}"\n)FROM(C-kern/resource/config/initthread)WHERE(inittype=="interface")
static int inithelper2_threadcontext(threadcontext_t * tcontext)
{
   INITIOBJ(mmimpl, mm_impl_t, tcontext->mm) ;
}

static int inithelper4_threadcontext(threadcontext_t * tcontext)
{
   INITIOBJ(objectcacheimpl, objectcache_impl_t, tcontext->objectcache) ;
}

static int inithelper5_threadcontext(threadcontext_t * tcontext)
{
   INITIOBJ(logwriter, logwriter_t, tcontext->log) ;
}

// TEXTDB:END

// TEXTDB:SELECT("static int inithelper"row-id"_threadcontext(threadcontext_t * tcontext)"\n"{"\n"   (void) tcontext;"\n"   return initthread_"module"("(if (parameter!="") "genericcast_iobj(&tcontext->" else "(")parameter(if (parameter!="") ", " else "")parameter")) ;"\n"}"\n)FROM(C-kern/resource/config/initthread)WHERE(inittype=="initthread")
static int inithelper1_threadcontext(threadcontext_t * tcontext)
{
   (void) tcontext;
   return initthread_pagecacheimpl(genericcast_iobj(&tcontext->pagecache, pagecache)) ;
}

// TEXTDB:END

// group: configruation

/* function: config_threadcontext
 * Adapts tcontext to chosen <maincontext_e> type. */
static int config_threadcontext(threadcontext_t * tcontext, maincontext_e context_type)
{
   log_t * ilog ;

   switch (context_type) {
   case maincontext_STATIC:
      return 0 ;
   case maincontext_DEFAULT:
      return 0;
   case maincontext_CONSOLE:
      ilog = genericcast_log(&tcontext->log) ;
      ilog->iimpl->setstate(ilog->object, log_channel_USERERR, log_state_UNBUFFERED) ;
      ilog->iimpl->setstate(ilog->object, log_channel_ERR, log_state_IGNORED) ;
      return 0 ;
   }

   return EINVAL ;
}

void flushlog_threadcontext(threadcontext_t * tcontext)
{
   tcontext->log.iimpl->flushbuffer(tcontext->log.object, log_channel_ERR) ;
}

// group: lifetime

int free_threadcontext(threadcontext_t * tcontext)
{
   int err = 0 ;
   int err2 ;
   threadcontext_t statictcontext = threadcontext_INIT_STATIC ;

   // TODO: add flush caches (log + database caches) to free_threadcontext
   //       another (better?) option is to add flush caches to end of a single
   //       transaction (or end of group transaction) and commit the default transaction

   flushlog_threadcontext(tcontext) ;

   size_t    initcount = tcontext->initcount ;
   tcontext->initcount = 0 ;

   switch (initcount) {
   default:    assert(false && "initcount out of bounds") ;
               break ;
// TEXTDB:SELECT("   case "row-id":     err2 = freehelper"row-id"_threadcontext(tcontext, &statictcontext) ;"\n"               if (err2) err = err2 ;")FROM(C-kern/resource/config/initthread)DESCENDING
   case 5:     err2 = freehelper5_threadcontext(tcontext, &statictcontext) ;
               if (err2) err = err2 ;
   case 4:     err2 = freehelper4_threadcontext(tcontext, &statictcontext) ;
               if (err2) err = err2 ;
   case 3:     err2 = freehelper3_threadcontext(tcontext, &statictcontext) ;
               if (err2) err = err2 ;
   case 2:     err2 = freehelper2_threadcontext(tcontext, &statictcontext) ;
               if (err2) err = err2 ;
   case 1:     err2 = freehelper1_threadcontext(tcontext, &statictcontext) ;
               if (err2) err = err2 ;
// TEXTDB:END
   case 0:     break ;
   }

   if (1 == tcontext->thread_id) {
      // end main thread => reset s_threadcontext_nextid
      s_threadcontext_nextid = 0 ;
   }

   tcontext->pcontext  = statictcontext.pcontext ;
   tcontext->thread_id = statictcontext.thread_id ;

   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORTFREE_ERRLOG(err) ;
   return err ;
}

int init_threadcontext(/*out*/threadcontext_t * tcontext, processcontext_t * pcontext, uint8_t context_type)
{
   int err ;

   *(volatile threadcontext_t *)tcontext = (threadcontext_t) threadcontext_INIT_STATIC ;
   ((volatile threadcontext_t *)tcontext)->pcontext = pcontext ;

   VALIDATE_STATE_TEST(maincontext_STATIC != type_maincontext(), ONABORT_NOFREE, ) ;
   ONERROR_testerrortimer(&s_threadcontext_errtimer, ONABORT_NOFREE) ;

   do {
      tcontext->thread_id = 1 + atomicadd_int(&s_threadcontext_nextid, 1) ;
   } while (tcontext->thread_id <= 1 && !ismain_thread(self_thread())) ;  // if wrapped around ? => repeat

// TEXTDB:SELECT(\n"   ONERROR_testerrortimer(&s_threadcontext_errtimer, ONABORT) ;"\n"   err = inithelper"row-id"_threadcontext(tcontext) ;"\n"   if (err) goto ONABORT ;"\n"   ++tcontext->initcount ;")FROM(C-kern/resource/config/initthread)

   ONERROR_testerrortimer(&s_threadcontext_errtimer, ONABORT) ;
   err = inithelper1_threadcontext(tcontext) ;
   if (err) goto ONABORT ;
   ++tcontext->initcount ;

   ONERROR_testerrortimer(&s_threadcontext_errtimer, ONABORT) ;
   err = inithelper2_threadcontext(tcontext) ;
   if (err) goto ONABORT ;
   ++tcontext->initcount ;

   ONERROR_testerrortimer(&s_threadcontext_errtimer, ONABORT) ;
   err = inithelper3_threadcontext(tcontext) ;
   if (err) goto ONABORT ;
   ++tcontext->initcount ;

   ONERROR_testerrortimer(&s_threadcontext_errtimer, ONABORT) ;
   err = inithelper4_threadcontext(tcontext) ;
   if (err) goto ONABORT ;
   ++tcontext->initcount ;

   ONERROR_testerrortimer(&s_threadcontext_errtimer, ONABORT) ;
   err = inithelper5_threadcontext(tcontext) ;
   if (err) goto ONABORT ;
   ++tcontext->initcount ;
// TEXTDB:END

   err = config_threadcontext(tcontext, context_type) ;
   if (err) goto ONABORT ;

   ONERROR_testerrortimer(&s_threadcontext_errtimer, ONABORT) ;

   return 0 ;
ONABORT:
   (void) free_threadcontext(tcontext) ;
ONABORT_NOFREE:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

// group: query

bool isstatic_threadcontext(const threadcontext_t * tcontext)
{
   return   &g_maincontext.pcontext == tcontext->pcontext
            && 0 == tcontext->pagecache.object   && 0 == tcontext->pagecache.iimpl
            && 0 == tcontext->mm.object          && 0 == tcontext->mm.iimpl
            && 0 == tcontext->syncrun
            && 0 == tcontext->objectcache.object && 0 == tcontext->objectcache.iimpl
            && 0 == tcontext->log.object && &g_logmain_interface == tcontext->log.iimpl
            && 0 == tcontext->thread_id
            && 0 == tcontext->initcount ;
}

// group: change

void resetthreadid_threadcontext()
{
   atomicwrite_int(&s_threadcontext_nextid, 0) ;
}

void setmm_threadcontext(threadcontext_t * tcontext, mm_t * new_mm)
{
   initcopy_iobj(&tcontext->mm, new_mm) ;
}


// group: test

#ifdef KONFIG_UNITTEST

typedef struct testmodule_impl_t    testmodule_impl_t ;

struct testmodule_impl_t {
   double   dummy[4] ;
} ;

iobj_DECLARE(testmodule_t, testmodule) ;

static struct testmodule_impl_t *  s_test_testmodule = 0 ;

static int init_testmoduleimpl(struct testmodule_impl_t * object)
{
   s_test_testmodule = object ;
   return 0 ;
}

static int free_testmoduleimpl(struct testmodule_impl_t * object)
{
   s_test_testmodule = object ;
   return 0 ;
}

static struct testmodule_it * interface_testmoduleimpl(void)
{
   return (struct testmodule_it*) 33 ;
}

static int call_initiobj(struct testmodule_t * testiobj)
{
   INITIOBJ(testmoduleimpl, struct testmodule_impl_t, *testiobj)
}

static int call_freeiobj(struct testmodule_t * testiobj)
{
   struct testmodule_t staticiobj = { (void*)1, (void*)2 } ;
   FREEIOBJ(testmoduleimpl, struct testmodule_impl_t, *testiobj, staticiobj)
}

#define init_testmoduleimpl2(testiobj)    (ENOSYS) /* not called */
#define free_testmoduleimpl2(testiobj)    (ENOSYS) /* not called */
#define interface_testmoduleimpl2()       (0)      /* INITIOBJ now checks for value 0 */

static int call_initiobj2(struct testmodule_t * testiobj)
{
   INITIOBJ(testmoduleimpl2, struct testmodule_impl_t, *testiobj)
}

static int call_freeiobj2(struct testmodule_t * testiobj)
{
   FREEIOBJ(testmoduleimpl2, struct testmodule_impl_t, *testiobj, *testiobj)
}

static int test_iobjhelper(void)
{
   struct testmodule_t  testiobj ;
   memblock_t           memblock = memblock_INIT_FREEABLE ;

   // TEST INITIOBJ
   size_t stsize = SIZESTATIC_PAGECACHE() ;
   s_test_testmodule = 0 ;
   free_iobj(&testiobj) ;
   TEST(0 == call_initiobj(&testiobj)) ;
   TEST(testiobj.object != 0) ;
   TEST(testiobj.iimpl  == (struct testmodule_it*)33) ;
   TEST(testiobj.object == (struct testmodule_t*)s_test_testmodule) ;
   TEST(SIZESTATIC_PAGECACHE() == stsize + sizeof(struct testmodule_impl_t)) ;

   // TEST FREEIOBJ
   void * const oldobject = s_test_testmodule ;
   s_test_testmodule = 0 ;
   TEST(0 == call_freeiobj(&testiobj)) ;
   TEST(testiobj.object == (void*)1) ;
   TEST(testiobj.iimpl  == (void*)2) ;
   TEST(oldobject       == s_test_testmodule) ;
   TEST(SIZESTATIC_PAGECACHE() == stsize) ;
   TEST(!ALLOCSTATIC_PAGECACHE(sizeof(struct testmodule_impl_t), &memblock)) ;
   TEST(oldobject       == addr_memblock(&memblock)) ;
   TEST(!FREESTATIC_PAGECACHE(&memblock)) ;
   s_test_testmodule = 0 ;
   TEST(0 == call_freeiobj(&testiobj)) ;
   TEST(testiobj.object == (void*)1) ;
   TEST(testiobj.iimpl  == (void*)2) ;
   TEST(0               == s_test_testmodule) ;
   TEST(SIZESTATIC_PAGECACHE() == stsize) ;

   // TEST INITIOBJ: keep static object (interface_testmoduleimpl2 returns 0)
   TEST(testiobj.object == (void*)1) ;
   TEST(testiobj.iimpl  == (void*)2) ;
   TEST(0 == call_initiobj2(&testiobj)) ;
   TEST(testiobj.object == (void*)1) ;
   TEST(testiobj.iimpl  == (void*)2) ;
   TEST(SIZESTATIC_PAGECACHE() == stsize) ;
   TEST(0 == call_freeiobj2(&testiobj)) ;
   TEST(testiobj.object == (void*)1) ;
   TEST(testiobj.iimpl  == (void*)2) ;
   TEST(SIZESTATIC_PAGECACHE() == stsize) ;

   // TEST INITIOBJ: ENOMEM
   testiobj.object = 0 ;
   testiobj.iimpl  = 0 ;
   for (unsigned i = 1; i <= 2; ++i) {
      init_testerrortimer(&s_threadcontext_errtimer, i, ENOMEM) ;
      TEST(ENOMEM == call_initiobj(&testiobj)) ;
      TEST(testiobj.object == 0) ;
      TEST(testiobj.iimpl  == 0) ;
      TEST(SIZESTATIC_PAGECACHE() == stsize) ;
   }

   return 0 ;
ONABORT:
   FREESTATIC_PAGECACHE(&memblock) ;
   return EINVAL ;
}

static int call_initobject(struct testmodule_impl_t ** testobj)
{
   INITOBJECT(testmoduleimpl, struct testmodule_impl_t, *testobj)
}

static int call_freeobject(struct testmodule_impl_t ** testobj)
{
   struct testmodule_impl_t * staticobj = (void*)3 ;
   FREEOBJECT(testmoduleimpl, struct testmodule_impl_t, *testobj, staticobj)
}

static int test_objhelper(void)
{
   testmodule_impl_t *  testobj  = 0 ;
   memblock_t           memblock = memblock_INIT_FREEABLE ;

   // TEST INITOBJECT
   size_t stsize = SIZESTATIC_PAGECACHE() ;
   s_test_testmodule = 0 ;
   TEST(0 == call_initobject(&testobj)) ;
   TEST(testobj != 0) ;
   TEST(testobj == s_test_testmodule) ;
   TEST(SIZESTATIC_PAGECACHE() == stsize + sizeof(struct testmodule_impl_t)) ;

   // TEST FREEOBJECT
   void * const oldobj = s_test_testmodule ;
   s_test_testmodule = 0 ;
   TEST(0 == call_freeobject(&testobj)) ;
   TEST(testobj == (void*)3) ;
   TEST(oldobj  == s_test_testmodule) ;
   TEST(SIZESTATIC_PAGECACHE() == stsize) ;
   TEST(!ALLOCSTATIC_PAGECACHE(sizeof(struct testmodule_impl_t), &memblock)) ;
   TEST(oldobj  == addr_memblock(&memblock)) ;
   TEST(!FREESTATIC_PAGECACHE(&memblock)) ;
   s_test_testmodule = 0 ;
   TEST(0 == call_freeobject(&testobj)) ;
   TEST(testobj == (void*)3) ;
   TEST(0       == s_test_testmodule) ;
   TEST(SIZESTATIC_PAGECACHE() == stsize) ;

   // TEST INITOBJECT: ENOMEM
   testobj = 0 ;
   for (unsigned i = 1; i <= 2; ++i) {
      init_testerrortimer(&s_threadcontext_errtimer, i, ENOMEM) ;
      TEST(ENOMEM == call_initobject(&testobj)) ;
      TEST(testobj == 0) ;
      TEST(SIZESTATIC_PAGECACHE() == stsize) ;
   }

   return 0 ;
ONABORT:
   FREESTATIC_PAGECACHE(&memblock) ;
   return EINVAL ;
}

static int thread_testwraparound(void * dummy)
{
   (void) dummy ;

   TEST(2 == tcontext_maincontext()->thread_id) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_initfree(void)
{
   threadcontext_t      tcontext = threadcontext_INIT_FREEABLE ;
   thread_t *           thread   = 0 ;
   processcontext_t *   p        = pcontext_maincontext() ;
   const size_t         nrsvc    = 5 ;
   size_t               sizestatic ;

   // prepare
   TEST(p != 0) ;

   // TEST threadcontext_INIT_FREEABLE
   TEST(0 == tcontext.pcontext) ;
   TEST(0 == tcontext.pagecache.object) ;
   TEST(0 == tcontext.pagecache.iimpl) ;
   TEST(0 == tcontext.mm.object) ;
   TEST(0 == tcontext.mm.iimpl) ;
   TEST(0 == tcontext.syncrun) ;
   TEST(0 == tcontext.objectcache.object) ;
   TEST(0 == tcontext.objectcache.iimpl) ;
   TEST(0 == tcontext.log.object) ;
   TEST(0 == tcontext.log.iimpl) ;
   TEST(0 == tcontext.thread_id) ;
   TEST(0 == tcontext.initcount) ;

   // TEST threadcontext_INIT_STATIC
   tcontext = (threadcontext_t) threadcontext_INIT_STATIC ;
   TEST(1 == isstatic_threadcontext(&tcontext)) ;

   // TEST init_threadcontext
   maincontext_e contexttype[] = { maincontext_DEFAULT, maincontext_CONSOLE } ;
   for (unsigned i = 0; i < lengthof(contexttype); ++i) {
      sizestatic = SIZESTATIC_PAGECACHE() ;
      TEST(0 == init_threadcontext(&tcontext, p, contexttype[i])) ;
      TEST(p == tcontext.pcontext) ;
      TEST(0 != tcontext.pagecache.object) ;
      TEST(0 != tcontext.pagecache.iimpl) ;
      TEST(0 != tcontext.mm.object) ;
      TEST(0 != tcontext.mm.iimpl) ;
      TEST(0 != tcontext.syncrun) ;
      TEST(0 != tcontext.objectcache.object) ;
      TEST(0 != tcontext.objectcache.iimpl) ;
      TEST(0 != tcontext.log.object) ;
      TEST(0 != tcontext.log.iimpl) ;
      TEST(0 != tcontext.log.object) ;
      TEST(&g_logmain_interface != tcontext.log.iimpl) ;
      TEST(0 != tcontext.thread_id) ;
      TEST(nrsvc == tcontext.initcount) ;
      TEST(SIZESTATIC_PAGECACHE() > sizestatic) ;
      switch (contexttype[i]) {
      case maincontext_STATIC:
         break ;
      case maincontext_DEFAULT:
         TEST(log_state_IGNORED  == tcontext.log.iimpl->getstate(tcontext.log.object, log_channel_USERERR)) ;
         TEST(log_state_BUFFERED == tcontext.log.iimpl->getstate(tcontext.log.object, log_channel_ERR)) ;
         break ;
      case maincontext_CONSOLE:
         TEST(log_state_UNBUFFERED == tcontext.log.iimpl->getstate(tcontext.log.object, log_channel_USERERR)) ;
         TEST(log_state_IGNORED    == tcontext.log.iimpl->getstate(tcontext.log.object, log_channel_ERR)) ;
         break ;
      }

      // TEST free_threadcontext
      TEST(0 == free_threadcontext(&tcontext)) ;
      TEST(1 == isstatic_threadcontext(&tcontext)) ;
      TEST(SIZESTATIC_PAGECACHE() == sizestatic) ;
      TEST(0 == free_threadcontext(&tcontext)) ;
      TEST(1 == isstatic_threadcontext(&tcontext)) ;
      TEST(SIZESTATIC_PAGECACHE() == sizestatic) ;
   }

   // TEST init_threadcontext: s_threadcontext_nextid incremented
   for (unsigned i = 0; i < 10; ++i) {
      size_t s = 1 + s_threadcontext_nextid ;
      TEST(0 == init_threadcontext(&tcontext, p, maincontext_DEFAULT)) ;
      TEST(s == tcontext.thread_id) ;
      TEST(s == s_threadcontext_nextid) ;
      TEST(0 == free_threadcontext(&tcontext)) ;
      TEST(0 == tcontext.thread_id) ;
   }

   // TEST init_threadcontext: main thread => no wrap around
   s_threadcontext_nextid = SIZE_MAX ;
   TEST(0 == init_threadcontext(&tcontext, p, maincontext_DEFAULT)) ;
   TEST(0 == tcontext.thread_id) ;
   TEST(0 == s_threadcontext_nextid) ;
   TEST(0 == free_threadcontext(&tcontext)) ;

   // TEST init_threadcontext: thread => wrap around to value 2
   s_threadcontext_nextid = SIZE_MAX ;
   TEST(0 == new_thread(&thread, thread_testwraparound, (void*)0)) ;
   TEST(0 == join_thread(thread)) ;
   TEST(0 == returncode_thread(thread)) ;
   TEST(0 == delete_thread(&thread)) ;

   // TEST free_threadcontext: reset s_threadcontext_nextid in case of main thread
   s_threadcontext_nextid = 3 ;
   TEST(0 == init_threadcontext(&tcontext, p, maincontext_DEFAULT)) ;
   TEST(4 == tcontext.thread_id) ;
   TEST(4 == s_threadcontext_nextid) ;
   tcontext.thread_id = 1 ; // simulate main thread
   TEST(0 == free_threadcontext(&tcontext)) ;
   TEST(0 == tcontext.thread_id) ;
   TEST(0 == s_threadcontext_nextid) ; // reset

   // TEST init_threadcontext: EPROTO
   maincontext_e oldtype = type_maincontext() ;
   g_maincontext.type = maincontext_STATIC ;
   TEST(EPROTO == init_threadcontext(&tcontext, p, maincontext_DEFAULT)) ;
   g_maincontext.type = oldtype ;

   // TEST init_threadcontext: EINVAL
   TEST(EINVAL == init_threadcontext(&tcontext, p, maincontext_CONSOLE+1)) ;

   // TEST init_threadcontext: ERROR
   s_threadcontext_nextid = 0 ;
   for(unsigned i = 1; i; ++i) {
      init_testerrortimer(&s_threadcontext_errtimer, i, (int)i) ;
      memset(&tcontext, 0xff, sizeof(tcontext)) ;
      int err = init_threadcontext(&tcontext, p, maincontext_DEFAULT) ;
      if (err == 0) {
         TEST(1 == s_threadcontext_nextid) ;
         TEST(0 == free_threadcontext(&tcontext)) ;
         TEST(0 == s_threadcontext_nextid) ;    // reset
         TEST(i > nrsvc) ;
         break ;
      }
      TEST(0 == s_threadcontext_nextid) ; // not changed
      TEST(i == (unsigned)err) ;
      TEST(1 == isstatic_threadcontext(&tcontext)) ;
      TEST(SIZESTATIC_PAGECACHE() == sizestatic) ;
   }

   // unprepare
   s_threadcontext_errtimer = (test_errortimer_t) test_errortimer_INIT_FREEABLE ;

   return 0 ;
ONABORT:
   s_threadcontext_errtimer = (test_errortimer_t) test_errortimer_INIT_FREEABLE ;
   delete_thread(&thread) ;
   return EINVAL ;
}

static int test_query(void)
{
   threadcontext_t tcontext = threadcontext_INIT_STATIC ;

   // TEST isstatic_threadcontext
   TEST(1 == isstatic_threadcontext(&tcontext)) ;
   tcontext.pcontext = 0 ;
   TEST(0 == isstatic_threadcontext(&tcontext)) ;
   tcontext.pcontext = &g_maincontext.pcontext ;
   tcontext.pagecache.object = (void*)1 ;
   TEST(0 == isstatic_threadcontext(&tcontext)) ;
   tcontext.pagecache.object = 0 ;
   tcontext.pagecache.iimpl = (void*)1 ;
   TEST(0 == isstatic_threadcontext(&tcontext)) ;
   tcontext.pagecache.iimpl = 0 ;
   tcontext.mm.object = (void*)1 ;
   TEST(0 == isstatic_threadcontext(&tcontext)) ;
   tcontext.mm.object = 0 ;
   tcontext.mm.iimpl = (void*)1 ;
   TEST(0 == isstatic_threadcontext(&tcontext)) ;
   tcontext.mm.iimpl = 0 ;
   tcontext.syncrun = (void*)1 ;
   TEST(0 == isstatic_threadcontext(&tcontext)) ;
   tcontext.syncrun = 0 ;
   tcontext.objectcache.object = (void*)1 ;
   TEST(0 == isstatic_threadcontext(&tcontext)) ;
   tcontext.objectcache.object = 0 ;
   tcontext.objectcache.iimpl = (void*)1 ;
   TEST(0 == isstatic_threadcontext(&tcontext)) ;
   tcontext.objectcache.iimpl = 0 ;
   tcontext.log.object = (struct log_t*)1 ;
   TEST(0 == isstatic_threadcontext(&tcontext)) ;
   tcontext.log.object = 0 ;
   tcontext.log.iimpl = 0 ;
   TEST(0 == isstatic_threadcontext(&tcontext)) ;
   tcontext.log.iimpl = &g_logmain_interface ;
   tcontext.thread_id = 1 ;
   TEST(0 == isstatic_threadcontext(&tcontext)) ;
   tcontext.thread_id = 0 ;
   tcontext.initcount = 1 ;
   TEST(0 == isstatic_threadcontext(&tcontext)) ;
   tcontext.initcount = 0 ;
   TEST(1 == isstatic_threadcontext(&tcontext)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_change(void)
{
   threadcontext_t tcontext = threadcontext_INIT_FREEABLE ;

   // TEST resetthreadid_threadcontext
   s_threadcontext_nextid = 10 ;
   TEST(0 != s_threadcontext_nextid) ;
   resetthreadid_threadcontext() ;
   TEST(0 == s_threadcontext_nextid) ;

   // TEST setmm_threadcontext
   setmm_threadcontext(&tcontext, &(mm_t) iobj_INIT((void*)1, (void*)2)) ;
   TEST(tcontext.mm.object == (mm_t*)1) ;
   TEST(tcontext.mm.iimpl  == (mm_it*)2) ;
   setmm_threadcontext(&tcontext, &(mm_t) iobj_INIT(0, 0)) ;
   TEST(tcontext.mm.object == 0) ;
   TEST(tcontext.mm.iimpl  == 0) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

int unittest_context_threadcontext()
{
   size_t            oldid = s_threadcontext_nextid ;
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_iobjhelper())  goto ONABORT ;
   if (test_objhelper())   goto ONABORT ;
   if (test_initfree())    goto ONABORT ;
   if (test_query())       goto ONABORT ;
   if (test_change())      goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   s_threadcontext_nextid = oldid ;
   return 0 ;
ONABORT:
   s_threadcontext_nextid = oldid ;
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
